// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SESSIONMODE_H
#define SESSIONMODE_H
#include <QDialog>
class QWidget;
namespace SessionMode {
enum class Mode { Cancelled, Demo, ReadOnly, Full };
// A closing or rejected selector never chooses a mode.
Mode select(QWidget* parent = nullptr);
bool confirmFull(QWidget* parent = nullptr);
}
#endif
