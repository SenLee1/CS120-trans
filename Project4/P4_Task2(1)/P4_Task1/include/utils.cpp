#include "utils.h"
#include <JuceHeader.h>

IPType Str2IPType(const std::string &ip) {
    juce::IPAddress tmp(ip);
    IPType ret = 0;
    for (int i = 0; i < 4; ++i)
        ret = ret << 8 | tmp.address[i];
    return ret;
}

std::string IPType2Str(IPType ip) {
    juce::IPAddress tmp(ip);
    return tmp.toString().toStdString();
}

// 新增：标准的 CRC32 实现 (IEEE 802.3)
unsigned int calculateCRC32(const std::string &data) {
    unsigned int crc = 0xFFFFFFFF;
    for (char c : data) {
        crc ^= (unsigned char)c;
        for (int i = 0; i < 8; i++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}