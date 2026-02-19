// Copyright (C) 2025 Signal Slot Inc.
// SPDX-License-Identifier: BSD-3-Clause

#include "mainwindow.h"

#include <QtWidgets/QApplication>

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("Signal Slot Inc."_L1);
    QCoreApplication::setOrganizationDomain("signal-slot.co.jp"_L1);
    QCoreApplication::setApplicationName("psdcreator"_L1);
    QCoreApplication::setApplicationVersion("0.1.0"_L1);

    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    return app.exec();
}
