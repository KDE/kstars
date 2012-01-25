#include "genericdevice.h"

#include "indi/indielement.h"
#include "indi/indiproperty.h"
namespace EkoDevice
{

GenericDevice::GenericDevice(INDI_D *idp) : QObject()
{
  dp = idp;
}


 bool GenericDevice::Connect()
 {
     INDI_P * pp = dp->findProp("CONNECTION");

     if (pp == NULL)
         return false;

     INDI_E * el = pp->findElement(("CONNECT"));

     if (el == NULL)
         return false;

     pp->newSwitch(el);

     return true;
 }

 bool GenericDevice::Disconnect()
 {
     INDI_P * pp = dp->findProp("CONNECTION");

     if (pp == NULL)
         return false;

     INDI_E * el = pp->findElement(("DISCONNECT"));

     if (el == NULL)
         return false;

     pp->newSwitch(el);


     return true;
 }

 INDI_P * GenericDevice::getProperty(const QString &pp)
 {
     return dp->findProp(pp);
 }

 INDI_E * GenericDevice::getElement(const QString &el)
 {
     return dp->findElem(el);
 }

 bool GenericDevice::setSwitch(const QString &sw, ISState value)
 {
     return true;
 }

 bool GenericDevice::setNumber(const QString &nm, double value)
 {
     return true;
 }

 bool GenericDevice::setText(const QString &tx, QString value)
 {
     return true;
 }

 bool GenericDevice::setLight(const QString &lg, IPState value)
 {
     return true;
 }

 bool GenericDevice::setBLOB(const QString &bb, char *buf, unsigned int size)
 {
     return true;
 }

 bool GenericDevice::processSwitch(const QString &sw, ISState value)
 {
     return true;
 }

 bool GenericDevice::processNumber(const QString &nm, double value)
 {
     return true;
 }

 bool GenericDevice::processText(const QString &tx, QString value)
 {
     return true;
 }

 bool GenericDevice::processLight(const QString &lg, IPState value)
 {
     return true;
 }

 bool GenericDevice::processBLOB(const QString &bb, unsigned char *buffer, int bufferSize, const QString &dataFormat, INDI_D::DTypes dataType)
 {
     return true;
 }

 bool GenericDevice::processNewProperty(INDI_P * pp)
 {
     return true;
 }

} // namespace EkoDevice

#include "genericdevice.moc"
