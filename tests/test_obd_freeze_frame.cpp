// SPDX-License-Identifier: GPL-3.0-or-later
#include "OBDFreezeFrame.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace OBDFreezeFrame;
int main() {
    Value v;
    assert(decode({0x42,0x02,0x00,0x01,0x33}, 0x02, 0, &v) == Result::Success && v.code == "P0133");
    assert(decode({0x42,0x02,0x00,0x43,0x45}, 0x02, 0, &v) == Result::Success && v.code == "C0345");
    assert(decode({0x42,0x02,0x00,0x81,0x23}, 0x02, 0, &v) == Result::Success && v.code == "B0123");
    assert(decode({0x42,0x02,0x00,0xc1,0x00}, 0x02, 0, &v) == Result::Success && v.code == "U0100");
    v.code = "unchanged";
    assert(decode({0x42,0x02,0x00,0,0}, 0x02, 0, &v) == Result::NoDTC && v.code == "unchanged");
    assert(decode({0x42,0x0c,0x00,0x1a,0xf8}, 0x0c, 0, &v) == Result::Success && v.number == 1726.0 && v.unit == "rpm");
    assert(decode({0x42,0x05,0x00,0x7b}, 0x05, 0, &v) == Result::Success && v.number == 83 && v.unit == "C");
    assert(decode({0x42,0x04,0x00,0xff}, 0x04, 0, &v) == Result::Success && std::fabs(v.number - 100.0) < 0.001);
    assert(decode({0x42,0x0d,0x00,0x64}, 0x0d, 0, &v) == Result::Success && v.number == 100 && v.unit == "km/h");
    assert(decode({0x42,0x0c,0x00,0x1a}, 0x0c, 0, &v) == Result::InvalidResponse);
    assert(decode({0x7f,0x02,0x11}, 0x0c, 0, &v) == Result::InvalidResponse);
    assert(decode({0x42,0x0c,0x01,0x1a,0xf8}, 0x0c, 0, &v) == Result::InvalidResponse);
    assert(decode({0x42,0xff,0x00,0x01}, 0xff, 0, &v) == Result::UnsupportedPID);
    assert(decode({0x42,0x0c,0x00,0x1a,0xf8}, 0x0c, 0, nullptr) == Result::InvalidResponse);
    std::cout << "Mode 02: fictional DTC, values and malformed responses OK\n";
}
