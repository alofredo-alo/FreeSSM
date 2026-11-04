// SPDX-License-Identifier: GPL-3.0-or-later
#include "SessionMode.h"
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace SessionMode {
Mode select(QWidget* parent) {
    QDialog dialog(parent);
    dialog.setObjectName("sessionModeSelector");
    dialog.setWindowTitle(QString::fromUtf8("FreeSSM beta — Seleccionar modo"));
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    QLabel* title = new QLabel(QString::fromUtf8("<h2>Selecciona un modo para esta sesión</h2>"), &dialog);
    title->setWordWrap(true);
    layout->addWidget(title);
    QLabel* beta = new QLabel(QString::fromUtf8("Versión beta: la compatibilidad y los resultados necesitan validación. El informe no certifica el estado del vehículo."), &dialog);
    beta->setWordWrap(true);
    layout->addWidget(beta);
    QLabel* details = new QLabel(QString::fromUtf8("<a href=\"https://github.com/alofredo-alo/FreeSSM#informe-de-estado-del-veh%C3%ADculo\">Consultar limitaciones</a>"), &dialog);
    details->setOpenExternalLinks(true);
    layout->addWidget(details);
    auto add = [&](const QString& text, const char* name, int code) {
        QPushButton* button = new QPushButton(text, &dialog);
        button->setObjectName(name);
        button->setAutoDefault(false);
        button->setDefault(false);
        layout->addWidget(button);
        QObject::connect(button, &QPushButton::clicked, &dialog, [&dialog, code]() { dialog.done(code); });
    };
    add(QString::fromUtf8("Demo — datos simulados, sin vehículo"), "modeDemo", 10);
    add(QString::fromUtf8("Solo lectura — recomendado"), "modeReadOnly", 11);
    add(QString::fromUtf8("Diagnóstico completo — permite cambios"), "modeFull", 12);
    add(QString::fromUtf8("Cancelar"), "modeCancel", QDialog::Rejected);
    dialog.resize(500, 260);
    const int result = dialog.exec();
    if (result == 10) return Mode::Demo;
    if (result == 11) return Mode::ReadOnly;
    if (result == 12) return Mode::Full;
    return Mode::Cancelled;
}
bool confirmFull(QWidget* parent) {
    QMessageBox warning(QMessageBox::Warning, QString::fromUtf8("Diagnóstico completo — versión beta"),
        QString::fromUtf8("Este modo permite borrar códigos, modificar ajustes y activar componentes del vehículo. "
                          "Usa estas funciones solo si conoces sus efectos. ¿Deseas habilitar el diagnóstico completo?"),
        QMessageBox::Yes | QMessageBox::No, parent);
    warning.setObjectName("confirmFullMode");
    warning.setDefaultButton(QMessageBox::No);
    return warning.exec() == QMessageBox::Yes;
}
}
