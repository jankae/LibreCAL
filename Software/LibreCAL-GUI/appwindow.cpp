#include "appwindow.h"

#include "ui_main.h"
#include "usbdevice.h"
#include "about.h"
#include "unit.h"
#include "touchstoneimportdialog.h"
#include "CustomWidgets/informationbox.h"

#include <QDebug>
#include <QInputDialog>
#include <QFileDialog>
#include <QValueAxis>
#include <QProgressDialog>

using namespace std;

AppWindow::AppWindow() :
    device(nullptr),
    updateTimer(nullptr)
{
    ui = new Ui::MainWindow;
    ui->setupUi(this);

    setWindowIcon(QIcon(":/librecal.svg"));

    deviceActionGroup = new QActionGroup(this);

    status = new QLabel("No device connected");
    ui->statusbar->addPermanentWidget(status);
    ui->portInvalidLabel->setStyleSheet("QLabel { color : red }");
    ui->portInvalidLabel->setVisible(false);

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
    connect(ui->actionEnter_Bootloader_Mode, &QAction::triggered, [=](){
        if(InformationBox::AskQuestion("Enter Bootloader?", "Do you want to enter the bootloader mode? The device will disconnect and reboot as a mass storage device for the firmware update.", true)) {
            EnterBootloader();
        }
    });
    connect(ui->actionAbout, &QAction::triggered, [=](){
        About::getInstance().about();
    });
    connect(ui->coeffList, &QListWidget::currentRowChanged, [=](int row){
        if(device) {
            if(row >= 0 && row < (int) device->getCoefficientSets().size()) {
                showCoefficientSet(device->getCoefficientSets()[row]);
            }
        }
    });
    connect(ui->loadCoefficients, &QPushButton::clicked, this, &AppWindow::loadCoefficients);
    connect(ui->saveCoefficients, &QPushButton::clicked, this, &AppWindow::saveCoefficients);
    connect(ui->newCoefficient, &QPushButton::clicked, this, &AppWindow::createCoefficient);
    connect(ui->updateFactory, &QPushButton::clicked, this, [=](){
        if(device) {
            if(device->hasModifiedCoefficients()) {
                InformationBox::ShowError("Unsaved coefficients", "Some coefficients have been modified. They must be saved to the device before the factory coefficients can be updated");
            } else {
                device->factoryUpdateDialog();
            }
        }
    });
    if(UpdateDeviceList()) {
        ConnectToDevice();
    }
}

AppWindow::~AppWindow()
{
    delete device;
    delete deviceActionGroup;
    delete status;
    delete updateTimer;
    delete tempSeries;
    delete heaterSeries;
    delete ui;
}

void AppWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    delete device;
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
        ui->actionEnter_Bootloader_Mode->setEnabled(true);
        ui->loadCoefficients->setEnabled(true);
        ui->updateFactory->setEnabled(true);
        ui->newCoefficient->setEnabled(true);

        // this needs to be a queued connection because connection problems are only discovered during communication.
        // Deleting the device (in the DeviceConnectionLost slot) mustn't happen before the current communication has finished)
        auto conn = connect(device, &CalDevice::disconnected, this, &AppWindow::DeviceConnectionLost, Qt::QueuedConnection);
        // However, we only want to trigger the device disconnect once (additional communication attempts might send the
        // CalDevice::disconnected signal again -> disconnect with a direct connection
        connect(device, &CalDevice::disconnected, [=](){
            disconnect(conn);
        });

        for(auto d : deviceActionGroup->actions()) {
            if(d->text() == device->serial()) {
                d->blockSignals(true);
                d->setChecked(true);
                d->blockSignals(false);
                break;
            }
        }

        QString LibreCAL_DateTimeUTC = device->getDateTimeUTC();
        if(LibreCAL_DateTimeUTC.isEmpty())
        {
            status->setText("Connected to "+device->serial()+
                            ", firmware version "+device->getFirmware()+
                            ", "+QString::number(device->getNumPorts())+" ports");
        }else
        {
            status->setText("Connected to "+device->serial()+
                            ", firmware version "+device->getFirmware()+
                            ", "+QString::number(device->getNumPorts())+" ports"+
                            ", "+LibreCAL_DateTimeUTC);
        }
        for(unsigned int i=0;i<device->getNumPorts();i++) {
            portCBs[i]->setEnabled(true);
            for(auto s : CalDevice::availableStandards()) {
                if(s.type == CalDevice::Standard::Type::Through && s.throughDest == (int) i+1) {
                    // do not add through to the port itself
                    continue;
                }
                portCBs[i]->addItem(CalDevice::StandardToString(s));
            }
            portCBs[i]->setCurrentText(CalDevice::StandardToString(device->getStandard(i+1)));
            connect(portCBs[i], &QComboBox::currentTextChanged, this, [=](){
                auto standard = CalDevice::StandardFromString(portCBs[i]->currentText());
                device->setStandard(i+1, standard);
            });
        }

