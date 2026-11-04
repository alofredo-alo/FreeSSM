/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "VehicleReportPdf.h"
#include <QFileInfo>
#include <QPrinter>
#include <QTextDocument>

namespace VehicleReport {
namespace {
QString utf8(const std::string& value) { return QString::fromUtf8(value.data(), static_cast<int>(value.size())); }
QString html(const std::string& value) { return utf8(value).toHtmlEscaped(); }
QString status(QueryStatus s) {
    switch (s) {
    case QueryStatus::Complete: return QString::fromUtf8("Completada");
    case QueryStatus::Failed: return QString::fromUtf8("Fallida");
    case QueryStatus::Unsupported: return QString::fromUtf8("No soportada");
    case QueryStatus::NotQueried: return QString::fromUtf8("No consultada");
    }
    return QString::fromUtf8("Desconocida");
}
QString category(CodeCategory c) {
    switch (c) {
    case CodeCategory::Current: return QString::fromUtf8("Actual");
    case CodeCategory::Temporary: return QString::fromUtf8("Temporal");
    case CodeCategory::Historic: return QString::fromUtf8("Histórico");
    case CodeCategory::Memorized: return QString::fromUtf8("Memorizado");
    case CodeCategory::CurrentOrTemporary: return QString::fromUtf8("Actual o temporal");
    case CodeCategory::HistoricOrMemorized: return QString::fromUtf8("Histórico o memorizado");
    case CodeCategory::Unknown: return QString::fromUtf8("Desconocida");
    }
    return QString::fromUtf8("Desconocida");
}
QString reading(const Reading& r) {
    if (r.status == QueryStatus::Complete)
        return r.value.empty() ? QString::fromUtf8("Sin valor reportado") : html(r.value);
    return status(r.status);
}
}
bool writePdf(const Report& report, const QString& path, QString* error) {
    std::string json, validation;
    if (!toJson(report, &json, &validation)) {
        if (error) *error = QString::fromStdString(validation);
        return false;
    }
    if (path.isEmpty()) { if (error) *error = QString::fromUtf8("Destino vacío"); return false; }
    QString body = QString::fromUtf8("<h1>Informe de estado del vehículo</h1>");
    body += QString::fromUtf8("<p><b>Fecha de exportación:</b> ") + html(report.capturedAt) + "</p>";
    if (!report.declaredModel.empty()) body += QString::fromUtf8("<p><b>Modelo declarado:</b> ") + html(report.declaredModel) + "</p>";
    if (report.declaredYear) body += QString::fromUtf8("<p><b>Año declarado:</b> ") + QString::number(report.declaredYear) + "</p>";
    body += QString::fromUtf8("<p><b>Kilometraje del tablero declarado:</b> ");
    body += report.dashboardKm < 0 ? QString::fromUtf8("No informado") : QString::number(report.dashboardKm) + " km";
    body += QString::fromUtf8("</p><p><b>VIN:</b> ") + reading(report.vin) + "</p>";
    for (const Module& module : report.modules) {
        body += "<h2>" + html(module.name) + "</h2>";
        body += QString::fromUtf8("<p><b>Consulta:</b> ") + status(module.status) + "</p>";
        if (!module.source.empty()) body += QString::fromUtf8("<p><b>Fuente:</b> ") + html(module.source) + "</p>";
        body += QString::fromUtf8("<p><b>ROM-ID:</b> ") + reading(module.romId) + "</p>";
        if (!module.detail.empty()) body += QString::fromUtf8("<p><b>Detalle:</b> ") + html(module.detail) + "</p>";
        for (const DtcCoverage& group : module.coverage)
            body += QString::fromUtf8("<p><b>Cobertura ") + category(group.category) + ":</b> " + status(group.status) + "</p>";
        if (module.status != QueryStatus::Complete)
            body += QString::fromUtf8("<p>No existe un resultado completo de códigos para este módulo.</p>");
        else if (module.codes.empty())
            body += QString::fromUtf8("<p>Sin códigos reportados en los grupos consultados.</p>");
        else for (const DiagnosticCode& code : module.codes)
            body += "<p><b>" + html(code.code) + "</b> — " + category(code.category) + ": " + html(code.description) + "</p>";
    }
    body += QString::fromUtf8("<p>El kilometraje declarado no está contrastado electrónicamente. Los módulos no consultados no forman parte del diagnóstico.</p><p>Los códigos almacenados disponibles no constituyen un historial completo del vehículo.</p>");
    QTextDocument document;
    document.setHtml(body);
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    document.print(&printer);
    const QFileInfo output(path);
    if (printer.printerState() == QPrinter::Error || !output.exists() || output.size() == 0) {
        if (error) *error = QString::fromUtf8("No se pudo generar el PDF.");
        return false;
    }
    if (error) error->clear();
    return true;
}
}
