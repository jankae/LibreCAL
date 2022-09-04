#include <QtWidgets/QApplication>

#include "appwindow.h"

#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("LibreVNA");
    QCoreApplication::setApplicationName("LibreCAL-GUI");

    auto window = new AppWindow;

    window->show();

    return a.exec();
}
