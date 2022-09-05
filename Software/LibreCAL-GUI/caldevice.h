#ifndef CALDEVICE_H
#define CALDEVICE_H

#include "usbdevice.h"

#include <QString>

class CalDevice
{
public:
    CalDevice(QString serial);
    ~CalDevice();

    enum class Standard {
        Open,
        Short,
        Load,
        Through,
        None
    };

    static QString StandardToString(Standard s);
    static Standard StandardFromString(QString s);

    Standard getStandard(int port);
    bool setStandard(int port, Standard s);
    static std::vector<Standard> availableStandards();

    double getTemperature();
    bool stabilized();
    double getHeaterPower();

    QString serial();
    QString getFirmware() const;
    int getNumPorts() const;

private:
    USBDevice *usb;
    QString firmware;
    int numPorts;
};

#endif // CALDEVICE_H
