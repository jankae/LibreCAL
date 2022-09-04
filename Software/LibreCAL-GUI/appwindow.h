#ifndef APPWINDOW_H
#define APPWINDOW_H

#include <QMainWindow>

#include "ui_main.h"
#include "usbdevice.h"

class AppWindow : public QMainWindow
{
    Q_OBJECT
public:
    AppWindow();

private slots:
    void UpdateDeviceList();
private:
    Ui::MainWindow *ui;
};

#endif // APPWINDOW_H
