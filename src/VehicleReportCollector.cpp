/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "VehicleReportCollector.h"

namespace VehicleReport {
Collector::Collector(const std::string& name, const std::string& source,
                     bool currentSupported, bool historySupported) {
    module_.name = name;
    module_.source = source;
    DtcCoverage current;
    current.category = CodeCategory::CurrentOrTemporary;
    current.status = currentSupported ? QueryStatus::NotQueried : QueryStatus::Unsupported;
    module_.coverage.push_back(current);
    DtcCoverage history;
    history.category = CodeCategory::HistoricOrMemorized;
    history.status = historySupported ? QueryStatus::NotQueried : QueryStatus::Unsupported;
    module_.coverage.push_back(history);
}
void Collector::setRomId(const Reading& value) { module_.romId = value; }
bool Collector::accept(CodeCategory category, const std::vector<DiagnosticCode>& codes) {
    if (failed_) return false;
    std::size_t index;
    if (category == CodeCategory::CurrentOrTemporary) index = 0;
    else if (category == CodeCategory::HistoricOrMemorized) index = 1;
    else return false;
    if (module_.coverage[index].status == QueryStatus::Unsupported) return false;
    for (const DiagnosticCode& code : codes)
        if (code.code.empty() || code.category != category) return false;
    if (index == 0) currentCodes_ = codes;
    else historyCodes_ = codes;
    module_.coverage[index].status = QueryStatus::Complete;
    return true;
}
void Collector::fail(const std::string& detail) {
    failed_ = true;
    failureDetail_ = detail;
    for (DtcCoverage& group : module_.coverage)
        if (group.status == QueryStatus::NotQueried) {
            group.status = QueryStatus::Failed;
            group.detail = detail;
        }
}
Module Collector::result() const {
    Module output = module_;
    if (failed_) {
        output.status = QueryStatus::Failed;
        output.detail = failureDetail_;
        return output;
    }
    bool supported = false;
    for (const DtcCoverage& group : output.coverage) {
        if (group.status == QueryStatus::NotQueried) return output;
        if (group.status == QueryStatus::Complete) supported = true;
    }
    if (!supported) {
        output.status = QueryStatus::Unsupported;
        return output;
    }
    output.status = QueryStatus::Complete;
    output.codes = currentCodes_;
    output.codes.insert(output.codes.end(), historyCodes_.begin(), historyCodes_.end());
    return output;
}
}
