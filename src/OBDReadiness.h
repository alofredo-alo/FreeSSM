/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef OBDREADINESS_H
#define OBDREADINESS_H
#include <cstdint>
#include <string>
#include <vector>
namespace OBDReadiness {
enum class MonitorState { Unsupported, Ready, NotReady };
struct Monitor {
    std::string id;
    MonitorState state = MonitorState::Unsupported;
};
struct Snapshot {
    bool milOn = false;
    uint8_t storedDtcCount = 0;
    bool compressionIgnition = false;
    // Retain the reported bitmaps; labels vary with ignition type and model year.
    uint8_t rawA = 0, rawB = 0, rawC = 0, rawD = 0;
    std::vector<Monitor> monitors;
};
// Decodes one ECU's ISO-TP-reassembled Mode 01 PID 01 response. No I/O.
// Output is unchanged on a malformed, truncated or negative response.
bool decode(const std::vector<uint8_t>& response, Snapshot* output);
}
#endif
