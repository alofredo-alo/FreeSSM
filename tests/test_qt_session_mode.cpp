// SPDX-License-Identifier: GPL-3.0-or-later
#include "SessionMode.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileDialog>
#include <QFile>
#include <QKeyEvent>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <cassert>
#include <iostream>

static void choose(const char* objectName) {
    QTimer::singleShot(0, [objectName]() {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog && dialog->objectName() == "sessionModeSelector");
        auto* button = dialog->findChild<QPushButton*>(objectName);
        assert(button); button->click();
    });
}
static void confirm(QMessageBox::StandardButton choice) {
    QTimer::singleShot(0, [choice]() {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        assert(box && box->objectName() == "confirmFullMode");
        assert(box->defaultButton() == box->button(QMessageBox::No));
        box->button(choice)->click();
    });
}
int main(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    choose("modeCancel"); assert(SessionMode::select() == SessionMode::Mode::Cancelled);
    QTimer::singleShot(0, []() { QApplication::activeModalWidget()->close(); });
    assert(SessionMode::select() == SessionMode::Mode::Cancelled);
    QTimer::singleShot(0, []() {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(dialog);
        auto* button = dialog->findChild<QPushButton*>("modeDemo");
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(button, &press);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(button, &release);
        if (dialog->isVisible()) dialog->reject();
    });
    assert(SessionMode::select() == SessionMode::Mode::Cancelled);
    choose("modeDemo"); assert(SessionMode::select() == SessionMode::Mode::Demo);
    choose("modeReadOnly"); assert(SessionMode::select() == SessionMode::Mode::ReadOnly);
    choose("modeFull"); assert(SessionMode::select() == SessionMode::Mode::Full);
    confirm(QMessageBox::No); assert(!SessionMode::confirmFull());
    confirm(QMessageBox::Yes); assert(SessionMode::confirmFull());
    std::cout << "Selector y confirmacion de modos OK\n";
}
