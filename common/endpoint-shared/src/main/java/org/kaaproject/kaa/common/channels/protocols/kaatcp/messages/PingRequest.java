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

package org.kaaproject.kaa.common.channels.protocols.kaatcp.messages;

import org.kaaproject.kaa.common.channels.protocols.kaatcp.KaaTcpProtocolException;

/**
 * Kaatcp PingRequest Class.
 * The PINGREQ message is an "are you alive?" message that is sent
 * from a connected client to the server.
 * @author Andrey Panasenko
 *
 */
public class PingRequest extends MqttFrame {


    /**
     * Default constructor.
     */
    public PingRequest() {
        super();
        setMessageType(MessageType.PINGREQ);
        remainingLength = 0;
    }

    /* (non-Javadoc)
     * @see org.kaaproject.kaa.common.channels.protocols.kaatcp.messages.mqttFrame#pack()
     */
    @Override
    protected void pack() {
    }

    /* (non-Javadoc)
     * @see org.kaaproject.kaa.common.channels.protocols.kaatcp.messages.mqttFrame#decode()
     */
    @Override
    protected void decode() throws KaaTcpProtocolException {

    }

    /* (non-Javadoc)
     * @see org.kaaproject.kaa.common.channels.protocols.kaatcp.messages.MqttFrame#isNeedCloseConnection()
     */
    @Override
    public boolean isNeedCloseConnection() {
        return false;
    }

}
