// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DEMODIAGINTERFACE_H
#define DEMODIAGINTERFACE_H
#include "AbstractDiagInterface.h"
#include <QElapsedTimer>
#include <QMutex>
#include <vector>

// In-memory SSM2 vehicle. No OS device, DLL, or physical interface is opened.
class DemoDiagInterface : public AbstractDiagInterface {
public:
    DemoDiagInterface();
    interface_type interfaceType() override;
    bool open(std::string name) override;
    bool isOpen() override;
    bool close() override;
    bool connect(protocol_type protocol) override;
    bool isConnected() override;
    bool disconnect() override;
    bool read(std::vector<char>* buffer) override;
    bool write(std::vector<char> buffer) override;
    bool clearSendBuffer() override;
    bool clearReceiveBuffer() override;
private:
    unsigned char dataByte(unsigned int address) const;
    bool open_;
    bool connected_;
    std::vector<char> pending_;
    QElapsedTimer timer_;
    QMutex mutex_;
};
#endif
