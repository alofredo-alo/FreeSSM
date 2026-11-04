// SPDX-License-Identifier: GPL-3.0-or-later
// Drives the actual main function through Qt's event loop without hardware.
#include <QApplication>
#include <QDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <cassert>

int freessmMain(int argc, char** argv);
static void closeNextWindow() {
    const QByteArray path = qgetenv("FREESSM_STARTUP_TEST");
    if (path == "demo" || path == "cli-demo" || path == "gui-engine" || path == "gui-transmission") {
        QWidget* mainWindow = nullptr;
        for (QWidget* candidate : QApplication::topLevelWidgets()) {
            if (candidate->objectName() == "FreeSSM_MainWindow") mainWindow = candidate;
        }
        assert(mainWindow && mainWindow->isVisible());
        assert(mainWindow->windowTitle().contains("DATOS SIMULADOS"));
        assert(mainWindow->findChild<QPushButton*>("engine_pushButton")->isEnabled());
        assert(mainWindow->findChild<QPushButton*>("transmission_pushButton")->isEnabled());
        assert(!mainWindow->findChild<QPushButton*>("preferences_pushButton")->isEnabled());
        assert(!mainWindow->findChild<QPushButton*>("absvdc_pushButton")->isEnabled());
        if (path == "gui-engine" || path == "gui-transmission") {
            bool closedDialog = false;
            QTimer::singleShot(2500, [&closedDialog]() {
                for (QWidget* item : QApplication::topLevelWidgets()) {
                    if (auto* dialog = qobject_cast<QDialog*>(item)) {
                        if (dialog->isVisible() && dialog->windowTitle().contains("DATOS SIMULADOS")) {
                            closedDialog = true; dialog->close(); return;
                        }
                    }
                }
            });
            const char* buttonName = path == "gui-engine" ? "engine_pushButton" : "transmission_pushButton";
            mainWindow->findChild<QPushButton*>(buttonName)->click();
            assert(closedDialog);
        }
        mainWindow->close();
    } else {
        QWidget* widget = QApplication::activeModalWidget();
        auto* warning = qobject_cast<QMessageBox*>(widget);
        assert(warning && warning->objectName() == "confirmFullMode");
        warning->button(QMessageBox::No)->click();
    }
}
static void driveStartup() {
    QTimer::singleShot(0, []() {
        const QByteArray path = qgetenv("FREESSM_STARTUP_TEST");
        if (path == "cli-full" || path == "cli-demo") { closeNextWindow(); return; }
        auto* selector = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        assert(selector && selector->objectName() == "sessionModeSelector");
        if (path == "cancel") { selector->reject(); return; }
        const char* button = (path == "demo" || path == "gui-engine" || path == "gui-transmission") ? "modeDemo" : "modeFull";
        auto* choice = selector->findChild<QPushButton*>(button);
        assert(choice);
        QTimer::singleShot(0, closeNextWindow);
        choice->click();
    });
}
Q_COREAPP_STARTUP_FUNCTION(driveStartup)
int main(int argc, char** argv) { return freessmMain(argc, argv); }
