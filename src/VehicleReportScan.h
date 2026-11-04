/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VEHICLEREPORTSCAN_H
#define VEHICLEREPORTSCAN_H
#include "VehicleReport.h"
#include <QString>
class AbstractDiagInterface;
class QWidget;
namespace VehicleReport {
// Uses SSM2 read services only. The caller owns an open, disconnected interface.
Report scanEngineAndTransmission(AbstractDiagInterface* interface, const QString& language,
                                 bool preferKLine, QWidget* parent = nullptr);
}
#endif
