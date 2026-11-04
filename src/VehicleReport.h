/* VehicleReport: structured vehicle inspection data and JSON export.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef VEHICLEREPORT_H
#define VEHICLEREPORT_H

#include <string>
#include <vector>

namespace VehicleReport {

enum class QueryStatus { Complete, Failed, Unsupported, NotQueried };
enum class CodeCategory {
    Current, Temporary, Historic, Memorized,
    CurrentOrTemporary, HistoricOrMemorized, Unknown
};

// For VIN or other optional readings, an empty value in a Complete field means
// the source returned an empty value; other statuses carry no value.
struct Reading {
    QueryStatus status = QueryStatus::NotQueried;
    std::string value;
    std::string source;
    std::string detail;
};

struct DiagnosticCode {
    std::string code;
    std::string description;
    CodeCategory category = CodeCategory::Unknown;
};

struct DtcCoverage {
    CodeCategory category = CodeCategory::Unknown;
    QueryStatus status = QueryStatus::NotQueried;
    std::string detail;
};

struct Module {
    std::string name;
    QueryStatus status = QueryStatus::NotQueried;
    std::string source;
    std::string detail;
    Reading romId;
    bool unverifiedDtcDefinitions = false; // Explicit experimental definition, not a general validation claim.
    std::vector<DiagnosticCode> codes;
    std::vector<DtcCoverage> coverage;
};

struct Report {
    bool simulated = false;
    std::string applicationVersion;
    std::string capturedAt; // ISO 8601 with timezone; set by the caller.
    std::string declaredModel;
    int declaredYear = 0; // 0 means not supplied.
    long long dashboardKm = -1; // -1 means not supplied.
    Reading vin;
    std::vector<Module> modules;
};

// Returns false and sets error for inconsistent data. On failure, json is not
// modified. Strings supplied by the caller must be valid UTF-8.
bool toJson(const Report& report, std::string* json, std::string* error);

} // namespace VehicleReport
#endif
