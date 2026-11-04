// SPDX-License-Identifier: GPL-3.0-or-later
#include "OBDReadiness.h"
#include <cassert>
#include <iostream>
using namespace OBDReadiness;
int main() {
    Snapshot s;
    assert(decode({0x41,0x01,0x82,0x57,0x05,0x04}, &s));
    assert(s.milOn && s.storedDtcCount == 2 && !s.compressionIgnition);
    assert(s.monitors.size() == 11);
    assert(s.monitors[0].id == "misfire" && s.monitors[0].state == MonitorState::NotReady);
    assert(s.monitors[1].state == MonitorState::Ready);
    assert(s.monitors[2].state == MonitorState::NotReady);
    assert(s.monitors[3].state == MonitorState::Ready);
    assert(s.monitors[4].state == MonitorState::Unsupported);
    assert(s.monitors[5].state == MonitorState::NotReady);
    assert(s.rawA == 0x82 && s.rawB == 0x57 && s.rawC == 0x05 && s.rawD == 0x04);
    assert(decode({0x41,0x01,0,0x08,0x00,0xff}, &s));
    assert(s.compressionIgnition && !s.milOn && s.storedDtcCount == 0);
    for (const auto& m : s.monitors) assert(m.state == MonitorState::Unsupported);
    for (unsigned int bit = 0; bit < 8; ++bit) {
        const uint8_t mask = static_cast<uint8_t>(1u << bit);
        for (unsigned int supported = 0; supported < 2; ++supported) {
            for (unsigned int incomplete = 0; incomplete < 2; ++incomplete) {
                assert(decode({0x41, 0x01, 0, 0, static_cast<uint8_t>(supported ? mask : 0),
                               static_cast<uint8_t>(incomplete ? mask : 0)}, &s));
                const auto expected = !supported ? MonitorState::Unsupported :
                    (incomplete ? MonitorState::NotReady : MonitorState::Ready);
                assert(s.monitors[3 + bit].state == expected);
            }
        }
    }
    for (unsigned int bit = 0; bit < 3; ++bit) {
        for (unsigned int supported = 0; supported < 2; ++supported) {
            for (unsigned int incomplete = 0; incomplete < 2; ++incomplete) {
                const uint8_t b = static_cast<uint8_t>((supported << bit) | (incomplete << (bit + 4)));
                assert(decode({0x41, 0x01, 0, b, 0, 0}, &s));
                const auto expected = !supported ? MonitorState::Unsupported :
                    (incomplete ? MonitorState::NotReady : MonitorState::Ready);
                assert(s.monitors[bit].state == expected);
            }
        }
    }
    s.storedDtcCount = 42;
    assert(!decode({0x41,0x01,0,0,0}, &s) && s.storedDtcCount == 42);
    assert(!decode({0x7f,0x01,0x12}, &s) && s.storedDtcCount == 42);
    assert(!decode({0x41,0x02,0,0,0,0}, &s) && s.storedDtcCount == 42);
    assert(!decode({0x41,0x01,0,0,0,0}, nullptr));
    std::cout << "Mode 01 PID 01: readiness, MIL and malformed responses OK\n";
}
