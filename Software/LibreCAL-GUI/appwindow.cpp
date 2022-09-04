#include "appwindow.h"

#include "ui_main.h"
#include "usbdevice.h"

AppWindow::AppWindow()
{
    ui = new Ui::MainWindow;
    ui->setupUi(this);

    connect(ui->actionUpdate_Device_List, &QAction::triggered, this, &AppWindow::UpdateDeviceList);
    UpdateDeviceList();
}

void AppWindow::UpdateDeviceList()
{
//    deviceActionGroup->setExclusive(true);
    ui->menuConnect_to->clear();
    auto devices = USBDevice::GetDevices();
//    if(vdevice) {
//        devices.insert(vdevice->serial());
//    }
//    int available = 0;
    bool found = false;
    if(devices.size()) {
        for(auto d : devices) {
//            if(!parser.value("device").isEmpty() && parser.value("device") != d) {
//                // specified device does not match, ignore
//                continue;
//            }
            auto connectAction = ui->menuConnect_to->addAction(d);
            connectAction->setCheckable(true);
//            connectAction->setActionGroup(deviceActionGroup);
//            if(vdevice && d == vdevice->serial()) {
//                connectAction->setChecked(true);
//            }
//            connect(connectAction, &QAction::triggered, [this, d]() {
//               ConnectToDevice(d);
//            });
            found = true;
//            available++;
        }
    }
    ui->menuConnect_to->setEnabled(found);
//    qDebug() << "Updated device list, found" << available;
//    return available;
}
