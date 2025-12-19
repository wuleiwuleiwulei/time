/*
 * Copyright (C) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SNTP_CLIENT_SNTP_CLIENT_H
#define SNTP_CLIENT_SNTP_CLIENT_H

#include <string>
#include <sys/types.h>

namespace OHOS {
namespace MiscServices {
class SNTPClient {
public:
    bool RequestTime(const std::string &host);
    int64_t getNtpTime();
    int64_t getNtpTimeReference();
    int64_t getRoundTripTime();

private:
    struct ntp_timestamp {
        uint64_t second;
        uint64_t fraction;
    };

    struct date_structure {
        int hour;
        int minute;
        int second;
        int millisecond;
    };

    struct SNTPMessage {
        unsigned char _leapIndicator;
        unsigned char _versionNumber;
        unsigned char _mode;
        unsigned char _stratum;
        unsigned char _pollInterval;
        unsigned char _precision;
        unsigned int _rootDelay;
        unsigned int _rootDispersion;
        unsigned int _referenceIdentifier[4];
        uint64_t _referenceTimestamp;
        uint64_t _originateTimestamp;
        uint64_t _receiveTimestamp;
        uint64_t _transmitTimestamp;

        /**
         * Zero all the values.
         */
        void clear();
    };

    unsigned int GetNtpField32(int offset, const char *buffer);
    /**
     * This function returns an array based on the Reference ID
     * (converted from NTP message), given the offset provided.
     *
     * @param offset the offset of the field in the NTP message
     * @param buffer the received message
     *
     * Returns the array of Reference ID
     */
    void GetReferenceId(int offset, char *buffer, int *_outArray);
    /**
     * This function sets the clock offset in ms.
     * Negative value means the local clock is ahead,
     * positive means the local clock is behind (relative to the NTP server)
     *
     * @param clockOffset the clock offset in ms
     */
    void SetClockOffset(int clockOffset);

    /**
     * This function converts the UNIX time to NTP
     *
     * @param ntpTs the structure NTP where the NTP values are stored
     * @param unixTs the structure UNIX (with the already set tv_sec and tv_usec)
     */
    void ConvertUnixToNtp(struct ntp_timestamp *ntpTs, struct timeval *unixTs);

    /**
    * This function creates the SNTP message ready for transmission (SNTP Req)
    * and returns it back.
    *
    * @param buffer the message to be sent
    */
    void CreateMessage(char *buffer);

    /**
     * This function gets the information received from the SNTP response
     * and prints the results (e.g. offset, round trip delay etc.)
     *
     * @param buffer the message received
     */
    bool ReceivedMessage(char *buffer);

    /**
     * This function returns the timestamp (64-bit) from the received
     * buffer, given the offset provided.
     *
     * @param offset the offset of the timestamp in the NTP message
     * @param buffer the received message
     *
     * Returns the ntp timestamp
     */
    uint64_t GetNtpTimestamp64(int offset, const char *buffer);

    /**
     * This function converts the NTP time to timestamp
     *
     * @param _ntpTs the NTP timestamp to be converted
     * Returns the milliseconds
     */
    int64_t ConvertNtpToStamp(uint64_t _ntpTs);
    int64_t m_clockOffset;
    int64_t m_originateTimestamp;
    int64_t mNtpTime;
    int64_t mNtpTimeReference;
    int64_t mRoundTripTime;
};
} // namespace MiscServices
} // namespace OHOS
#endif // SNTP_CLIENT_SNTP_CLIENT_H