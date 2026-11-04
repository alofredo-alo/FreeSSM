/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MEASUREMENTFILE_H
#define MEASUREMENTFILE_H
#include <QString>
namespace MeasurementFile {
// Reserves a unique CSV path in directory without truncating any existing file.
bool reserve(const QString& directory, QString* path, QString* error);
}
#endif
