/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "MeasurementFile.h"
#include <QDateTime>
#include <QDir>
#include <QTemporaryFile>
namespace MeasurementFile {
bool reserve(const QString& directory, QString* path, QString* error) {
    if (!path) return false;
    const QString prefix = "measurements_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_XXXXXX.csv";
    QTemporaryFile file(QDir(directory).filePath(prefix));
    if (!file.open()) {
        if (error) *error = file.errorString();
        return false;
    }
    file.setAutoRemove(false);
    *path = file.fileName();
    file.close();
    if (error) error->clear();
    return true;
}
}
