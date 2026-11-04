/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "VehicleReport.h"
#include <sstream>

namespace VehicleReport {
namespace {
const char* statusName(QueryStatus value) {
    switch (value) {
    case QueryStatus::Complete: return "completa";
    case QueryStatus::Failed: return "fallida";
    case QueryStatus::Unsupported: return "no_soportada";
    case QueryStatus::NotQueried: return "no_consultada";
    }
    return nullptr;
}
const char* categoryName(CodeCategory value) {
    switch (value) {
    case CodeCategory::Current: return "actual";
    case CodeCategory::Temporary: return "temporal";
    case CodeCategory::Historic: return "historico";
    case CodeCategory::Memorized: return "memorizado";
    case CodeCategory::CurrentOrTemporary: return "actual_o_temporal";
    case CodeCategory::HistoricOrMemorized: return "historico_o_memorizado";
    case CodeCategory::Unknown: return "desconocida";
    }
    return nullptr;
}
void quoted(std::ostringstream& out, const std::string& value) {
    static const char hex[] = "0123456789abcdef";
    out << '"';
    for (unsigned char c : value) {
        switch (c) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20) out << "\\u00" << hex[c >> 4] << hex[c & 15];
            else out << static_cast<char>(c);
        }
    }
    out << '"';
}
void reading(std::ostringstream& out, const Reading& value) {
    out << "{\"estado\":"; quoted(out, statusName(value.status));
    out << ",\"valor\":";
    if (value.status == QueryStatus::Complete) quoted(out, value.value);
    else out << "null";
    out << ",\"fuente\":"; quoted(out, value.source);
    out << ",\"detalle\":"; quoted(out, value.detail);
    out << '}';
}
bool validateReading(const Reading& value) {
    return statusName(value.status) &&
        (value.status == QueryStatus::Complete || value.value.empty());
}
bool fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}
} // namespace

bool toJson(const Report& report, std::string* json, std::string* error) {
    if (!json) return fail(error, "JSON output is null");
    if (!validateReading(report.vin)) return fail(error, "Invalid VIN reading");
    if (report.declaredYear < 0 || report.dashboardKm < -1)
        return fail(error, "Invalid manually supplied value");
    for (const Module& module : report.modules) {
        if (module.name.empty() || !statusName(module.status) || !validateReading(module.romId))
            return fail(error, "Invalid module or ROM ID");
        if (module.status != QueryStatus::Complete && !module.codes.empty())
            return fail(error, "Codes require a complete module query");
        bool anyGroupComplete = false;
        for (const DtcCoverage& group : module.coverage) {
            if (!categoryName(group.category) || !statusName(group.status))
                return fail(error, "Invalid DTC group coverage");
            if (group.status == QueryStatus::Complete) anyGroupComplete = true;
            if (module.status == QueryStatus::Complete &&
                group.status != QueryStatus::Complete && group.status != QueryStatus::Unsupported)
                return fail(error, "Complete module has incomplete DTC group");
        }
        if (module.status == QueryStatus::Complete && !module.coverage.empty() && !anyGroupComplete)
            return fail(error, "Complete module has no completed DTC group");
        for (const DiagnosticCode& code : module.codes) {
            if (code.code.empty() || !categoryName(code.category))
                return fail(error, "Invalid diagnostic code");
        }
    }
    std::ostringstream out;
    out << "{\"formato_version\":\"1.0\",\"fecha_captura\":";
    if (report.capturedAt.empty()) out << "null"; else quoted(out, report.capturedAt);
    out << ",\"vehiculo\":{\"modelo_declarado\":";
    if (report.declaredModel.empty()) out << "null"; else quoted(out, report.declaredModel);
    out << ",\"anio_declarado\":";
    if (report.declaredYear == 0) out << "null"; else out << report.declaredYear;
    out << ",\"odometro_tablero_km\":";
    if (report.dashboardKm == -1) out << "null"; else out << report.dashboardKm;
    out << ",\"vin\":"; reading(out, report.vin);
    out << "},\"modulos\":[";
    for (std::size_t i = 0; i < report.modules.size(); ++i) {
        if (i) out << ',';
        const Module& module = report.modules[i];
        out << "{\"nombre\":"; quoted(out, module.name);
        out << ",\"estado_consulta\":"; quoted(out, statusName(module.status));
        out << ",\"fuente\":"; quoted(out, module.source);
        out << ",\"detalle\":"; quoted(out, module.detail);
        out << ",\"rom_id\":"; reading(out, module.romId);
        out << ",\"cobertura_dtc\":[";
        for (std::size_t j = 0; j < module.coverage.size(); ++j) {
            if (j) out << ',';
            const DtcCoverage& group = module.coverage[j];
            out << "{\"categoria\":"; quoted(out, categoryName(group.category));
            out << ",\"estado\":"; quoted(out, statusName(group.status));
            out << ",\"detalle\":"; quoted(out, group.detail);
            out << '}';
        }
        out << "]";
        out << ",\"dtc\":";
        if (module.status != QueryStatus::Complete) out << "null";
        else {
            out << '[';
            for (std::size_t j = 0; j < module.codes.size(); ++j) {
                if (j) out << ',';
                const DiagnosticCode& code = module.codes[j];
                out << "{\"codigo\":"; quoted(out, code.code);
                out << ",\"descripcion\":"; quoted(out, code.description);
                out << ",\"categoria\":"; quoted(out, categoryName(code.category));
                out << '}';
            }
            out << ']';
        }
        out << '}';
    }
    out << "]}\n";
    *json = out.str();
    if (error) error->clear();
    return true;
}
} // namespace VehicleReport
