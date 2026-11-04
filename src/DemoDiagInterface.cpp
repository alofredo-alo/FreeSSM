/* SPDX-License-Identifier: GPL-3.0-or-later
 * In-memory SSM2 simulator adapted from tools/fake_ecu.py by Jacob Seman.
 * Synthetic flags, memory and protocol behavior are based on that GPL source.
 */
#include "DemoDiagInterface.h"
#include <QMutexLocker>
#include <string>

namespace {
unsigned char byte(char value) { return static_cast<unsigned char>(value); }
unsigned int address(const std::vector<char>& b, std::size_t at) {
    return (static_cast<unsigned int>(byte(b[at])) << 16) |
           (static_cast<unsigned int>(byte(b[at+1])) << 8) | byte(b[at+2]);
}
}
DemoDiagInterface::DemoDiagInterface() : open_(false), connected_(false) {
    setName("DEMO - DATOS SIMULADOS");
    setVersion("1.0");
    setSupportedProtocols({protocol_type::SSM2_ISO14230});
    timer_.start();
}
AbstractDiagInterface::interface_type DemoDiagInterface::interfaceType() { return interface_type::serialPassThrough; }
bool DemoDiagInterface::open(std::string) { QMutexLocker lock(&mutex_); if (open_) return false; open_ = true; return true; }
bool DemoDiagInterface::isOpen() { QMutexLocker lock(&mutex_); return open_; }
bool DemoDiagInterface::close() { QMutexLocker lock(&mutex_); connected_ = false; open_ = false; pending_.clear(); setProtocolType(protocol_type::NONE); return true; }
bool DemoDiagInterface::connect(protocol_type protocol) {
    QMutexLocker lock(&mutex_);
    if (!open_ || connected_ || protocol != protocol_type::SSM2_ISO14230) return false;
    setProtocolType(protocol); setProtocolBaudrate(4800); connected_ = true; return true;
}
bool DemoDiagInterface::isConnected() { QMutexLocker lock(&mutex_); return connected_; }
bool DemoDiagInterface::disconnect() {
    QMutexLocker lock(&mutex_); connected_ = false; pending_.clear(); setProtocolType(protocol_type::NONE); setProtocolBaudrate(0); return true;
}
bool DemoDiagInterface::clearSendBuffer() { return true; }
bool DemoDiagInterface::clearReceiveBuffer() { QMutexLocker lock(&mutex_); pending_.clear(); return true; }
bool DemoDiagInterface::read(std::vector<char>* out) {
    if (!out) return false;
    QMutexLocker lock(&mutex_); if (!open_ || !connected_) return false;
    out->swap(pending_); pending_.clear(); return true;
}
unsigned char DemoDiagInterface::dataByte(unsigned int a) const {
    const unsigned int rpmRaw = (850 + static_cast<unsigned int>((timer_.elapsed() / 1000) % 8) * 100) * 4;
    if (a == 0x0e) return static_cast<unsigned char>(rpmRaw >> 8);
    if (a == 0x0f) return static_cast<unsigned char>(rpmRaw);
    if (a == 0x07) return 46; // engine load
    if (a == 0x08) return 125; // coolant: 85 C
    if (a == 0x09 || a == 0x0a) return 128;
    if (a == 0x0d) return 35;
    if (a == 0x11) return 148;
    if (a == 0x12) return 68;
    if (a == 0x15) return 8;
    if (a == 0x19) return 90;
    if (a == 0x1c) return 172;
    if (a == 0x20) return 10;
    if (a == 0x22) return 128;
    if (a == 0x23) return 85;
    if (a == 0x24) return 78;
    if (a == 0x46) return 128;
    if (a == 0x62) return 0xc8;
    if (a == 0x64) return 0xc0;
    if (a == 0x65) return 0x08;
    if (a == 0x8e) return 0x01;
    if (a == 0xae) return 0x04;
    if (a == 0xdb) return 0x20;
    const std::string vin = "JF1ABCDEFG1234567"; // fictional
    if (a >= 0x2000 && a < 0x2000 + vin.size()) return static_cast<unsigned char>(vin[a - 0x2000]);
    return 0;
}
bool DemoDiagInterface::write(std::vector<char> frame) {
    QMutexLocker lock(&mutex_);
    if (!open_ || !connected_ || frame.size() < 6 || byte(frame[0]) != 0x80 ||
        (byte(frame[1]) != 0x10 && byte(frame[1]) != 0x18) || byte(frame[2]) != 0xf0 ||
        frame.size() != static_cast<std::size_t>(byte(frame[3])) + 5) return false;
    unsigned int sum = 0; for (std::size_t i = 0; i + 1 < frame.size(); ++i) sum += byte(frame[i]);
    if (static_cast<unsigned char>(sum) != byte(frame.back())) return false;
    const unsigned char service = byte(frame[4]);
    std::vector<char> payload;
    if (service == 0xbf && byte(frame[3]) == 1) {
        const unsigned char id[] = {0xff, 0xa2, 0x10, 0x01, 0x1d, 0x12, 0, 7, 5};
        for (unsigned char c : id) payload.push_back(static_cast<char>(c));
        std::vector<unsigned char> flags(48, 0);
        const int mb[][2] = {{1,1},{1,2},{1,5},{1,6},{1,7},{1,8},{2,2},{2,4},{2,6},{2,7},{2,8},{3,1},{3,2},{3,4},{3,6},{3,8},{4,1},{4,2},{4,4},{4,6},{4,8},{5,2},{5,6},{5,8},{6,4},{8,4},{8,6}, {12,2},{12,3},{12,5},{12,6},{12,7},{13,2},{13,3},{13,4},{13,7},{13,8},{14,1},{14,3},{14,5},{14,6},{14,7},{15,1},{15,5},{15,6},{15,7},{15,8},{16,2},{16,4},{16,5},{16,6},{16,8}};
        for (const auto& f : mb) flags[f[0]-1] |= 1u << (f[1]-1);
        flags[29] |= 1u << 4; flags[36] |= 1u;
        for (unsigned char f : flags) payload.push_back(static_cast<char>(f));
    } else if (service == 0xa8 && byte(frame[3]) >= 5 && (byte(frame[3])-2) % 3 == 0) {
        payload.push_back(static_cast<char>(0xe8));
        for (std::size_t i = 6; i + 2 < frame.size()-1; i += 3) payload.push_back(static_cast<char>(dataByte(address(frame, i))));
    } else if (service == 0xa0 && byte(frame[3]) == 6) {
        payload.push_back(static_cast<char>(0xe0));
        const unsigned int start = address(frame, 6);
        for (unsigned int i=0; i<=byte(frame[9]); ++i) payload.push_back(static_cast<char>(dataByte(start+i)));
    } else return false; // No simulated writes or other services.
    pending_.clear();
    pending_.push_back(static_cast<char>(0x80)); pending_.push_back(static_cast<char>(0xf0)); pending_.push_back(frame[1]);
    pending_.push_back(static_cast<char>(payload.size())); pending_.insert(pending_.end(), payload.begin(), payload.end());
    sum = 0; for (char c : pending_) sum += byte(c);
    pending_.push_back(static_cast<char>(sum));
    return true;
}
