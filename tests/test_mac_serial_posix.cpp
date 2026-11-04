// SPDX-License-Identifier: GPL-3.0-or-later
// Compatibility test on Linux only, using a stub for macOS's IOSSIOSPEED.
#include "serialCOM.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>

int main(int argc, char** argv) {
    if (argc != 3 || std::string(argv[1]).find("/dev/pts/") != 0) return 2;
    const bool echo = std::string(argv[2]) == "echo";
    serialCOM port;
    if (!port.OpenPort(argv[1]) || !port.SetPortSettings(4800, 8, 'N', 1)) return 3;
    double baud = 0;
    if (!port.GetPortSettings(&baud) || baud != 4800) return 4;
    int outcome = 0;
    std::thread worker([&]() {
    auto exercise = [&]() -> int {
    std::vector<char> request = {char(0x80), char(0x10), char(0xf0), 1, char(0xbf), char(0x40)};
    if (!port.Write(request)) return 5;
    std::vector<char> result;
    if (echo && (!port.Read(6, 6, 2000, &result) || result != request)) return 6;
    if (!port.Read(4, 4, 2000, &result) || result.size() != 4 ||
        static_cast<unsigned char>(result[0]) != 0x80 ||
        static_cast<unsigned char>(result[1]) != 0xf0 ||
        static_cast<unsigned char>(result[2]) != 0x10) return 7;
    const unsigned int length = static_cast<unsigned char>(result[3]);
    unsigned int sum = 0;
    for (char value : result) sum += static_cast<unsigned char>(value);
    if (!port.Read(length + 1, length + 1, 2000, &result) || result.size() != length + 1 ||
        static_cast<unsigned char>(result[0]) != 0xff) return 8;
    for (unsigned int i = 0; i < length; ++i) sum += static_cast<unsigned char>(result[i]);
    if ((sum & 0xff) != static_cast<unsigned char>(result[length])) return 9;
    return 0;
    };
    outcome = exercise();
    });
    worker.join();
    if (outcome) return outcome;
    if (!port.ClosePort()) return 10;
    std::cout << "macOS POSIX serial compatibility on Linux PTY OK\n";
}
