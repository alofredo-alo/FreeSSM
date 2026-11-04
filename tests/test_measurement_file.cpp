// SPDX-License-Identifier: GPL-3.0-or-later
#include "MeasurementFile.h"
#include <QTemporaryDir>
#include <QFile>
#include <cassert>
#include <iostream>
int main() {
    QTemporaryDir directory;
    assert(directory.isValid());
    QFile old(directory.path() + "/measurements.csv");
    assert(old.open(QIODevice::WriteOnly));
    assert(old.write("existing") == 8);
    old.close();
    QString first, second, error;
    assert(MeasurementFile::reserve(directory.path(), &first, &error));
    assert(MeasurementFile::reserve(directory.path(), &second, &error));
    assert(first != second && QFile::exists(first) && QFile::exists(second));
    assert(old.open(QIODevice::ReadOnly));
    assert(old.readAll() == "existing");
    assert(!MeasurementFile::reserve(directory.path() + "/no-such-dir", &second, &error));
    assert(!error.isEmpty());
    std::cout << "Unique measurement files and existing CSV preservation OK\n";
}
