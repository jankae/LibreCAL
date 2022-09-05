#include "appwindow.h"

#include "ui_main.h"
#include "usbdevice.h"

#include <QDebug>

using namespace std;
using namespace QtCharts;

AppWindow::AppWindow() :
    device(nullptr)
{
    ui = new Ui::MainWindow;
    ui->setupUi(this);

    deviceActionGroup = new QActionGroup(this);

    status = new QLabel("No device connected");
    ui->statusbar->addPermanentWidget(status);

    portCBs = {ui->port1, ui->port2, ui->port3, ui->port4};

    // Set up the temperature chart
    auto chart = new QChart();
    ui->chartView->setChart(chart);
    tempSeries = new QLineSeries;
    heaterSeries = new QLineSeries;

    auto pen = tempSeries->pen();
    pen.setWidth(2);
    pen.setBrush(QBrush("blue"));
    tempSeries->setPen(pen);

    pen = heaterSeries->pen();
    pen.setWidth(2);
    pen.setBrush(QBrush("red"));
    heaterSeries->setPen(pen);

    auto xAxis = new QValueAxis;
    xAxis->setTitleText("Time [s]");
    xAxis->setRange(-maxTempHistory,0);
    auto yAxis = new QValueAxis;
    yAxis->setTitleText("Temperature [°C]");
    yAxis->setTitleBrush(QBrush("blue"));
    yAxis->setRange(-5, 40);

    auto yAxis2 = new QValueAxis;
    yAxis2->setTitleText("Heater Power [W]");
    yAxis2->setTitleBrush(QBrush("red"));
    yAxis2->setRange(0, 2);

    chart->legend()->hide();
    chart->addSeries(tempSeries);
    chart->addSeries(heaterSeries);
    chart->addAxis(xAxis, Qt::AlignBottom);
    chart->addAxis(yAxis, Qt::AlignLeft);
    chart->addAxis(yAxis2, Qt::AlignRight);
    tempSeries->attachAxis(xAxis);
    tempSeries->attachAxis(yAxis);
    heaterSeries->attachAxis(xAxis);
    heaterSeries->attachAxis(yAxis2);

    connect(ui->actionUpdate_Device_List, &QAction::triggered, this, &AppWindow::UpdateDeviceList);
    connect(ui->actionDisconnect, &QAction::triggered, this, &AppWindow::DisconnectDevice);
    if(UpdateDeviceList()) {
        ConnectToDevice();
    }
}

int AppWindow::UpdateDeviceList()
{
    deviceActionGroup->setExclusive(true);
    ui->menuConnect_to->clear();
    auto devices = USBDevice::GetDevices();
    if(device) {
        devices.insert(device->serial());
    }
    int available = 0;
    bool found = false;
    if(devices.size()) {
        for(auto d : devices) {
//            if(!parser.value("device").isEmpty() && parser.value("device") != d) {
//                // specified device does not match, ignore
//                continue;
//            }
            auto connectAction = ui->menuConnect_to->addAction(d);
            connectAction->setCheckable(true);
            connectAction->setActionGroup(deviceActionGroup);
            if(device && d == device->serial()) {
                connectAction->setChecked(true);
            }
            connect(connectAction, &QAction::triggered, [this, d]() {
               ConnectToDevice(d);
            });
            found = true;
            available++;
        }
    }
    ui->menuConnect_to->setEnabled(found);
    qDebug() << "Updated device list, found" << available;
    return available;
}

