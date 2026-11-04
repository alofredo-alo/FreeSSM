/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "VehicleReportScan.h"
#include "SSMprotocol2.h"
#include "VehicleReportCollector.h"
#include "DiagnosticSafety.h"
#include <QApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QProgressDialog>
#include <QTimer>
#include <memory>

namespace VehicleReport {
namespace {
class ReadOnlyGuard {
public:
    ReadOnlyGuard() : previous_(DiagnosticSafety::isReadOnly()) { DiagnosticSafety::setReadOnly(true); }
    ~ReadOnlyGuard() { DiagnosticSafety::setReadOnly(previous_); }
private: bool previous_;
};
std::vector<DiagnosticCode> convert(const QStringList& codes, const QStringList& descriptions, CodeCategory category) {
    std::vector<DiagnosticCode> result;
    for (int i = 0; i < codes.size(); ++i) {
        DiagnosticCode item;
        item.code = codes[i].toUtf8().constData();
        item.description = descriptions[i].toUtf8().constData();
        item.category = category;
        result.push_back(item);
    }
    return result;
}
Module scanOne(AbstractDiagInterface* iface, const QString& language, bool preferKLine,
               CUtype type, Reading* vin) {
    Module failure;
    failure.name = type == CUtype::Engine ? "motor" : "transmision";
    failure.source = "SSM2";
    failure.status = QueryStatus::Failed;
    failure.detail = "No se pudo conectar o identificar el módulo mediante SSM2";
    using P = AbstractDiagInterface::protocol_type;
    const P first = preferKLine ? P::SSM2_ISO14230 : P::SSM2_ISO15765;
    const P second = preferKLine ? P::SSM2_ISO15765 : P::SSM2_ISO14230;
    std::unique_ptr<SSMprotocol2> protocol;
    for (const P candidate : {first, second}) {
        if (!iface->connect(candidate)) continue;
        std::unique_ptr<SSMprotocol2> attempt(new SSMprotocol2(iface, language));
        const auto result = attempt->setupCUdata(type, false, true);
        if (result == SSMprotocol::result_success) { protocol = std::move(attempt); break; }
        if (result == SSMprotocol::result_noDefs || result == SSMprotocol::result_noOrInvalidDefsFile) {
            failure.detail = "Módulo identificado, sin definiciones de DTC disponibles";
            failure.romId.status = QueryStatus::Complete;
            failure.romId.value = attempt->getROMID();
            failure.romId.source = "SSM2";
            attempt.reset();
            iface->disconnect();
            return failure;
        }
        attempt.reset();
        iface->disconnect();
    }
    if (!protocol) return failure;
    Reading rom;
    rom.value = protocol->getROMID();
    rom.status = rom.value.empty() ? QueryStatus::NotQueried : QueryStatus::Complete;
    rom.source = "SSM2";
    if (type == CUtype::Engine && vin) {
        bool supported = false;
        if (protocol->hasVINsupport(&supported) && !supported) {
            vin->status = QueryStatus::Unsupported;
            vin->source = "SSM2";
        } else if (supported && iface->protocolType() == P::SSM2_ISO14230) {
            QString value;
            if (protocol->getVIN(&value) && !value.isEmpty()) {
                vin->status = QueryStatus::Complete;
                vin->value = value.toUtf8().constData();
                vin->source = "SSM2";
            }
        }
    }
    int groups = 0;
    if (!protocol->getSupportedDCgroups(&groups)) {
        failure.romId = rom;
        failure.detail = "No se pudo obtener la cobertura de códigos";
        protocol.reset(); iface->disconnect(); return failure;
    }
    const bool current = groups & (SSMprotocol::currentDTCs_DCgroup | SSMprotocol::temporaryDTCs_DCgroup);
    const bool history = groups & (SSMprotocol::historicDTCs_DCgroup | SSMprotocol::memorizedDTCs_DCgroup);
    Collector collector(failure.name, "SSM2", current, history);
    collector.setRomId(rom);
    if (!current && !history) {
        Module output = collector.result();
        protocol.reset(); iface->disconnect(); return output;
    }
    QEventLoop loop;
    bool communicationFailed = false;
    QObject::connect(protocol.get(), &SSMprotocol::currentOrTemporaryDTCs, &loop,
        [&](QStringList codes, QStringList descriptions, bool, bool dcheck) {
            if (!current) return;
            if (dcheck || codes.size() != descriptions.size() ||
                !collector.accept(CodeCategory::CurrentOrTemporary, convert(codes, descriptions, CodeCategory::CurrentOrTemporary))) {
                collector.fail(dcheck ? "Comprobación del sistema incompleta" : "Respuesta DTC inválida");
                loop.quit(); return;
            }
            if (collector.result().status == QueryStatus::Complete) loop.quit();
        });
    QObject::connect(protocol.get(), &SSMprotocol::historicOrMemorizedDTCs, &loop,
        [&](QStringList codes, QStringList descriptions) {
            if (!history) return;
            if (codes.size() != descriptions.size() ||
                !collector.accept(CodeCategory::HistoricOrMemorized, convert(codes, descriptions, CodeCategory::HistoricOrMemorized))) {
                collector.fail("Respuesta DTC inválida"); loop.quit(); return;
            }
            if (collector.result().status == QueryStatus::Complete) loop.quit();
        });
    QObject::connect(protocol.get(), &SSMprotocol::commError, &loop, [&]() {
        communicationFailed = true; collector.fail("Error de comunicación"); loop.quit();
    });
    if (!protocol->startDCreading(groups)) collector.fail("No se pudo iniciar la lectura de DTC");
    else {
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(15000);
        if (collector.result().status == QueryStatus::NotQueried && !communicationFailed) loop.exec();
        if (collector.result().status == QueryStatus::NotQueried) collector.fail("Tiempo de espera agotado");
        if (!protocol->stopDCreading()) collector.fail("No se pudo detener la lectura de DTC");
    }
    const Module output = collector.result();
    protocol.reset();
    iface->disconnect();
    return output;
}
}
Report scanEngineAndTransmission(AbstractDiagInterface* iface, const QString& language,
                                 bool preferKLine, QWidget* parent) {
    ReadOnlyGuard guard;
    Report report;
    report.capturedAt = QDateTime::currentDateTime().toString(Qt::ISODate).toStdString();
    if (!iface || !iface->isOpen() || iface->isConnected()) {
        for (const char* name : {"motor", "transmision"}) {
            Module m; m.name = name; m.source = "SSM2"; m.status = QueryStatus::Failed;
            m.detail = "Interfaz no disponible o ya conectada";
            report.modules.push_back(m);
        }
        for (const char* name : {"abs_vdc", "airbag", "tablero"}) {
            Module m; m.name = name; report.modules.push_back(m);
        }
        return report;
    }
    QProgressDialog progress(QCoreApplication::translate("FreeSSM", "Reading engine..."), QString(), 0, 0, parent);
    progress.setWindowTitle(QCoreApplication::translate("FreeSSM", "Vehicle report"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    progress.show();
    QApplication::processEvents();
    report.modules.push_back(scanOne(iface, language, preferKLine, CUtype::Engine, &report.vin));
    progress.setLabelText(QCoreApplication::translate("FreeSSM", "Reading transmission..."));
    QApplication::processEvents();
    report.modules.push_back(scanOne(iface, language, preferKLine, CUtype::Transmission, nullptr));
    for (const char* name : {"abs_vdc", "airbag", "tablero"}) {
        Module m; m.name = name; report.modules.push_back(m);
    }
    return report;
}
}
