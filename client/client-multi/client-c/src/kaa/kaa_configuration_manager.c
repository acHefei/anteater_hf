/**
 *  Copyright 2014-2016 CyberVision, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef KAA_DISABLE_FEATURE_CONFIGURATION


#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <sys/types.h>
#include "platform/stdio.h"
#include "platform/sock.h"
#include "kaa_configuration_manager.h"

#include "platform/ext_sha.h"
#include "platform/ext_configuration_persistence.h"
#include "collections/kaa_list.h"
#include "kaa_common.h"
#include "kaa_status.h"
#include "kaa_defaults.h"
#include "kaa_platform_utils.h"
#include "kaa_platform_common.h"
#include "kaa_channel_manager.h"
#include "utilities/kaa_mem.h"
#include "utilities/kaa_log.h"
#include "avro_src/avro/io.h"

#define KAA_CONFIGURATION_RECEIVE_UPDATES_FLAG   0x01
#define KAA_CONFIGURATION_HASH_FLAG              0x02
#define KAA_CONFIGURATION_FULL_RESYNC_FLAG       0x04
#define KAA_CONFIGURATION_ALL_FLAGS              (KAA_CONFIGURATION_RECEIVE_UPDATES_FLAG | KAA_CONFIGURATION_HASH_FLAG | KAA_CONFIGURATION_FULL_RESYNC_FLAG)


#define KAA_CONFIGURATION_BODY_PRESENT           0x02

extern kaa_transport_channel_interface_t *kaa_channel_manager_get_transport_channel(kaa_channel_manager_t *self, kaa_service_t service_type);

static kaa_service_t configuration_sync_services[1] = { KAA_SERVICE_CONFIGURATION };

struct kaa_configuration_manager {
    kaa_digest                           configuration_hash;
    kaa_configuration_root_receiver_t    root_receiver;
    kaa_root_configuration_t            *root_record;
    kaa_channel_manager_t               *channel_manager;
    kaa_status_t                        *status;
    kaa_logger_t                        *logger;
};


static kaa_root_configuration_t *kaa_configuration_manager_deserialize(const char *buffer, size_t buffer_size)
{
    KAA_RETURN_IF_NIL2(buffer, buffer_size, NULL);

    avro_reader_t reader = avro_reader_memory(buffer, buffer_size);
    KAA_RETURN_IF_NIL(reader, NULL);
    kaa_root_configuration_t *result = KAA_CONFIGURATION_DESERIALIZE(reader);
    avro_reader_free(reader);
    return result;
}


kaa_error_t kaa_configuration_manager_create(kaa_configuration_manager_t **configuration_manager_p, kaa_channel_manager_t *channel_manager, kaa_status_t *status, kaa_logger_t *logger)
{
    KAA_RETURN_IF_NIL3(configuration_manager_p, status, logger, KAA_ERR_BADPARAM);
    kaa_configuration_manager_t *manager = (kaa_configuration_manager_t *) KAA_MALLOC(sizeof(kaa_configuration_manager_t));
    KAA_RETURN_IF_NIL(manager, KAA_ERR_NOMEM);

    manager->channel_manager = channel_manager;
    manager->status = status;
    manager->logger = logger;
    manager->root_receiver = (kaa_configuration_root_receiver_t) { NULL, NULL };

    char *buffer = NULL;
    size_t buffer_size = 0;
    bool need_deallocation = false;
    if (status->is_updated)
        ext_configuration_read(&buffer, &buffer_size, &need_deallocation);
    else
        ext_configuration_delete();
    if (!buffer || !buffer_size) {
        need_deallocation = false;
#if KAA_CONFIGURATION_DATA_LENGTH > 0
        buffer = (char *)KAA_CONFIGURATION_DATA;
        buffer_size = KAA_CONFIGURATION_DATA_LENGTH;
#endif
    }

    if (buffer && buffer_size > 0) {
        ext_calculate_sha_hash(buffer, buffer_size, manager->configuration_hash);
        manager->root_record = kaa_configuration_manager_deserialize(buffer, buffer_size);

        if (!manager->root_record) {
            KAA_FREE(manager);
            if (need_deallocation && buffer)
                KAA_FREE(buffer);
            return KAA_ERR_NOMEM;
        }

        if (need_deallocation)
            KAA_FREE(buffer);
    }

    *configuration_manager_p = manager;
    return KAA_ERR_NONE;
}



void kaa_configuration_manager_destroy(kaa_configuration_manager_t *self)
{
    if (self) {
        if (self->root_record)
            self->root_record->destroy(self->root_record);
        KAA_FREE(self);
    }
}



kaa_error_t kaa_configuration_manager_get_size(kaa_configuration_manager_t *self, size_t *expected_size)
{
    KAA_RETURN_IF_NIL2(self, expected_size, KAA_ERR_BADPARAM);

    *expected_size = KAA_EXTENSION_HEADER_SIZE + SHA_1_DIGEST_LENGTH;

    return KAA_ERR_NONE;
}



kaa_error_t kaa_configuration_manager_request_serialize(kaa_configuration_manager_t *self, kaa_platform_message_writer_t *writer)
{
    KAA_RETURN_IF_NIL2(self, writer, KAA_ERR_BADPARAM);

    KAA_LOG_TRACE(self->logger, KAA_ERR_NONE, "Going to serialize client configuration sync");

    uint32_t payload_size = sizeof(uint32_t) + SHA_1_DIGEST_LENGTH;

    kaa_platform_message_writer_t tmp_writer = *writer;
    kaa_error_t error_code = kaa_platform_message_write_extension_header(&tmp_writer, KAA_CONFIGURATION_EXTENSION_TYPE, KAA_CONFIGURATION_ALL_FLAGS, payload_size);
    if (error_code) {
        KAA_LOG_ERROR(self->logger, error_code, "Failed to write configuration extension header");
        return KAA_ERR_WRITE_FAILED;
    }

    error_code = kaa_platform_message_write_aligned(&tmp_writer, self->configuration_hash, SHA_1_DIGEST_LENGTH);
    if (error_code) {
        KAA_LOG_ERROR(self->logger, error_code, "Failed to write configuration hash");
        return KAA_ERR_WRITE_FAILED;
    }

    *writer = tmp_writer;

    return KAA_ERR_NONE;
}



kaa_error_t kaa_configuration_manager_handle_server_sync(kaa_configuration_manager_t *self
                                                       , kaa_platform_message_reader_t *reader
                                                       , uint16_t extension_options
                                                       , size_t extension_length)
{
    KAA_RETURN_IF_NIL2(self, reader, KAA_ERR_BADPARAM);

    KAA_LOG_INFO(self->logger, KAA_ERR_NONE, "Received configuration server sync: options %u, payload size %u", extension_options, extension_length);

    if (extension_length >= sizeof(uint32_t)) {
        if (extension_options & KAA_CONFIGURATION_BODY_PRESENT) {
            uint32_t body_size = KAA_NTOHL(*((uint32_t *) reader->current));
            reader->current += sizeof(uint32_t);
            KAA_LOG_INFO(self->logger, KAA_ERR_NONE, "Received configuration body, size '%u' ", body_size);
            const char* body = reader->current;
            kaa_error_t error = kaa_platform_message_skip(reader, kaa_aligned_size_get(body_size));
            if (error) {
                 KAA_LOG_ERROR(self->logger, error, "Failed to read configuration body, size %u", body_size);
                 return error;
            }

#if KAA_CONFIGURATION_DELTA_SUPPORT

#else
            if (self->root_record)
                self->root_record->destroy(self->root_record);

            self->root_record = kaa_configuration_manager_deserialize(body, body_size);
            if (!self->root_record) {
                KAA_LOG_ERROR(self->logger, KAA_ERR_READ_FAILED, "Failed to deserialize configuration body, size %u", body_size);
                return KAA_ERR_READ_FAILED;
            }

            kaa_error_t err = ext_calculate_sha_hash(body, body_size, self->configuration_hash);
            if (err) {
                KAA_LOG_WARN(self->logger, err, "Failed to calculate configuration body hash");
                return err;
            }
            ext_configuration_store(body, body_size);
#endif
            if (self->root_receiver.on_configuration_updated)
                self->root_receiver.on_configuration_updated(self->root_receiver.context, self->root_record);

            kaa_transport_channel_interface_t *channel =
                    kaa_channel_manager_get_transport_channel(self->channel_manager, configuration_sync_services[0]);
            if (channel)
                channel->sync_handler(channel->context, configuration_sync_services, 1);
        }
    }
    return KAA_ERR_NONE;
}



const kaa_root_configuration_t *kaa_configuration_manager_get_configuration(kaa_configuration_manager_t *self)
{
    return self ? self->root_record : NULL;
}



kaa_error_t kaa_configuration_manager_set_root_receiver(kaa_configuration_manager_t *self, const kaa_configuration_root_receiver_t *receiver)
{
    KAA_RETURN_IF_NIL2(self, receiver, KAA_ERR_BADPARAM);

    self->root_receiver = *receiver;

    return KAA_ERR_NONE;
}


#endif /* KAA_DISABLE_FEATURE_CONFIGURATION */
