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

#ifndef KAA_HTONLL_H_
#define KAA_HTONLL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t htonll(uint64_t hostlonglong);

uint64_t ntohll(uint64_t netlonglong);

#ifdef __cplusplus
}
#endif

#endif /* KAA_HTONLL_H_ */
