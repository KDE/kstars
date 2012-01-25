#ifndef EKODEVICE_GENERICDEVICE_H
#define EKODEVICE_GENERICDEVICE_H

#include <indiapi.h>
#include <QObject>

#include "indi/indidevice.h"

class QString;

class INDI_P;
class INDI_E;

namespace EkoDevice
{

class GenericDevice : public QObject
{
    Q_OBJECT

public:
    GenericDevice(INDI_D *idp);

    virtual bool Connect();
    virtual bool Disconnect();

    virtual bool setSwitch(const QString &sw, ISState value);
    virtual bool setNumber(const QString &nm, double value);
    virtual bool setText(const QString &tx, QString value);
    virtual bool setLight(const QString &lg, IPState value);
    virtual bool setBLOB(const QString &bb, char *buf, unsigned int size);

    INDI_D * getDevice() { return dp; }
    INDI_P * getProperty(const QString &pp);
    INDI_E * getElement(const QString &el);

    INDI_D *getDP() { return dp; }

public slots:
    virtual bool processSwitch(const QString &sw, ISState value);
    virtual bool processNumber(const QString &nm, double value);
    virtual bool processText(const QString &tx, QString value);
    virtual bool processLight(const QString &lg, IPState value);
    virtual bool processBLOB(const  QString &bb, unsigned char *buffer, int bufferSize, const QString &dataFormat, INDI_D::DTypes dataType);
    virtual bool processNewProperty(INDI_P *p);

signals:
    void deviceConnected();
    void deviceDisconnected();

    // We don't emit those directly, they are emitted by INDI_D
    void newSwitch(const QString &sw, ISState value);
    void newNumber(const QString &nm, double value);
    void newText(const QString &tx, QString value);
    void newLight(const QString &lg, IPState value);
    void newBLOB(const QString &bb, unsigned char *buffer, int bufferSize, const QString &dataFormat, INDI_D::DTypes dataType);
    void newProperty(INDI_P *p);

private:

    INDI_D *dp;

};

} // namespace EkoDevice

#endif // EKODEVICE_GENERICDEVICE_H
