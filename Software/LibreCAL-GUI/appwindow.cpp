#include "appwindow.h"

#include "ui_main.h"
#include "usbdevice.h"
#include "about.h"
#include "unit.h"
#include "touchstoneimportdialog.h"
#include "CustomWidgets/informationbox.h"

#include <QDebug>

using namespace std;
using namespace QtCharts;

AppWindow::AppWindow() :
    device(nullptr),
    updateTimer(nullptr),
    backgroundOperations(false)
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
    connect(ui->actionReload_Coefficients, &QAction::triggered, this, &AppWindow::loadCoefficients);
    connect(ui->actionDisconnect, &QAction::triggered, this, &AppWindow::DisconnectDevice);
    connect(ui->actionAbout, &QAction::triggered, [=](){
        About::getInstance().about();
    });
    connect(ui->coeffList, &QListWidget::currentRowChanged, [=](int row){
        if(device) {
            if(row < device->getCoefficientSets().size()) {
                showCoefficientSet(device->getCoefficientSets()[row]);
            }
        }
    });
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
        ui->actionDisconnect->setEnabled(true);
        ui->actionReload_Coefficients->setEnabled(true);

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

//        loadCoefficients();

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

    backgroundOperations = false;

    for(auto a : deviceActionGroup->actions()) {
        a->setChecked(false);
    }
    if(deviceActionGroup->checkedAction()) {
        deviceActionGroup->checkedAction()->setChecked(false);
    }
    ui->actionDisconnect->setEnabled(false);
    ui->actionReload_Coefficients->setEnabled(false);
    tempSeries->clear();
    heaterSeries->clear();
    ui->temperatureStatus->clear();
    ui->coeffList->clear();
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
    if(device && !backgroundOperations) {
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

void AppWindow::loadCoefficients()
{
    if(!device || backgroundOperations) {
        return;
    }
    if(device->hasModifiedCoefficients()) {
        if(!confirmUnsavedCoefficients()) {
            // user selected to abort reloading
            return;
        }
    }
    backgroundOperations = true;
    auto d = new QProgressDialog();
    d->setLabelText("Loading calibration coefficients from device...");
    d->setWindowTitle("Updating");
    d->setWindowModality(Qt::ApplicationModal);
    d->setMinimumDuration(0);
    connect(device, &CalDevice::updateCoefficientsPercent, d, &QProgressDialog::setValue);
    connect(device, &CalDevice::updateCoefficientsDone, d, [=](){
        backgroundOperations = false;
        d->accept();
        delete d;

        ui->coeffList->clear();
        for(auto set : device->getCoefficientSets()) {
            ui->coeffList->addItem(set.name);
        }
        ui->coeffList->setCurrentRow(0);
    });
    d->show();
    device->updateCoefficientSets();
}

void AppWindow::showCoefficientSet(const CalDevice::CoefficientSet &set)
{
    auto setupCoefficient = [=](CalDevice::CoefficientSet::Coefficient *c, QCheckBox *modified, QLineEdit *info, QDialogButtonBox *buttons, int requiredPorts, bool editable){
        info->setStyleSheet("QLineEdit:read-only{background: palette(window);}");
        modified->setStyleSheet("QCheckBox{background: palette(window);}");
        modified->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto save = buttons->button(QDialogButtonBox::Save);
        auto load = buttons->button(QDialogButtonBox::Open);
        disconnect(save, &QPushButton::clicked, this, nullptr);
        disconnect(load, &QPushButton::clicked, this, nullptr);
        connect(save, &QPushButton::clicked, this, [=](){
            QString ending = ".s"+QString::number(c->t.ports())+"p";
            auto filename = QFileDialog::getSaveFileName(this, "Select file for exporting coefficients", "", "Touchstone file (*"+ending+")", nullptr, QFileDialog::DontUseNativeDialog);
            if(filename.isEmpty()) {
                // aborted selection
                return false;
            }
            c->t.toFile(filename);
        });
        connect(load, &QPushButton::clicked, this, [=](){
            auto filename = QFileDialog::getOpenFileName(this, "Select file for importing coefficients", "", "Touchstone files (*.s1p *.s2p *.s3p *.s4p)", nullptr, QFileDialog::DontUseNativeDialog);
            if(filename.isEmpty()) {
                // aborted selection
                return false;
            }
            auto import = new TouchstoneImportDialog(requiredPorts, filename);
            connect(import, &TouchstoneImportDialog::fileImported, [=](Touchstone file){
                c->t = file;
                c->modified = true;
                QString s = QString::number(c->t.points())+" points from "+Unit::ToString(c->t.minFreq(), "Hz", " kMG", 4);
                s += " to "+Unit::ToString(c->t.maxFreq(), "Hz", " kMG", 4);
                info->setText(s);
                save->setEnabled(true);
                modified->setChecked(true);
            });
            import->show();
        });
        if(!c->t.points()) {
            info->setText("Not available");
            save->setEnabled(false);
            modified->setChecked(false);
        } else {
            QString s = QString::number(c->t.points())+" points from "+Unit::ToString(c->t.minFreq(), "Hz", " kMG", 4);
            s += " to "+Unit::ToString(c->t.maxFreq(), "Hz", " kMG", 4);
            info->setText(s);
            save->setEnabled(true);
            modified->setChecked(c->modified);
        }
        if(editable) {
            modified->show();
            buttons->show();
        } else {
            modified->hide();
            buttons->hide();
        }
    };

    bool editable = set.name != "FACTORY";
    if(set.ports >= 1) {
        setupCoefficient(set.opens[0], ui->modifiedOpenP_1, ui->infoOpenP_1, ui->buttonsOpenP_1, 1, editable);
        setupCoefficient(set.shorts[0], ui->modifiedShortP_1, ui->infoShortP_1, ui->buttonsShortP_1, 1, editable);
        setupCoefficient(set.loads[0], ui->modifiedLoadP_1, ui->infoLoadP_1, ui->buttonsLoadP_1, 1, editable);
        ui->coeffBoxP_1->show();
    } else {
        ui->coeffBoxP_1->hide();
    }
    if(set.ports >= 2) {
        setupCoefficient(set.opens[1], ui->modifiedOpenP_2, ui->infoOpenP_2, ui->buttonsOpenP_2, 1, editable);
        setupCoefficient(set.shorts[1], ui->modifiedShortP_2, ui->infoShortP_2, ui->buttonsShortP_2, 1, editable);
        setupCoefficient(set.loads[1], ui->modifiedLoadP_2, ui->infoLoadP_2, ui->buttonsLoadP_2, 1, editable);
        ui->coeffBoxP_2->show();
    } else {
        ui->coeffBoxP_2->hide();
    }
    if(set.ports >= 3) {
        setupCoefficient(set.opens[2], ui->modifiedOpenP_3, ui->infoOpenP_3, ui->buttonsOpenP_3, 1, editable);
        setupCoefficient(set.shorts[2], ui->modifiedShortP_3, ui->infoShortP_3, ui->buttonsShortP_3, 1, editable);
        setupCoefficient(set.loads[2], ui->modifiedLoadP_3, ui->infoLoadP_3, ui->buttonsLoadP_3, 1, editable);
        ui->coeffBoxP_3->show();
    } else {
        ui->coeffBoxP_3->hide();
    }
    if(set.ports >= 4) {
        setupCoefficient(set.opens[3], ui->modifiedOpenP_4, ui->infoOpenP_4, ui->buttonsOpenP_4, 1, editable);
        setupCoefficient(set.shorts[3], ui->modifiedShortP_4, ui->infoShortP_4, ui->buttonsShortP_4, 1, editable);
        setupCoefficient(set.loads[3], ui->modifiedLoadP_4, ui->infoLoadP_4, ui->buttonsLoadP_4, 1, editable);
        ui->coeffBoxP_4->show();
    } else {
        ui->coeffBoxP_4->hide();
    }

    if(set.ports >= 2) {
        ui->coeffBoxThroughs->show();
        std::vector<QLabel*> tLabels = {ui->labelThroughP_12, ui->labelThroughP_13, ui->labelThroughP_14, ui->labelThroughP_23, ui->labelThroughP_24, ui->labelThroughP_34};
        std::vector<QCheckBox*> tCheckboxes = {ui->modifiedThroughP_12, ui->modifiedThroughP_13, ui->modifiedThroughP_14, ui->modifiedThroughP_23, ui->modifiedThroughP_24, ui->modifiedThroughP_34};
        std::vector<QLineEdit*> tInfos = {ui->infoThroughP_12, ui->infoThroughP_13, ui->infoThroughP_14, ui->infoThroughP_23, ui->infoThroughP_24, ui->infoThroughP_34};
        std::vector<QDialogButtonBox*> tButtons = {ui->buttonsThroughP_12, ui->buttonsThroughP_13, ui->buttonsThroughP_14, ui->buttonsThroughP_23, ui->buttonsThroughP_24, ui->buttonsThroughP_34};

        int index = 0;
        for(int i=1;i<=4;i++) {
            for(int j=i+1;j<=4;j++) {
                if(j <= set.ports) {
                    tLabels[index]->show();
                    tCheckboxes[index]->show();
                    tInfos[index]->show();
                    tButtons[index]->show();

                    setupCoefficient(set.getThrough(i,j), tCheckboxes[index], tInfos[index], tButtons[index], 2, editable);
                } else {
                    tLabels[index]->hide();
                    tCheckboxes[index]->hide();
                    tInfos[index]->hide();
                    tButtons[index]->hide();
                }
                index++;
            }
        }
    } else {
        // only one port, no throughs
        ui->coeffBoxThroughs->hide();
    }
}

bool AppWindow::confirmUnsavedCoefficients()
{
    return InformationBox::AskQuestion("Continue?", "You have modified the coefficients locally but not yet stored the updates in the device. "
                                             "If you continue, these changes will be lost. "
                                               "Do you want to continue?", true);
}
