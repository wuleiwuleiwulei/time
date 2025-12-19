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

#include "sntp_client.h"
#include "ntp_trusted_time.h"

#include <netdb.h>
#include <securec.h>
#include <sstream>
#include <sys/time.h>

#include "time_sysevent.h"

namespace OHOS {
namespace MiscServices {
namespace {
constexpr uint64_t SECONDS_SINCE_FIRST_EPOCH = 2208988800; // Seconds from 1/1/1900 00.00 to 1/1/1970 00.00;
constexpr uint64_t MILLISECOND_TO_SECOND = 1000;
constexpr uint64_t FRACTION_TO_SECOND = 0x100000000;
constexpr uint64_t UINT32_MASK = 0xFFFFFFFF;
constexpr int VERSION_MASK = 0x38;
constexpr int MODE_MASK = 0x7;
constexpr int32_t INDEX_ZERO = 0;
constexpr int32_t INDEX_ONE = 1;
constexpr int32_t INDEX_TWO = 2;
constexpr int32_t INDEX_THREE = 3;
constexpr int32_t INDEX_FOUR = 4;
constexpr int32_t TIME_OUT = 5;
constexpr unsigned char MODE_THREE = 3;
constexpr unsigned char VERSION_THREE = 3;
constexpr double TEN_TO_MINUS_SIX_POWER = 1.0e-6;
constexpr const char* NTP_PORT = "123";
constexpr int32_t NTP_MSG_OFFSET_ROOT_DELAY = 4;
constexpr int32_t NTP_MSG_OFFSET_ROOT_DISPERSION = 8;
constexpr int32_t NTP_MSG_OFFSET_REFERENCE_IDENTIFIER = 12;
constexpr int32_t REFERENCE_TIMESTAMP_OFFSET = 16;
constexpr int32_t ORIGINATE_TIMESTAMP_OFFSET = 24;
constexpr int32_t RECEIVE_TIMESTAMP_OFFSET = 32;
constexpr int32_t TRANSMIT_TIMESTAMP_OFFSET = 40;
constexpr int32_t NTP_PACKAGE_SIZE = 48;
constexpr int32_t SNTP_MSG_OFFSET_SIX = 6;
constexpr int32_t SNTP_MSG_OFFSET_THREE = 3;
} // namespace

bool SNTPClient::RequestTime(const std::string &host)
{
    int bufLen = NTP_PACKAGE_SIZE;
    struct addrinfo hints = { 0 };
    struct addrinfo *addrs;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    int error = getaddrinfo(host.c_str(), NTP_PORT, &hints, &addrs);
    if (error != 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "getaddrinfo failed error %{public}d", error);
        return false;
    }

    // Create a socket for sending data
    int sendSocket = socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);
    if (sendSocket < 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE,
                    "create socket failed: %{public}s family: %{public}d socktype: %{public}d protocol: %{public}d",
                    strerror(errno), addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);
        return false;
    }

    // Set send and recv function timeout
    struct timeval timeout = { TIME_OUT, 0 };
    setsockopt(sendSocket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(struct timeval));
    setsockopt(sendSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval));
    if (connect(sendSocket, addrs->ai_addr, addrs->ai_addrlen) < 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "socket connect failed: %{public}s", strerror(errno));
        close(sendSocket);
        return false;
    }

    // Create the NTP tx timestamp and fill the fields in the msg to be tx
    char sendBuf[NTP_PACKAGE_SIZE] = { 0 };
    CreateMessage(sendBuf);
    if (send(sendSocket, sendBuf, bufLen, 0) < 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Send socket message failed: %{public}s, Host: %{public}s",
                    strerror(errno), host.c_str());
        close(sendSocket);
        return false;
    }

    char bufferRx[NTP_PACKAGE_SIZE] = { 0 };
    // Receive until the peer closes the connection
    if (recv(sendSocket, bufferRx, NTP_PACKAGE_SIZE, 0) < 0) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "Receive socket message failed: %{public}s, Host: %{public}s",
                    strerror(errno), host.c_str());
        close(sendSocket);
        return false;
    }
    close(sendSocket);
    if (!ReceivedMessage(bufferRx)) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "ReceivedMessage failed: Host: %{public}s", host.c_str());
        return false;
    }
    return true;
}

void SNTPClient::SetClockOffset(int clockOffset)
{
    m_clockOffset = clockOffset;
}

uint64_t SNTPClient::GetNtpTimestamp64(int offset, const char *buffer)
{
    const int _len = sizeof(uint64_t);
    char valueRx[_len];
    errno_t ret = memset_s(valueRx, sizeof(uint64_t), 0, sizeof(uint64_t));
    if (ret != EOK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "memcpy_s failed, err = %{public}d", ret);
        return false;
    }
    int numOfBit = sizeof(uint64_t) - 1;
    for (int loop = offset; loop < offset + _len; loop++) {
        valueRx[numOfBit] = buffer[loop];
        numOfBit--;
    }

    uint64_t milliseconds;
    ret = memcpy_s(&milliseconds, sizeof(uint64_t), valueRx, sizeof(uint64_t));
    if (ret != EOK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "memcpy_s failed, err = %{public}d", ret);
        return false;
    }
    return le64toh(milliseconds);
}

