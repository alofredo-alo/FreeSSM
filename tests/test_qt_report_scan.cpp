// SPDX-License-Identifier: GPL-3.0-or-later
#include "VehicleReportScan.h"
#include "SerialPassThroughDiagInterface.h"
#include "DiagnosticSafety.h"
#include <QApplication>
#include <iostream>
#include <string>
int main(int argc, char** argv) {
    if ((argc != 2 && argc != 3) || std::string(argv[1]).find("/dev/pts/") != 0) return 2;
    const bool missingTransmission = argc == 3 && std::string(argv[2]) == "--missing-transmission";
    QApplication app(argc, argv);
    const auto unavailable = VehicleReport::scanEngineAndTransmission(nullptr, "en", true);
    if (unavailable.modules.size() != 5 ||
        unavailable.modules[0].status != VehicleReport::QueryStatus::Failed ||
        unavailable.modules[1].status != VehicleReport::QueryStatus::Failed ||
        unavailable.modules[2].status != VehicleReport::QueryStatus::NotQueried) return 7;
    SerialPassThroughDiagInterface iface;
    if (!iface.open(argv[1])) return 3;
    DiagnosticSafety::setReadOnly(false);
    const auto report = VehicleReport::scanEngineAndTransmission(&iface, "en", true);
    if (DiagnosticSafety::isReadOnly() || iface.isConnected() || report.modules.size() != 5 ||
        report.modules[0].name != "motor" || report.modules[1].name != "transmision" ||
        report.modules[0].status != VehicleReport::QueryStatus::Complete ||
        report.modules[1].status != (missingTransmission ? VehicleReport::QueryStatus::Failed : VehicleReport::QueryStatus::Complete) ||
        report.vin.value != "JF1ABCDEFG1234567") {
        for (const auto& module : report.modules) std::cerr << module.name << " " << static_cast<int>(module.status) << " " << module.detail << '\n';
        std::cerr << "VIN: " << report.vin.value << '\n';
        return 4;
    }
    if (report.modules[0].codes.empty() || (!missingTransmission && report.modules[1].codes.empty()) ||
        (missingTransmission && !report.modules[1].codes.empty())) return 5;
    std::string json, error;
    if (!VehicleReport::toJson(report, &json, &error)) { std::cerr << error << '\n'; return 6; }
    std::cout << "Combined SSM2 engine/transmission scan and read-only restore OK\n";
}
