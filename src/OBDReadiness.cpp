/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "OBDReadiness.h"
namespace OBDReadiness {
namespace {
MonitorState state(bool supported, bool incomplete) {
    return !supported ? MonitorState::Unsupported :
        (incomplete ? MonitorState::NotReady : MonitorState::Ready);
}
}
bool decode(const std::vector<uint8_t>& response, Snapshot* output) {
    if (!output || response.size() < 6 || response[0] != 0x41 || response[1] != 0x01) return false;
    Snapshot value;
    value.rawA = response[2]; value.rawB = response[3]; value.rawC = response[4]; value.rawD = response[5];
    value.milOn = (value.rawA & 0x80) != 0;
    value.storedDtcCount = value.rawA & 0x7f;
    value.compressionIgnition = (value.rawB & 0x08) != 0;
    static const char* common[] = {"misfire", "fuel_system", "components"};
    for (unsigned int i = 0; i < 3; ++i) {
        Monitor m; m.id = common[i];
        m.state = state((value.rawB & (1u << i)) != 0, (value.rawB & (1u << (i + 4))) != 0);
        value.monitors.push_back(m);
    }
    for (unsigned int i = 0; i < 8; ++i) {
        Monitor m; m.id = "non_continuous_bit_" + std::to_string(i);
        m.state = state((value.rawC & (1u << i)) != 0, (value.rawD & (1u << i)) != 0);
        value.monitors.push_back(m);
    }
    *output = value;
    return true;
}
}
