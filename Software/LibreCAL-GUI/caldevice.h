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
        int ports;
        class Coefficient {
        public:
            Coefficient() : t(Touchstone(1)), modified(false) {}
            Touchstone t;
            bool modified;
        };

        std::vector<Coefficient*> opens;
        std::vector<Coefficient*> shorts;
        std::vector<Coefficient*> loads;
        std::vector<Coefficient*> throughs;

        Coefficient *getThrough(int port1, int port2) const;
    };

    // Extracts the coefficients from the device. This is done with a dedicated thread.
    // Do not call any other functions until the update is finished. Process can be
    // monitored through the updateCoefficientsPercent and updateCoefficientsDone signals
    void updateCoefficientSets();
    std::vector<CoefficientSet> getCoefficientSets() const;

    bool hasModifiedCoefficients();

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
