#include "caldevice.h"

#include <QDebug>

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
