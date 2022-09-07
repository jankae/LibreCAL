#ifndef CALDEVICE_H
#define CALDEVICE_H

#include "usbdevice.h"
#include "touchstone.h"

#include <QString>
#include <QObject>

class CalDevice : public QObject
{
    Q_OBJECT
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

    class CoefficientSet {
    public:
        QString name;
        std::vector<Touchstone*> opens;
        std::vector<Touchstone*> shorts;
        std::vector<Touchstone*> loads;
        std::vector<Touchstone*> throughs;
    };

    // Extracts the coefficients from the device. This is done with a dedicated thread.
    // Do not call any other functions until the update is finished. Process can be
    // monitored through the updateCoefficientsPercent and updateCoefficientsDone signals
    void updateCoefficientSets();
signals:
    void updateCoefficientsPercent(int percent);
    // emitted when all coefficients have been received and it is safe to call all functions again
    void updateCoefficientsDone();

private:
    void updateCoefficientSetsThread();

    USBDevice *usb;
    QString firmware;
    int numPorts;

    std::vector<CoefficientSet> coeffSets;
};

#endif // CALDEVICE_H
