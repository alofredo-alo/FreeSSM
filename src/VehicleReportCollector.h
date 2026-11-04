/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VEHICLEREPORTCOLLECTOR_H
#define VEHICLEREPORTCOLLECTOR_H
#include "VehicleReport.h"

namespace VehicleReport {
// Accumulates independently received SSM DTC groups. The caller decides which
// groups are supported; a group is complete only after a genuine response.
class Collector {
public:
    Collector(const std::string& moduleName, const std::string& source,
              bool currentSupported, bool historySupported);
    void setRomId(const Reading& romId);
    void setUnverifiedDtcDefinitions(bool unverified);
    bool accept(CodeCategory category, const std::vector<DiagnosticCode>& codes);
    void fail(const std::string& detail);
    Module result() const;
private:
    Module module_;
    std::vector<DiagnosticCode> currentCodes_;
    std::vector<DiagnosticCode> historyCodes_;
    bool failed_ = false;
    std::string failureDetail_;
};
}
#endif
