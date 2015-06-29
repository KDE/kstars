#include <QApplication>
#include "Schedulerjob.h"

QString ObservableA::getName() const
{
return name;
}

void ObservableA::setName(const QString &value)
{
name = value;
}
QString ObservableA::getRA() const
{
return RA;
}

void ObservableA::setRA(const QString &value)
{
RA = value;
}
QString ObservableA::getDEC() const
{
return DEC;
}

void ObservableA::setDEC(const QString &value)
{
DEC = value;
}
QString ObservableA::getStartTime() const
{
return startTime;
}

void ObservableA::setStartTime(const QString &value)
{
startTime = value;
}
QString ObservableA::getFinTime() const
{
return finTime;
}

void ObservableA::setFinTime(const QString &value)
{
finTime = value;
}
QString ObservableA::getFileName() const
{
return fileName;
}

void ObservableA::setFileName(const QString &value)
{
fileName = value;
}
SkyObject *ObservableA::getOb() const
{
return ob;
}

void ObservableA::setOb(SkyObject *value)
{
ob = value;
}
float ObservableA::getAlt() const
{
return alt;
}

void ObservableA::setAlt(float value)
{
alt = value;
}
float ObservableA::getMoonSep() const
{
return MoonSep;
}

void ObservableA::setMoonSep(float value)
{
MoonSep = value;
}
int ObservableA::getHours() const
{
return hours;
}

void ObservableA::setHours(int value)
{
hours = value;
}
int ObservableA::getMins() const
{
return mins;
}

void ObservableA::setMins(int value)
{
mins = value;
}
bool ObservableA::getNow() const
{
return Now;
}

void ObservableA::setNow(bool value)
{
Now = value;
}
bool ObservableA::getSpecTime() const
{
return specTime;
}

void ObservableA::setSpecTime(bool value)
{
specTime = value;
}
bool ObservableA::getSpecAlt() const
{
return specAlt;
}

void ObservableA::setSpecAlt(bool value)
{
specAlt = value;
}
bool ObservableA::getMoonSepbool() const
{
return moonSepbool;
}

void ObservableA::setMoonSepbool(bool value)
{
moonSepbool = value;
}
bool ObservableA::getMerFlip() const
{
return merFlip;
}

void ObservableA::setMerFlip(bool value)
{
merFlip = value;
}
bool ObservableA::getWhenSeqComp() const
{
return whenSeqComp;
}

void ObservableA::setWhenSeqComp(bool value)
{
whenSeqComp = value;
}
bool ObservableA::getLoop() const
{
return loop;
}

void ObservableA::setLoop(bool value)
{
loop = value;
}
bool ObservableA::getOnTime() const
{
return onTime;
}

void ObservableA::setOnTime(bool value)
{
onTime = value;
}


