void SNTPClient::ConvertUnixToNtp(struct ntp_timestamp *ntpTs, struct timeval *unixTs)
{
    // 0x83AA7E80; the seconds from Jan 1, 1900 to Jan 1, 1970
    ntpTs->second = unixTs->tv_sec + SECONDS_SINCE_FIRST_EPOCH; // 0x83AA7E80;
    ntpTs->fraction =
        static_cast<uint64_t>((unixTs->tv_usec + 1) * (1LL << RECEIVE_TIMESTAMP_OFFSET) * TEN_TO_MINUS_SIX_POWER);
    TIME_HILOGD(TIME_MODULE_SERVICE, "end");
}

/*
  *	/// SNTP Timestamp Format (as described in RFC 2030)
  *                         1                   2                   3
  *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *   |                           Seconds                             |
  *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  *   |                  Seconds Fraction (0-padded)                  |
  *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  */
int64_t SNTPClient::ConvertNtpToStamp(uint64_t _ntpTs)
{
    auto second = static_cast<uint32_t>((_ntpTs >> RECEIVE_TIMESTAMP_OFFSET) & UINT32_MASK);
    auto fraction = static_cast<uint32_t>(_ntpTs & UINT32_MASK);
    if (second == 0 && fraction == 0) {
        return 0;
    }
    if (second < SECONDS_SINCE_FIRST_EPOCH) {
        return 0;
    }
    // convert sntp timestamp to seconds
    return ((second - SECONDS_SINCE_FIRST_EPOCH) * MILLISECOND_TO_SECOND) +
           ((fraction * MILLISECOND_TO_SECOND) / FRACTION_TO_SECOND);
}

void SNTPClient::CreateMessage(char *buffer)
{
    struct ntp_timestamp ntp{};
    struct timeval unix;

    gettimeofday(&unix, nullptr);
    // convert unix time to ntp time
    ConvertUnixToNtp(&ntp, &unix);
    uint64_t _ntpTs = ntp.second;
    _ntpTs = (_ntpTs << RECEIVE_TIMESTAMP_OFFSET) | ntp.fraction;
    errno_t ret = TimeUtils::GetBootTimeMs(m_originateTimestamp);
    if (ret != E_TIME_OK) {
        return;
    }

    SNTPMessage _sntpMsg{};
    // Important, if you don't set the version/mode, the server will ignore you.
    _sntpMsg.clear();
    _sntpMsg._leapIndicator = 0;
    _sntpMsg._versionNumber = VERSION_THREE;
    _sntpMsg._mode = MODE_THREE;
    // optional (?)
    _sntpMsg._originateTimestamp = _ntpTs;
    char value[sizeof(uint64_t)];
    ret = memcpy_s(value, sizeof(uint64_t), &_sntpMsg._originateTimestamp, sizeof(uint64_t));
    if (ret != EOK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "memcpy_s failed, err = %{public}d", ret);
        return;
    }
    int numOfBit = sizeof(uint64_t) - 1;
    int offsetEnd = ORIGINATE_TIMESTAMP_OFFSET + sizeof(uint64_t);
    for (int loop = ORIGINATE_TIMESTAMP_OFFSET; loop < offsetEnd; loop++) {
        buffer[loop] = value[numOfBit];
        numOfBit--;
    }
    // create the 1-byte info in one go... the result should be 27 :)
    buffer[INDEX_ZERO] = (_sntpMsg._leapIndicator << SNTP_MSG_OFFSET_SIX) |
                         (_sntpMsg._versionNumber << SNTP_MSG_OFFSET_THREE) | _sntpMsg._mode;
    TIME_HILOGD(TIME_MODULE_SERVICE, "end");
}

