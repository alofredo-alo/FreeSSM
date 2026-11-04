// SPDX-License-Identifier: GPL-3.0-or-later
// Only opens Linux pseudo-terminals created by the companion test.
#include "SerialPassThroughDiagInterface.h"
#include "SSMP2communication_core.h"
#include "SSMprotocol2.h"
#include "CUcontent_DCs_abstract.h"
#include "DiagnosticSafety.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QVBoxLayout>
#include <iostream>
#include <string>

class SimulatorWidget : public CUcontent_DCs_abstract {
public:
    SimulatorWidget() : CUcontent_DCs_abstract(nullptr) { setLayout(new QVBoxLayout); }
    bool setup(SSMprotocol* p) override {
        _SSMPdev = p;
        return p->getSupportedDCgroups(&_supportedDCgroups) && _supportedDCgroups != 0;
    }
    VehicleReport::Module result() const { return _reportCollector->result(); }
protected:
    void connectGUIelements() override {}
    void disconnectGUIelements() override {}
#ifndef SMALL_RESOLUTION
    void createDCprintTables(QTextCursor) override {}
#endif
};

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]).find("/dev/pts/") != 0) return 2;
    QApplication app(argc, argv);
    DiagnosticSafety::setReadOnly(true);
    SerialPassThroughDiagInterface iface;
    if (!iface.open(argv[1])) { std::cerr << "cannot open simulated PTY\n"; return 1; }
    if (!iface.connect(AbstractDiagInterface::protocol_type::SSM2_ISO14230)) {
        std::cerr << "cannot configure simulated PTY\n"; return 1;
    }
    SSMP2communication_core core(&iface);
    std::vector<char> data;
    const auto result = core.GetCUdata(0x10, &data);
    if (result != SSMP2communication_core::Result::success || data.size() != 56 ||
        static_cast<unsigned char>(data[0]) != 0xa2 || static_cast<unsigned char>(data[3]) != 0x1d) {
        std::cerr << "invalid ECU identification\n"; return 1;
    }
    SSMprotocol2 protocol(&iface, "en");
    if (protocol.setupCUdata(CUtype::Engine, false) != SSMprotocol::result_success) {
        std::cerr << "failed to initialize simulated engine\n"; return 1;
    }
    bool vinSupported = false;
    QString vin, cached;
    if (!protocol.hasVINsupport(&vinSupported) || !vinSupported ||
        !protocol.getVIN(&vin) || vin != "JF1ABCDEFG1234567" ||
        !protocol.getCachedVIN(&cached) || cached != vin) {
        std::cerr << "failed to read and cache fictional VIN\n"; return 1;
    }
    SimulatorWidget widget;
    if (!widget.setup(&protocol) || !widget.startDCreading()) {
        std::cerr << "failed to start simulated DTC reading\n"; return 1;
    }
    QElapsedTimer timer;
    timer.start();
    while (widget.result().status == VehicleReport::QueryStatus::NotQueried && timer.elapsed() < 10000) {
        app.processEvents();
        QThread::msleep(10);
    }
    const auto captured = widget.result();
    if (captured.status != VehicleReport::QueryStatus::Complete) {
        std::cerr << "simulated DTC capture incomplete\n";
        widget.stopDCreading();
        return 1;
    }
    if (captured.codes.size() != 2 || captured.codes[0].code != "P0335" || captured.codes[1].code != "P0336" ||
        captured.codes[0].category != VehicleReport::CodeCategory::CurrentOrTemporary ||
        captured.codes[1].category != VehicleReport::CodeCategory::HistoricOrMemorized) {
        std::cerr << "unexpected simulated DTCs or categories\n";
        widget.stopDCreading();
        return 1;
    }
    VehicleReport::Report report;
    report.modules.push_back(captured);
    std::string json, error;
    if (!VehicleReport::toJson(report, &json, &error) || json.find("\"estado_consulta\":\"completa\"") == std::string::npos) {
        std::cerr << "cannot serialize simulated DTC capture: " << error << '\n';
        widget.stopDCreading();
        return 1;
    }
    widget.stopDCreading();
    std::cout << "SSM2 serial PTY: identification, DTC signals and report OK\n";
}
