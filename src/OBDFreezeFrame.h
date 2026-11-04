/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef OBDFREEZEFRAME_H
#define OBDFREEZEFRAME_H
#include <cstdint>
#include <string>
#include <vector>
namespace OBDFreezeFrame {
// Decodes one ISO-TP-reassembled Mode 02 response; no vehicle communication.
enum class Result { Success, NoDTC, UnsupportedPID, InvalidResponse };
struct Value {
    uint8_t pid = 0;
    uint8_t frame = 0;
    std::string name;
    std::string unit;
    double number = 0;
    std::string code;
};
// Caller must select the responding ECU and pass the requested PID and frame.
// Output is unchanged on malformed, unsupported or absent data.
Result decode(const std::vector<uint8_t>& response, uint8_t requestedPid,
              uint8_t requestedFrame, Value* output);
}
#endif
