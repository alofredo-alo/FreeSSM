// SPDX-License-Identifier: GPL-3.0-or-later
#include "DemoDiagInterface.h"
#include "DiagnosticSafety.h"
#include "EngineDialog.h"
#include "TransmissionDialog.h"
#include "VehicleReportScan.h"
#include "VehicleReportPdf.h"
#include "SSMP2communication_core.h"
#include <QTemporaryDir>
#include <QProcess>
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <cassert>
#include <iostream>
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    DiagnosticSafety::setDemo(true);
    DiagnosticSafety::setReadOnly(false);
    assert(DiagnosticSafety::isDemo() && DiagnosticSafety::isReadOnly());
    DemoDiagInterface iface;
    assert(iface.open("ignored"));
    assert(!iface.connect(AbstractDiagInterface::protocol_type::SSM2_ISO15765));
    assert(iface.connect(AbstractDiagInterface::protocol_type::SSM2_ISO14230));
    std::vector<char> rejected = {'\x80', '\x10', '\xf0', '\x01', '\xb8', '\x39'};
    assert(!iface.write(rejected));
    SSMP2communication_core core(&iface);
    std::vector<char> first, second;
    assert(core.ReadMultipleDatabytes(0x10, 0, {0x0e, 0x0f}, &first) == SSMP2communication_core::Result::success);
    QThread::msleep(1100);
    assert(core.ReadMultipleDatabytes(0x10, 0, {0x0e, 0x0f}, &second) == SSMP2communication_core::Result::success);
    assert(first != second);
    assert(iface.disconnect());
    const auto report = VehicleReport::scanEngineAndTransmission(&iface, "en", false);
    assert(report.simulated && report.modules.size() == 5);
    for (int i = 0; i < 2; ++i) {
        const auto& module = report.modules[i];
        if (module.status != VehicleReport::QueryStatus::Complete) std::cerr << module.name << ": " << module.detail << '\n';
        assert(module.status == VehicleReport::QueryStatus::Complete);
        assert(module.codes.size() == 2);
        assert(module.codes[0].code == "P0335" && module.codes[1].code == "P0336");
    }
    assert(report.vin.value == "JF1ABCDEFG1234567");
    std::string json, jsonError;
    assert(VehicleReport::toJson(report, &json, &jsonError));
    assert(json.find("\"simulado\":true") != std::string::npos);
    QTemporaryDir temp; assert(temp.isValid());
    QString pdfError; const QString pdf = temp.path() + "/demo.pdf";
    assert(VehicleReport::writePdf(report, pdf, &pdfError));
    QProcess extract; extract.start("pdftotext", QStringList() << pdf << "-");
    assert(extract.waitForFinished() && extract.exitCode() == 0);
    assert(QString::fromUtf8(extract.readAllStandardOutput()).contains("DATOS SIMULADOS"));
    assert(DiagnosticSafety::isReadOnly());
    EngineDialog engine(&iface, "en", false);
    assert(engine.windowTitle().contains("DATOS SIMULADOS"));
    assert(engine.setup(ControlUnitDialog::ContentSelection::DCsMode, QStringList()));
    engine.close();
    // EngineDialog owns an active protocol until destruction; test a separate interface.
    DemoDiagInterface transmissionIface; assert(transmissionIface.open("demo"));
    TransmissionDialog transmission(&transmissionIface, "en", false);
    assert(transmission.windowTitle().contains("DATOS SIMULADOS"));
    assert(transmission.setup(ControlUnitDialog::ContentSelection::DCsMode, QStringList()));
    transmission.close();
    DemoDiagInterface liveIface; assert(liveIface.open("demo"));
    EngineDialog liveEngine(&liveIface, "en", false);
    assert(liveEngine.setup(ControlUnitDialog::ContentSelection::MBsSWsMode, QStringList()));
    liveEngine.close();
    DemoDiagInterface liveTcuIface; assert(liveTcuIface.open("demo"));
    TransmissionDialog liveTcu(&liveTcuIface, "en", false);
    assert(liveTcu.setup(ControlUnitDialog::ContentSelection::MBsSWsMode, QStringList()));
    liveTcu.close();
    std::cout << "Demo integrada: SSM2, motor, transmisión, VIN y DTC OK\n";
}
