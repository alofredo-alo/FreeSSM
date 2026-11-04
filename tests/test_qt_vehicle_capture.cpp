// SPDX-License-Identifier: GPL-3.0-or-later
#include "CUcontent_DCs_abstract.h"
#include "VehicleReportPdf.h"
#include <QTemporaryDir>
#include <QFile>
#include <QApplication>
#include <QProcess>
#include <QVBoxLayout>
#include <cassert>
#include <iostream>

class FakeInterface : public AbstractDiagInterface {
public:
    interface_type interfaceType() override { return interface_type::serialPassThrough; }
    bool open(std::string) override { return true; }
    bool isOpen() override { return true; }
    bool close() override { return true; }
    bool connect(protocol_type) override { return true; }
    bool isConnected() override { return true; }
    bool disconnect() override { return true; }
    bool read(std::vector<char>*) override { return true; }
    bool write(std::vector<char>) override { return true; }
    bool clearSendBuffer() override { return true; }
    bool clearReceiveBuffer() override { return true; }
};
class FakeProtocol : public SSMprotocol {
public:
    explicit FakeProtocol(AbstractDiagInterface* iface) : SSMprotocol(iface) {
        _state = state_normal;
        _CU = CUtype::Engine;
        _unverifiedDtcDefinitions = true;
    }
    CUsetupResult_dt setupCUdata(enum CUtype) override { return result_success; }
    protocol_dt protocolType() override { return SSM2; }
    std::string getROMID() const override { return "ROM-PRUEBA"; }
    bool startDCreading(int groups) override { _state = state_DCreading; _selectedDCgroups = groups; return true; }
    bool stopDCreading() override { _state = state_normal; return true; }
    void current(bool incomplete = false) { emit currentOrTemporaryDTCs(QStringList() << "P0001", QStringList() << "test", false, incomplete); }
    void historic() { emit historicOrMemorizedDTCs(QStringList(), QStringList()); }
    void invalid() { emit currentOrTemporaryDTCs(QStringList() << "", QStringList() << "invalid", false, false); }
};
class TestWidget : public CUcontent_DCs_abstract {
public:
    TestWidget() : CUcontent_DCs_abstract(nullptr) { setLayout(new QVBoxLayout); }
    bool setup(SSMprotocol* protocol) override {
        _SSMPdev = protocol;
        _supportedDCgroups = SSMprotocol::currentDTCs_DCgroup | SSMprotocol::historicDTCs_DCgroup;
        return true;
    }
    VehicleReport::Module result() const { assert(_reportCollector); return _reportCollector->result(); }
protected:
    void connectGUIelements() override {}
    void disconnectGUIelements() override {}
#ifndef SMALL_RESOLUTION
    void createDCprintTables(QTextCursor) override {}
#endif
};
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    FakeInterface iface;
    FakeProtocol protocol(&iface);
    TestWidget widget;
    assert(widget.setup(&protocol));
    assert(widget.startDCreading());
    assert(widget.result().status == VehicleReport::QueryStatus::NotQueried);
    protocol.current();
    assert(widget.result().status == VehicleReport::QueryStatus::NotQueried);
    protocol.historic();
    auto module = widget.result();
    assert(module.status == VehicleReport::QueryStatus::Complete);
    assert(module.codes.size() == 1 && module.codes[0].code == "P0001");
    assert(module.romId.value == "ROM-PRUEBA");
    assert(module.unverifiedDtcDefinitions);
    VehicleReport::Report report;
    report.modules.push_back(module);
    QTemporaryDir temp;
    assert(temp.isValid());
    QString pdfError;
    const QString pdfPath = temp.path() + "/report.pdf";
    assert(VehicleReport::writePdf(report, pdfPath, &pdfError));
    QFile pdf(pdfPath);
    assert(pdf.open(QIODevice::ReadOnly));
    assert(pdf.read(5) == "%PDF-");
    QProcess extract;
    extract.start("pdftotext", QStringList() << pdfPath << "-");
    assert(extract.waitForFinished() && extract.exitCode() == 0);
    const QString pdfText = QString::fromUtf8(extract.readAllStandardOutput()).normalized(QString::NormalizationForm_KD).simplified();
    assert(pdfText.contains("definiciones DTC experimentales no verificadas"));
    protocol.current(true);
    assert(widget.result().status == VehicleReport::QueryStatus::Failed);
    assert(widget.stopDCreading());
    assert(widget.startDCreading());
    protocol.current();
    protocol.historic();
    assert(widget.result().status == VehicleReport::QueryStatus::Complete);
    protocol.invalid();
    assert(widget.result().status == VehicleReport::QueryStatus::Failed);
    assert(widget.stopDCreading());
    std::cout << "Qt capture: pending, complete and system check incomplete OK\n";
}
