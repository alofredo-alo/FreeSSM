/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VEHICLEREPORTPDF_H
#define VEHICLEREPORTPDF_H
#include "VehicleReport.h"
#include <QString>
namespace VehicleReport {
// Generates a PDF at the user-selected path; the caller handles overwrite confirmation.
bool writePdf(const Report& report, const QString& path, QString* error);
}
#endif
