#include "caldevice.h"

#include <thread>

#include <QDebug>

using namespace std;

CalDevice::CalDevice(QString serial) :
    usb(new USBDevice(serial))
{
    // Check device identification
    auto id = usb->Query("*IDN?");
    if(!id.startsWith("LibreCAL_")) {
        delete usb;
        throw std::runtime_error("Invalid response to *IDN?: "+id.toStdString());
    }

    firmware = usb->Query(":FIRMWARE?");
    QString ports = usb->Query(":PORTS?");
    bool okay;
    numPorts = ports.toInt(&okay);
    if(!okay) {
        numPorts = 0;
    }
}

CalDevice::~CalDevice()
{
    delete usb;
}

QString CalDevice::StandardToString(CalDevice::Standard s)
{
    switch(s) {
    case Standard::Open: return "OPEN";
    case Standard::Short: return "SHORT";
    case Standard::Load: return "LOAD";
    case Standard::Through: return "THROUGH";
    case Standard::None: return "NONE";
    }
}

CalDevice::Standard CalDevice::StandardFromString(QString s)
{
    for(int i=0;i<=(int) Standard::None;i++) {
        if(s == StandardToString((Standard) i)) {
            return (Standard) i;
        }
    }
    return Standard::None;
}

CalDevice::Standard CalDevice::getStandard(int port)
{
    auto query = ":PORT? "+QString::number(port);
    auto response = usb->Query(query);
    return StandardFromString(response);
}

bool CalDevice::setStandard(int port, CalDevice::Standard s)
{
    auto cmd = ":PORT "+QString::number(port)+" "+StandardToString(s);
    return usb->Cmd(cmd);
}

std::vector<CalDevice::Standard> CalDevice::availableStandards()
{
    return {Standard::None, Standard::Open, Standard::Short, Standard::Load, Standard::Through};
}

double CalDevice::getTemperature()
{
    QString tempString = usb->Query(":TEMP?");
    bool okay;
    double temp = tempString.toDouble(&okay);
    if(!okay) {
        temp = 0.0;
    }
    return temp;
}

bool CalDevice::stabilized()
{
    auto stable = usb->Query(":TEMPerature:STABLE?");
    return stable == "TRUE";
}

double CalDevice::getHeaterPower()
{
    QString tempString = usb->Query(":HEATER:POWER?");
    bool okay;
    double power = tempString.toDouble(&okay);
    if(!okay) {
        power = 0.0;
    }
    return power;
}

QString CalDevice::serial()
{
    return usb->serial();
}

QString CalDevice::getFirmware() const
{
    return firmware;
}

int CalDevice::getNumPorts() const
{
    return numPorts;
}



void CalDevice::updateCoefficientSets()
{
    coeffSets.clear();
    new std::thread(&CalDevice::updateCoefficientSetsThread, this);
}

void CalDevice::updateCoefficientSetsThread()
{
    QString resp = usb->Query(":COEFF:LIST?");
    if(!resp.startsWith("FACTORY")) {
        // something went wrong
        emit updateCoefficientsDone();
        return;
    }
    QStringList coeffList = resp.split(",");
    // get total number of coefficient points for accurate percentage calculation
    unsigned long totalPoints = 0;
    for(auto name : coeffList) {
        for(int i=1;i<=numPorts;i++) {
            totalPoints += usb->Query(":COEFF::NUM? "+name+" P"+QString::number(i)+"_OPEN").toInt();
            totalPoints += usb->Query(":COEFF::NUM? "+name+" P"+QString::number(i)+"_SHORT").toInt();
            totalPoints += usb->Query(":COEFF::NUM? "+name+" P"+QString::number(i)+"_LOAD").toInt();
            for(int j=i+1;j<=numPorts;i++) {
                totalPoints += usb->Query(":COEFF::NUM? "+name+" P"+QString::number(i)+QString::number(j)+"_THROUGH").toInt();
            }
        }
    }
    unsigned long readPoints = 0;
    int lastPercentage = 0;
    for(auto name : coeffList) {
        // create the coefficient set
        CoefficientSet set;
        set.name = name;
        // Read this coefficient set
        for(int i=1;i<=numPorts;i++) {
            auto createTouchstone = [&](QString setName, QString paramName) -> Touchstone* {
                int points = usb->Query(":COEFF::NUM? "+setName+" "+paramName).toInt();
                if(!points) {
                    // no points, nothing to do
                    return nullptr;
                }
                Touchstone *t;
                if(paramName.endsWith("THROUGH")) {
                    t = new Touchstone(2);
                } else {
                    t = new Touchstone(1);
                }
                for(int i=0;i<points;i++) {
                    QString pString = usb->Query(":COEFF:GET? "+setName+" "+paramName+" "+QString::number(i));
                    QStringList values = pString.split(",");
                    Touchstone::Datapoint p;
                    p.frequency = values[0].toDouble();
                    for(int j = 0;j<(values.size()-1)/2;j++) {
                        double real = values[1+j*2].toDouble();
                        double imag = values[2+j*2].toDouble();
                        p.S.push_back(complex<double>(real, imag));
                    }
                    t->AddDatapoint(p);
                    readPoints++;
                    int newPercentage = readPoints * 100 / totalPoints;
                    if(newPercentage != lastPercentage) {
                        lastPercentage = newPercentage;
                        emit updateCoefficientsPercent(newPercentage);
                    }
                }
                return t;
            };
            set.opens.push_back(createTouchstone(name, "P"+QString::number(i)+"_OPEN"));
            set.shorts.push_back(createTouchstone(name, "P"+QString::number(i)+"_SHORTS"));
            set.loads.push_back(createTouchstone(name, "P"+QString::number(i)+"_LOADS"));
            for(int j=i+1;j<=numPorts;i++) {
                set.throughs.push_back(createTouchstone(name, "P"+QString::number(i)+QString::number(j)+"_THROUGH"));
            }
        }
        coeffSets.push_back(set);
    }
    emit updateCoefficientsDone();
}