bool AppWindow::ConnectToDevice(QString serial)
{
    qDebug() << "Trying to connect to" << serial;
    if(device) {
        qDebug() << "Already connected to a device, disconnecting first...";
        DisconnectDevice();
    }
    try {
        qDebug() << "Attempting to connect to device...";
        device = new CalDevice(serial);
//        UpdateStatusBar(AppWindow::DeviceStatusBar::Connected);
//        connect(vdevice, &VirtualDevice::InfoUpdated, this, &AppWindow::DeviceInfoUpdated);
//        connect(vdevice, &VirtualDevice::LogLineReceived, &deviceLog, &DeviceLog::addLine);
//        connect(vdevice, &VirtualDevice::ConnectionLost, this, &AppWindow::DeviceConnectionLost);
//        connect(vdevice, &VirtualDevice::StatusUpdated, this, &AppWindow::DeviceStatusUpdated);
//        connect(vdevice, &VirtualDevice::NeedsFirmwareUpdate, this, &AppWindow::DeviceNeedsUpdate);
        ui->actionDisconnect->setEnabled(true);
//        if(!vdevice->isCompoundDevice()) {
//            ui->actionManual_Control->setEnabled(true);
//            ui->actionFirmware_Update->setEnabled(true);
//            ui->actionSource_Calibration->setEnabled(true);
//            ui->actionReceiver_Calibration->setEnabled(true);
//            ui->actionFrequency_Calibration->setEnabled(true);
//        }
//        ui->actionPreset->setEnabled(true);

//        UpdateAcquisitionFrequencies();

        for(auto d : deviceActionGroup->actions()) {
            if(d->text() == device->serial()) {
                d->blockSignals(true);
                d->setChecked(true);
                d->blockSignals(false);
                break;
            }
        }

        status->setText("Connected to "+device->serial()+", firmware version "+device->getFirmware()+", "+QString::number(device->getNumPorts())+" ports");

        for(int i=0;i<device->getNumPorts();i++) {
            portCBs[i]->setEnabled(true);
            for(auto s : CalDevice::availableStandards()) {
                portCBs[i]->addItem(CalDevice::StandardToString(s));
            }
            portCBs[i]->setCurrentText(CalDevice::StandardToString(device->getStandard(i+1)));
            connect(portCBs[i], &QComboBox::currentTextChanged, this, [=](){
                auto standard = CalDevice::StandardFromString(portCBs[i]->currentText());
                device->setStandard(i+1, standard);
            });
        }

        updateTimer = new QTimer(this);
        connect(updateTimer, &QTimer::timeout, this, &AppWindow::updateStatus);
        updateTimer->start(updateInterval * 1000);

        return true;
    } catch (const runtime_error &e) {
        qWarning() << "Failed to connect:" << e.what();
        DisconnectDevice();
        UpdateDeviceList();
        return false;
    }
}

void AppWindow::DisconnectDevice()
{
    delete updateTimer;
    delete device;
    device = nullptr;

    for(auto a : deviceActionGroup->actions()) {
        a->setChecked(false);
    }
    if(deviceActionGroup->checkedAction()) {
        deviceActionGroup->checkedAction()->setChecked(false);
    }
    ui->actionDisconnect->setEnabled(false);
//    UpdateStatusBar(DeviceStatusBar::Disconnected);
//    if(modeHandler->getActiveMode()) {
//        modeHandler->getActiveMode()->deviceDisconnected();
//    }
    tempSeries->clear();
    heaterSeries->clear();
    ui->temperatureStatus->clear();
    status->setText("No device connected");
    for(auto p : portCBs) {
        disconnect(p, &QComboBox::currentTextChanged, this, nullptr);
        p->clear();
        p->setEnabled(false);
    }
    qDebug() << "Disconnected device";
}

void AppWindow::updateStatus()
{
    if(device) {
        // update port status
        for(int i=0;i<device->getNumPorts();i++) {
            portCBs[i]->blockSignals(true);
            portCBs[i]->setCurrentText(CalDevice::StandardToString(device->getStandard(i+1)));
            portCBs[i]->blockSignals(false);
        }
        // update temperature
        auto temp = device->getTemperature();
        auto power = device->getHeaterPower();
        double minTemp = temp, maxTemp = temp;
        for(int i=0;i<tempSeries->count();i++) {
            auto point = tempSeries->at(i);
            if(point.y() > maxTemp) {
                maxTemp = point.y();
            }
            if(point.y() < minTemp) {
                minTemp = point.y();
            }
            tempSeries->replace(i, point + QPointF(-updateInterval, 0));
            heaterSeries->replace(i, heaterSeries->at(i) + QPointF(-updateInterval, 0));
        }
        tempSeries->append(0, temp);
        heaterSeries->append(0, power);
        if(tempSeries->count() > (maxTempHistory / updateInterval) + 1) {
            tempSeries->remove(0);
            heaterSeries->remove(0);
        }
        // find minimum/maximum temperature
        auto range = maxTemp - minTemp;
        double extraYSpace = range / 10;
        if(range == 0) {
            extraYSpace = 0.1;
        }
        ui->chartView->chart()->axisY()->setRange(minTemp - extraYSpace, maxTemp + extraYSpace);

        auto stable = device->stabilized();
        if(!stable) {
            ui->temperatureStatus->setStyleSheet("QLabel { color : red; }");
            ui->temperatureStatus->setText("Temperature unstable, please wait before taking measurements");
        } else {
            ui->temperatureStatus->clear();
        }
    }
}
