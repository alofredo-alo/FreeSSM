// Tests the production SSM2 communication core with a synthetic interface.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "SSMP2communication_core.h"
#include <cassert>
#include <iostream>
#include <vector>

// Minimal non-Qt libFSSM functions used by the production core in these tests.
char libFSSM::calcchecksum(const char *message, unsigned int n) {
    unsigned char sum = 0;
    for (unsigned int i = 0; i < n; ++i) sum += static_cast<unsigned char>(message[i]);
    return static_cast<char>(sum);
}
void libFSSM::push_backUInt24BigEndian(std::vector<char>& v, unsigned int x) {
    v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x);
}
void libFSSM::push_back_UInt32BigEndian(std::vector<char>& v, unsigned int x) {
    v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x);
}
unsigned int libFSSM::parseUInt32BigEndian(const char *p) {
    const unsigned char *s = reinterpret_cast<const unsigned char *>(p);
    return (static_cast<unsigned int>(s[0]) << 24) | (static_cast<unsigned int>(s[1]) << 16) | (static_cast<unsigned int>(s[2]) << 8) | s[3];
}

class FakeInterface : public AbstractDiagInterface {
public:
    enum Mode { Echo, NoEcho, Incomplete, WriteError, BadChecksum };
    explicit FakeInterface(Mode m) : mode(m) { setProtocolType(protocol_type::SSM2_ISO14230); }
    interface_type interfaceType() override { return interface_type::serialPassThrough; }
    bool open(std::string) override { return true; }
    bool isOpen() override { return true; }
    bool close() override { return true; }
    bool connect(protocol_type) override { return true; }
    bool isConnected() override { return true; }
    bool disconnect() override { return true; }
    bool clearSendBuffer() override { return true; }
    bool clearReceiveBuffer() override { pending.clear(); return true; }
    bool write(std::vector<char> request) override {
        if (mode == WriteError) return false;
        if (mode == Echo) pending = request;
        std::vector<char> reply = {'\x80', '\xF0', '\x10', '\x02', '\xE0', '\x42'};
        reply.push_back(libFSSM::calcchecksum(reply.data(), reply.size()));
        if (mode == BadChecksum) reply.back() ^= 1;
        if (mode == Incomplete) reply.resize(4);
        pending.insert(pending.end(), reply.begin(), reply.end());
        return true;
    }
    bool read(std::vector<char> *out) override { *out = pending; pending.clear(); return true; }
private:
    Mode mode;
    std::vector<char> pending;
};

int main() {
    using Result = SSMP2communication_core::Result;
    for (FakeInterface::Mode mode : {FakeInterface::Echo, FakeInterface::NoEcho, FakeInterface::Incomplete, FakeInterface::WriteError, FakeInterface::BadChecksum}) {
        FakeInterface interface(mode);
        SSMP2communication_core core(&interface);
        std::vector<char> data;
        Result result = core.ReadDataBlock(0x10, 0, 0x1234, 1, &data);
        if (mode == FakeInterface::Echo || mode == FakeInterface::NoEcho) {
            assert(result == Result::success);
            assert(data.size() == 1 && static_cast<unsigned char>(data[0]) == 0x42);
        } else {
            assert(result == Result::error);
            assert(data.empty());
        }
    }
    std::cout << "SSM2: echo, no echo, incomplete, write error and checksum OK\n";
}