//        loadCoefficients();

        qDebug() << "Creating timer";
        updateTimer = new QTimer(this);
        connect(updateTimer, &QTimer::timeout, this, &AppWindow::updateStatus);
        updateTimer->start(updateInterval * 1000);

        return true;
    } catch (const runtime_error &e) {
        qWarning() << "Failed to connect:" << e.what();
        InformationBox::ShowError("Error", "Failed to connect to "+serial+": "+e.what());
        DisconnectDevice();
        UpdateDeviceList();
        return false;
    }
}

void AppWindow::DisconnectDevice()
{
    qDebug() << "Destroying timer";
    delete updateTimer;
    updateTimer = nullptr;
    delete device;
    device = nullptr;

    ui->portInvalidLabel->setVisible(false);

    for(auto a : deviceActionGroup->actions()) {
        a->setChecked(false);
    }
    if(deviceActionGroup->checkedAction()) {
        deviceActionGroup->checkedAction()->setChecked(false);
    }
    ui->actionDisconnect->setEnabled(false);
    ui->actionEnter_Bootloader_Mode->setEnabled(false);
    ui->loadCoefficients->setEnabled(false);
    ui->updateFactory->setEnabled(false);
    ui->newCoefficient->setEnabled(false);
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

void AppWindow::DeviceConnectionLost()
{
    DisconnectDevice();
    InformationBox::ShowError("Disconnected", "The USB connection to the device has been lost");
    UpdateDeviceList();
}

void AppWindow::EnterBootloader()
{
    if(!device) {
        return;
    }
    if(device->enterBootloader()) {
        DisconnectDevice();
    } else {
        InformationBox::ShowError("Error", "Failed to enter bootloader mode");
    }
}

void AppWindow::updateStatus()
{
    if(device && !device->coefficientTransferActive()) {
        // update port status
        for(unsigned int i=0;i<device->getNumPorts();i++) {
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
        ui->chartView->chart()->axes(Qt::Vertical)[0]->setRange(minTemp - extraYSpace, maxTemp + extraYSpace);

        auto stable = device->stabilized();
        if(!stable) {
            ui->temperatureStatus->setStyleSheet("QLabel { color : red; }");
            QString date_time_utc = device->getDateTimeUTC();
            QString temperature;
            temperature = QString("Temperature %1°C (min=%2°C max=%3°C) unstable, please wait before taking measurements")
                          .arg(temp, 2, 'f', 2)
                          .arg(minTemp, 2, 'f', 2)
                          .arg(maxTemp, 2, 'f', 2);
            if(!date_time_utc.isEmpty()) {
                temperature = QString(date_time_utc+" "+temperature);
            }
            ui->temperatureStatus->setText(temperature);
        } else {
            ui->temperatureStatus->setStyleSheet("QLabel { color : black; }");
            QString date_time_utc = device->getDateTimeUTC();
            QString temperature;
            temperature = QString("Temperature %1°C (min=%2°C max=%3°C)")
                            .arg(temp, 2, 'f', 2)
                            .arg(minTemp, 2, 'f', 2)
                            .arg(maxTemp, 2, 'f', 2);
            if(!date_time_utc.isEmpty()) {
                temperature = QString(date_time_utc+" "+temperature);
            }
            ui->temperatureStatus->setText(temperature);
        }
    }
}

void AppWindow::loadCoefficients()
{
    if(!device || device->coefficientTransferActive()) {
        return;
    }
    if(device->hasModifiedCoefficients()) {
        if(!confirmUnsavedCoefficients()) {
            // user selected to abort reloading
            return;
        }
    }
    auto d = new QProgressDialog();
    d->setLabelText("Loading calibration coefficients from device...");
    d->setWindowTitle("Updating");
    d->setWindowModality(Qt::ApplicationModal);
    d->setMinimumDuration(0);
    d->setCancelButton(nullptr);
    connect(device, &CalDevice::updateCoefficientsPercent, d, &QProgressDialog::setValue, Qt::QueuedConnection);
    connect(device, &CalDevice::updateCoefficientsDone, d, [=](){
        d->accept();
        d->deleteLater();
    }, Qt::QueuedConnection);
    connect(device, &CalDevice::updateCoefficientsDone, this, [=](){
        ui->saveCoefficients->setEnabled(false);

        ui->coeffList->clear();
        for(auto set : device->getCoefficientSets()) {
            ui->coeffList->addItem(set.name);
        }
        ui->coeffList->setCurrentRow(0);
    }, Qt::QueuedConnection);
    d->show();
    device->loadCoefficientSets();
}

void AppWindow::saveCoefficients()
{
    if(!device) {
        return;
    }
    if(!device->hasModifiedCoefficients()) {
        // no changes, nothing to do
        ui->saveCoefficients->setEnabled(false);
        return;
    }
    auto d = new QProgressDialog();
    d->setLabelText("Saving calibration coefficients to device...");
    d->setWindowTitle("Updating");
    d->setWindowModality(Qt::ApplicationModal);
    d->setMinimumDuration(0);
    d->setCancelButton(nullptr);
    connect(device, &CalDevice::updateCoefficientsPercent, d, &QProgressDialog::setValue, Qt::QueuedConnection);
    connect(device, &CalDevice::updateCoefficientsDone, d, [=](){
        d->accept();
        delete d;
    }, Qt::QueuedConnection);
    connect(device, &CalDevice::updateCoefficientsDone, d, [=](){
        ui->saveCoefficients->setEnabled(device->hasModifiedCoefficients());

        ui->coeffList->clear();
        for(auto set : device->getCoefficientSets()) {
            ui->coeffList->addItem(set.name);
        }
        ui->coeffList->setCurrentRow(0);
    });
    d->show();
    device->saveCoefficientSets();
}

void AppWindow::createCoefficient()
{
    bool ok;
    QString text = QInputDialog::getText(this, "New coefficient", "Name:", QLineEdit::Normal, QString(), &ok);
    if (!ok || text.isEmpty()) {
        // aborted
        return;
    }
    // check if name is already taken
    for(auto c : device->getCoefficientSets()) {
        if(c.name == text) {
            // name collision
            InformationBox::ShowError("Duplicate name", "Unable to create, this coefficient set name is already in use.");
            return;
        }
    }
    // name is free
    device->addCoefficientSet(text);
    ui->coeffList->clear();
    for(auto set : device->getCoefficientSets()) {
        ui->coeffList->addItem(set.name);
    }
    // select the newly created coefficient set
    ui->coeffList->setCurrentRow(device->getCoefficientSets().size()-1);
}

void AppWindow::showCoefficientSet(CalDevice::CoefficientSet &set)
{
    auto setupCoefficient = [=](CalDevice::CoefficientSet::Coefficient *c, QCheckBox *modified, QLineEdit *info, QDialogButtonBox *buttons, int requiredPorts, bool editable){
        info->setStyleSheet("QLineEdit:read-only{background: palette(window);}");
        modified->setStyleSheet("QCheckBox{background: palette(window);}");
        modified->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto save = buttons->button(QDialogButtonBox::Save);
        auto load = buttons->button(QDialogButtonBox::Open);
        auto reset = buttons->button(QDialogButtonBox::Reset);
        disconnect(save, &QPushButton::clicked, this, nullptr);
        disconnect(load, &QPushButton::clicked, this, nullptr);
        disconnect(reset, &QPushButton::clicked, this, nullptr);
        connect(save, &QPushButton::clicked, this, [=](){
            QString ending = ".s"+QString::number(c->t.ports())+"p";
            auto filename = QFileDialog::getSaveFileName(this, "Select file for exporting coefficients", "", "Touchstone file (*"+ending+")", nullptr, QFileDialog::DontUseNativeDialog);
            if(filename.isEmpty()) {
                // aborted selection
                return;
            }
            c->t.toFile(filename);
        });
        connect(load, &QPushButton::clicked, this, [=](){
            auto filename = QFileDialog::getOpenFileName(this, "Select file for importing coefficients", "", "Touchstone files (*.s1p *.s2p *.s3p *.s4p)", nullptr, QFileDialog::DontUseNativeDialog);
            if(filename.isEmpty()) {
                // aborted selection
                return;
            }
            auto import = new TouchstoneImportDialog(requiredPorts, filename);
            connect(import, &TouchstoneImportDialog::fileImported, this, [=](Touchstone file){
                c->t = file;
                c->modified = true;
                QString s = QString::number(c->t.points())+" points from "+Unit::ToString(c->t.minFreq(), "Hz", " kMG", 4);
                s += " to "+Unit::ToString(c->t.maxFreq(), "Hz", " kMG", 4);
                info->setText(s);
                save->setEnabled(true);
                reset->setEnabled(true);
                modified->setChecked(true);
                ui->saveCoefficients->setEnabled(true);
            });
            import->show();
        });
        connect(reset, &QPushButton::clicked, this, [=](){
            c->t = Touchstone(requiredPorts);
            c->modified = true;
            info->setText("Not available");
            save->setEnabled(false);
            modified->setChecked(true);
            reset->setEnabled(false);
            ui->saveCoefficients->setEnabled(true);
        });
        if(!c || !c->t.points()) {
            info->setText("Not available");
            save->setEnabled(false);
            modified->setChecked(false);
            reset->setEnabled(false);
        } else {
            QString s = QString::number(c->t.points())+" points from "+Unit::ToString(c->t.minFreq(), "Hz", " kMG", 4);
            s += " to "+Unit::ToString(c->t.maxFreq(), "Hz", " kMG", 4);
            info->setText(s);
            save->setEnabled(true);
            reset->setEnabled(true);
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

    QList<QCheckBox*> modifiedOpen = {ui->modifiedOpenP_1, ui->modifiedOpenP_2, ui->modifiedOpenP_3, ui->modifiedOpenP_4};
    QList<QCheckBox*> modifiedShort = {ui->modifiedShortP_1, ui->modifiedShortP_2, ui->modifiedShortP_3, ui->modifiedShortP_4};
    QList<QCheckBox*> modifiedLoad = {ui->modifiedLoadP_1, ui->modifiedLoadP_2, ui->modifiedLoadP_3, ui->modifiedLoadP_4};

    QList<QLineEdit*> infoOpen = {ui->infoOpenP_1, ui->infoOpenP_2, ui->infoOpenP_3, ui->infoOpenP_4};
    QList<QLineEdit*> infoShort = {ui->infoShortP_1, ui->infoShortP_2, ui->infoShortP_3, ui->infoShortP_4};
    QList<QLineEdit*> infoLoad = {ui->infoLoadP_1, ui->infoLoadP_2, ui->infoLoadP_3, ui->infoLoadP_4};

    QList<QDialogButtonBox*> buttonsOpen = {ui->buttonsOpenP_1, ui->buttonsOpenP_2, ui->buttonsOpenP_3, ui->buttonsOpenP_4};
    QList<QDialogButtonBox*> buttonsShort = {ui->buttonsShortP_1, ui->buttonsShortP_2, ui->buttonsShortP_3, ui->buttonsShortP_4};
    QList<QDialogButtonBox*> buttonsLoad = {ui->buttonsLoadP_1, ui->buttonsLoadP_2, ui->buttonsLoadP_3, ui->buttonsLoadP_4};

    QList<QGroupBox*> coeffBox = {ui->coeffBoxP_1, ui->coeffBoxP_2, ui->coeffBoxP_3, ui->coeffBoxP_4};

    for(int port = 1;port <= set.ports;port++) {
        int index = port - 1;
        setupCoefficient(set.getOpen(port), modifiedOpen[index], infoOpen[index], buttonsOpen[index], 1, editable);
        setupCoefficient(set.getShort(port), modifiedShort[index], infoShort[index], buttonsShort[index], 1, editable);
        setupCoefficient(set.getLoad(port), modifiedLoad[index], infoLoad[index], buttonsLoad[index], 1, editable);
        coeffBox[index]->show();
    }
    for(unsigned int port = set.ports + 1; port <= 4;port++) {
        int index = port - 1;
        coeffBox[index]->hide();
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
