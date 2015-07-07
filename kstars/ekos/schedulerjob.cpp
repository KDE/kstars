#include <QApplication>
#include "schedulerjob.h"

QString Schedulerjob::getName() const
{
return name;
}

void Schedulerjob::setName(const QString &value)
{
name = value;
}
QString Schedulerjob::getRA() const
{
return RA;
}

void Schedulerjob::setRA(const QString &value)
{
RA = value;
}
QString Schedulerjob::getDEC() const
{
return DEC;
}

void Schedulerjob::setDEC(const QString &value)
{
DEC = value;
}
QString Schedulerjob::getStartTime() const
{
return startTime;
}

void Schedulerjob::setStartTime(const QString &value)
{
startTime = value;
}
QString Schedulerjob::getFinTime() const
{
return finTime;
}

void Schedulerjob::setFinTime(const QString &value)
{
finTime = value;
}
QString Schedulerjob::getFileName() const
{
return fileName;
}

void Schedulerjob::setFileName(const QString &value)
{
fileName = value;
}
SkyObject *Schedulerjob::getOb() const
{
return ob;
}

void Schedulerjob::setOb(SkyObject *value)
{
ob = value;
}
float Schedulerjob::getAlt() const
{
return alt;
}

void Schedulerjob::setAlt(float value)
{
alt = value;
}
float Schedulerjob::getMoonSeparation() const
{
return moonSeparation;
}

void Schedulerjob::setMoonSeparation(float value)
{
moonSeparation = value;
}
int Schedulerjob::getHours() const
{
return hours;
}

void Schedulerjob::setHours(int value)
{
hours = value;
}
int Schedulerjob::getMinutes() const
{
return minutes;
}

void Schedulerjob::setMinutes(int value)
{
minutes = value;
}
bool Schedulerjob::getNowCheck() const
{
return NowCheck;
}

void Schedulerjob::setNowCheck(bool value)
{
NowCheck = value;
}
bool Schedulerjob::getSpecificTime() const
{
return specificTime;
}

void Schedulerjob::setSpecificTime(bool value)
{
specificTime = value;
}
bool Schedulerjob::getSpecificAlt() const
{
return specificAlt;
}

void Schedulerjob::setSpecificAlt(bool value)
{
specificAlt = value;
}
bool Schedulerjob::getMoonSeparationCheck() const
{
return moonSeparationCheck;
}

void Schedulerjob::setMoonSeparationCheck(bool value)
{
moonSeparationCheck = value;
}
bool Schedulerjob::getMeridianFlip() const
{
return meridianFlip;
}

void Schedulerjob::setMeridianFlip(bool value)
{
meridianFlip = value;
}
bool Schedulerjob::getWhenSeqCompCheck() const
{
return whenSeqCompCheck;
}

void Schedulerjob::setWhenSeqCompCheck(bool value)
{
whenSeqCompCheck = value;
}
bool Schedulerjob::getLoopCheck() const
{
return loopCheck;
}

void Schedulerjob::setLoopCheck(bool value)
{
loopCheck = value;
}
bool Schedulerjob::getOnTimeCheck() const
{
return onTimeCheck;
}

void Schedulerjob::setOnTimeCheck(bool value)
{
onTimeCheck = value;
}

int Schedulerjob::getScore() const
{
return score;
}

void Schedulerjob::setScore(int value)
{
score = value;
}
int Schedulerjob::getFinishingHour() const
{
    return finishingHour;
}

void Schedulerjob::setFinishingHour(int value)
{
    finishingHour = value;
}
int Schedulerjob::getFinishingMinute() const
{
    return finishingMinute;
}

void Schedulerjob::setFinishingMinute(int value)
{
    finishingMinute = value;
}
bool Schedulerjob::getFocusCheck() const
{
    return focusCheck;
}

void Schedulerjob::setFocusCheck(bool value)
{
    focusCheck = value;
}




















