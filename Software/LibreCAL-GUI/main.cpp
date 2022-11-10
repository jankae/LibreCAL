#include <QtWidgets/QApplication>

#include "appwindow.h"

#include <QDebug>

static const QString APP_VERSION = QString::number(FW_MAJOR) + "." +
                                   QString::number(FW_MINOR) + "." +
                                   QString::number(FW_PATCH);
static const QString APP_GIT_HASH = QString(GITHASH);

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("LibreVNA");
    QCoreApplication::setApplicationName("LibreCAL-GUI");
    QCoreApplication::setApplicationVersion(APP_VERSION + "-" +
                                            APP_GIT_HASH.left(9));

    auto window = new AppWindow;

    window->show();

    return a.exec();
}
