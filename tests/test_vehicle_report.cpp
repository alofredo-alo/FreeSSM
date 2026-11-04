// SPDX-License-Identifier: GPL-3.0-or-later
#include "VehicleReport.h"
#include "VehicleReportCollector.h"
#include <cassert>
#include <iostream>
#include <string>

using namespace VehicleReport;

int main() {
    Report report;
    report.capturedAt = "2026-11-04T12:30:00-03:00";
    report.declaredModel = "Outback \"prueba\"\nñ";
    report.declaredYear = 2010;
    report.dashboardKm = 185430;
    // VIN remains NotQueried, so it must be encoded as null.
    Module engine;
    engine.name = "motor";
    engine.status = QueryStatus::Complete;
    engine.source = "SSM";
    engine.romId.status = QueryStatus::Complete;
    engine.romId.value = "B1AB217100";
    engine.romId.source = "SSM";
    report.modules.push_back(engine);
    Module transmission;
    transmission.name = "transmision";
    transmission.status = QueryStatus::Complete;
    transmission.source = "SSM";
    DiagnosticCode code;
    code.code = "P0001";
    code.description = std::string("Descripción\t\\") + char(1);
    code.category = CodeCategory::CurrentOrTemporary;
    transmission.codes.push_back(code);
    report.modules.push_back(transmission);
    Module failed;
    failed.name = "abs";
    failed.status = QueryStatus::Failed;
    failed.detail = "Sin respuesta";
    report.modules.push_back(failed);
    Module unsupported;
    unsupported.name = "airbag";
    unsupported.status = QueryStatus::Unsupported;
    report.modules.push_back(unsupported);
    Module missing;
    missing.name = "tablero";
    report.modules.push_back(missing);

    report.applicationVersion = "1.3.0-beta.1-candidate";
    Collector collector("motor", "SSM", true, true);
    collector.setUnverifiedDtcDefinitions(true);
    assert(collector.result().status == QueryStatus::NotQueried);
    std::vector<DiagnosticCode> empty;
    assert(collector.accept(CodeCategory::CurrentOrTemporary, empty));
    assert(collector.result().status == QueryStatus::NotQueried);
    assert(collector.accept(CodeCategory::HistoricOrMemorized, empty));
    assert(collector.result().status == QueryStatus::Complete);
    report.modules[0] = collector.result();
    report.modules[0].romId = engine.romId;
    Collector partial("motor", "SSM", true, true);
    assert(partial.accept(CodeCategory::CurrentOrTemporary, empty));
    partial.fail("respuesta incompleta");
    assert(partial.result().status == QueryStatus::Failed);
    assert(partial.result().coverage[0].status == QueryStatus::Complete);
    assert(partial.result().coverage[1].status == QueryStatus::Failed);
    assert(!partial.accept(CodeCategory::HistoricOrMemorized, empty));
    Collector unsupportedGroups("motor", "SSM", false, false);
    assert(unsupportedGroups.result().status == QueryStatus::Unsupported);

    std::string json, error;
    assert(toJson(report, &json, &error));
    assert(error.empty());
    std::cout << json;

    // Incomplete or failed reads must never silently become empty DTC lists.
    Report invalid = report;
    invalid.modules[2].codes.push_back(code);
    std::string unchanged = "unchanged";
    assert(!toJson(invalid, &unchanged, &error));
    assert(unchanged == "unchanged" && !error.empty());
    invalid = report;
    invalid.vin.value = "inconsistent";
    assert(!toJson(invalid, &unchanged, &error));
    invalid = report;
    invalid.modules[0].status = static_cast<QueryStatus>(99);
    assert(!toJson(invalid, &unchanged, &error));
    invalid = report;
    invalid.dashboardKm = -2;
    assert(!toJson(invalid, &unchanged, &error));
    assert(!toJson(report, nullptr, &error));
}