bool SNTPClient::ReceivedMessage(char *buffer)
{
    int64_t receiveBootTime = 0;
    errno_t ret = TimeUtils::GetBootTimeMs(receiveBootTime);
    if (ret != E_TIME_OK) {
        return false;
    }
    SNTPMessage _sntpMsg;
    _sntpMsg.clear();
    _sntpMsg._leapIndicator = buffer[INDEX_ZERO] >> SNTP_MSG_OFFSET_SIX;
    _sntpMsg._versionNumber = (buffer[INDEX_ZERO] & VERSION_MASK) >> SNTP_MSG_OFFSET_THREE;
    _sntpMsg._mode = (buffer[INDEX_ZERO] & MODE_MASK);
    _sntpMsg._stratum = buffer[INDEX_ONE];
    _sntpMsg._pollInterval = buffer[INDEX_TWO];
    _sntpMsg._precision = buffer[INDEX_THREE];
    _sntpMsg._rootDelay = GetNtpField32(NTP_MSG_OFFSET_ROOT_DELAY, buffer);
    _sntpMsg._rootDispersion = GetNtpField32(NTP_MSG_OFFSET_ROOT_DISPERSION, buffer);
    int _refId[INDEX_FOUR];
    GetReferenceId(NTP_MSG_OFFSET_REFERENCE_IDENTIFIER, buffer, _refId);
    _sntpMsg._referenceIdentifier[INDEX_ZERO] = _refId[INDEX_ZERO];
    _sntpMsg._referenceIdentifier[INDEX_ONE] = _refId[INDEX_ONE];
    _sntpMsg._referenceIdentifier[INDEX_TWO] = _refId[INDEX_TWO];
    _sntpMsg._referenceIdentifier[INDEX_THREE] = _refId[INDEX_THREE];
    _sntpMsg._referenceTimestamp = GetNtpTimestamp64(REFERENCE_TIMESTAMP_OFFSET, buffer);
    _sntpMsg._originateTimestamp = GetNtpTimestamp64(ORIGINATE_TIMESTAMP_OFFSET, buffer);
    _sntpMsg._receiveTimestamp = GetNtpTimestamp64(RECEIVE_TIMESTAMP_OFFSET, buffer);
    _sntpMsg._transmitTimestamp = GetNtpTimestamp64(TRANSMIT_TIMESTAMP_OFFSET, buffer);
    int64_t _originClient = m_originateTimestamp;
    int64_t _receiveServer = ConvertNtpToStamp(_sntpMsg._receiveTimestamp);
    int64_t _transmitServer = ConvertNtpToStamp(_sntpMsg._transmitTimestamp);
    if (_transmitServer == 0 || _receiveServer == 0) {
        return false;
    }
    int64_t _receiveClient = receiveBootTime;
    int64_t _clockOffset = (((_receiveServer - _originClient) + (_transmitServer - _receiveClient)) / INDEX_TWO);
    int64_t _roundTripDelay = (_receiveClient - _originClient) - (_transmitServer - _receiveServer);
    mRoundTripTime = _roundTripDelay;
    mNtpTime = receiveBootTime + _clockOffset;
    TimeUtils::GetBootTimeMs(mNtpTimeReference);
    SetClockOffset(_clockOffset);
    TIME_HILOGI(TIME_MODULE_SERVICE, "_originClient:%{public}s, _receiveServer:%{public}s, _transmitServer:%{public}s,"
                "_receiveClient:%{public}s", std::to_string(_originClient).c_str(),
                std::to_string(_receiveServer).c_str(), std::to_string(_transmitServer).c_str(),
                std::to_string(_receiveClient).c_str());
    TimeBehaviorReport(ReportEventCode::NTP_REFRESH,
        std::to_string(_originClient) + "|" + std::to_string(_receiveClient),
        std::to_string(_transmitServer) + "|" + std::to_string(_receiveServer), mNtpTime);
    return true;
}

unsigned int SNTPClient::GetNtpField32(int offset, const char *buffer)
{
    const int _len = sizeof(int);
    char valueRx[_len];
    errno_t ret = memset_s(valueRx, _len, 0, _len);
    if (ret != EOK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "memcpy_s failed, err = %{public}d", ret);
        return false;
    }
    int numOfBit = sizeof(int) - 1;
    for (int loop = offset; loop < offset + _len; loop++) {
        valueRx[numOfBit] = buffer[loop];
        numOfBit--;
    }

    unsigned int milliseconds;
    errno_t retValue = memcpy_s(&milliseconds, sizeof(int), valueRx, sizeof(int));
    if (retValue != EOK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "memcpy_s failed, err = %{public}d", retValue);
        return false;
    }
    TIME_HILOGD(TIME_MODULE_SERVICE, "end");
    return milliseconds;
}

void SNTPClient::GetReferenceId(int offset, char *buffer, int *_outArray)
{
    const int _len = sizeof(int);
    int num = 0;
    for (int loop = offset; loop < offset + _len; loop++) {
        _outArray[num] = buffer[loop];
        num++;
    }
    TIME_HILOGD(TIME_MODULE_SERVICE, "end");
}

void SNTPClient::SNTPMessage::clear()
{
    errno_t ret = memset_s(this, sizeof(*this), 0, sizeof(*this));
    if (ret != EOK) {
        TIME_HILOGE(TIME_MODULE_SERVICE, "memcpy_s failed, err = %{public}d", ret);
    }
}

// LCOV_EXCL_START
// The method has no input parameters, impossible to construct fuzz test.
int64_t SNTPClient::getNtpTime()
{
    return mNtpTime;
}

int64_t SNTPClient::getNtpTimeReference()
{
    return mNtpTimeReference;
}

int64_t SNTPClient::getRoundTripTime()
{
    return mRoundTripTime;
}
// LCOV_EXCL_STOP
} // namespace MiscServices
} // namespace OHOS