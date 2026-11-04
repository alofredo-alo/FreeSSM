/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "OBDFreezeFrame.h"
namespace OBDFreezeFrame {
Result decode(const std::vector<uint8_t>& response, uint8_t pid, uint8_t frame, Value* output) {
    if (!output || response.size() < 3 || response[0] != 0x42 ||
        response[1] != pid || response[2] != frame) return Result::InvalidResponse;
    Value value;
    value.pid = pid;
    value.frame = frame;
    switch (pid) {
    case 0x02: {
        if (response.size() < 5) return Result::InvalidResponse;
        const uint8_t a = response[3], b = response[4];
        if (!a && !b) return Result::NoDTC;
        static const char types[] = {'P','C','B','U'};
        static const char hex[] = "0123456789ABCDEF";
        value.name = "DTC";
        value.code.push_back(types[a >> 6]);
        value.code.push_back(hex[(a >> 4) & 3]);
        value.code.push_back(hex[a & 15]);
        value.code.push_back(hex[b >> 4]);
        value.code.push_back(hex[b & 15]);
        break;
    }
    case 0x04: case 0x11:
        if (response.size() < 4) return Result::InvalidResponse;
        value.name = pid == 0x04 ? "Calculated engine load" : "Throttle position";
        value.unit = "%"; value.number = response[3] * 100.0 / 255.0; break;
    case 0x05: case 0x0f:
        if (response.size() < 4) return Result::InvalidResponse;
        value.name = pid == 0x05 ? "Coolant temperature" : "Intake air temperature";
        value.unit = "C"; value.number = static_cast<int>(response[3]) - 40; break;
    case 0x0b:
        if (response.size() < 4) return Result::InvalidResponse;
        value.name = "Manifold absolute pressure"; value.unit = "kPa"; value.number = response[3]; break;
    case 0x0c:
        if (response.size() < 5) return Result::InvalidResponse;
        value.name = "Engine speed"; value.unit = "rpm";
        value.number = (response[3] * 256 + response[4]) / 4.0; break;
    case 0x0d:
        if (response.size() < 4) return Result::InvalidResponse;
        value.name = "Vehicle speed"; value.unit = "km/h"; value.number = response[3]; break;
    default: return Result::UnsupportedPID;
    }
    *output = value;
    return Result::Success;
}
}
