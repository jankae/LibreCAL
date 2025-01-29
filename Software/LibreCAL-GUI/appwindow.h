#ifndef APPWINDOW_H
#define APPWINDOW_H

#include "ui_main.h"
#include "caldevice.h"

#include <QMainWindow>
#include <QActionGroup>
#include <QLabel>
#include <QtCharts/QLineSeries>
#include <QTimer>

class AppWindow : public QMainWindow
{
    Q_OBJECT
public:
    AppWindow();
    ~AppWindow();

private slots:
    int UpdateDeviceList();
    bool ConnectToDevice(QString serial = QString());
    void DisconnectDevice();
    void DeviceConnectionLost();
    void EnterBootloader();

    void updateStatus();
    void loadCoefficients();
    void saveCoefficients();
    void createCoefficient();

    void showCoefficientSet(CalDevice::CoefficientSet &set);
private:
    bool confirmUnsavedCoefficients();
    static constexpr double updateInterval = 1.0;
    static constexpr double maxTempHistory = 60.0;
    Ui::MainWindow *ui;

    CalDevice *device;

    QActionGroup *deviceActionGroup;
    QLabel *status;

    std::array<QComboBox*, 4> portCBs;

    QTimer *updateTimer;
    // temperature chart widgets
    QLineSeries *tempSeries, *heaterSeries;
};

#endif // APPWINDOW_H
