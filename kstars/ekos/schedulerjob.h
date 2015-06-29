#include "kstars.h"

class Schedulerjob {
public:
    QString getName() const;
    void setName(const QString &value);

    QString getRA() const;
    void setRA(const QString &value);

    QString getDEC() const;
    void setDEC(const QString &value);

    QString getStartTime() const;
    void setStartTime(const QString &value);

    QString getFinTime() const;
    void setFinTime(const QString &value);

    QString getFileName() const;
    void setFileName(const QString &value);

    SkyObject *getOb() const;
    void setOb(SkyObject *value);

    float getAlt() const;
    void setAlt(float value);

    float getMoonSeparation() const;
    void setMoonSeparation(float value);

    int getHours() const;
    void setHours(int value);

    int getMinutes() const;
    void setMinutes(int value);

    bool getNowCheck() const;
    void setNowCheck(bool value);

    bool getSpecificTime() const;
    void setSpecificTime(bool value);

    bool getSpecificAlt() const;
    void setSpecificAlt(bool value);

    bool getMoonSeparationCheck() const;
    void setMoonSeparationCheck(bool value);

    bool getMeridianFlip() const;
    void setMeridianFlip(bool value);

    bool getWhenSeqCompCheck() const;
    void setWhenSeqCompCheck(bool value);

    bool getLoopCheck() const;
    void setLoopCheck(bool value);

    bool getOnTimeCheck() const;
    void setOnTimeCheck(bool value);

private:
    QString name;
    QString RA;
    QString DEC;
    QString startTime;
    QString finTime;
    QString fileName;
    SkyObject *ob;

    float alt;
    float moonSeparation;
    int hours;
    int minutes;

    bool NowCheck;
    bool specificTime;
    bool specificAlt;
    bool moonSeparationCheck;
    bool meridianFlip;

    bool whenSeqCompCheck;
    bool loopCheck;
    bool onTimeCheck;
};
