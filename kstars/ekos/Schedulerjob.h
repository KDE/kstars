#include "kstars.h"

class ObservableA {
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

    float getMoonSep() const;
    void setMoonSep(float value);

    int getHours() const;
    void setHours(int value);

    int getMins() const;
    void setMins(int value);

    bool getNow() const;
    void setNow(bool value);

    bool getSpecTime() const;
    void setSpecTime(bool value);

    bool getSpecAlt() const;
    void setSpecAlt(bool value);

    bool getMoonSepbool() const;
    void setMoonSepbool(bool value);

    bool getMerFlip() const;
    void setMerFlip(bool value);

    bool getWhenSeqComp() const;
    void setWhenSeqComp(bool value);

    bool getLoop() const;
    void setLoop(bool value);

    bool getOnTime() const;
    void setOnTime(bool value);

private:
    QString name;
    QString RA;
    QString DEC;
    QString startTime;
    QString finTime;
    QString fileName;
    SkyObject *ob;

    float alt;
    float MoonSep;
    int hours;
    int mins;

    bool Now;
    bool specTime;
    bool specAlt;
    bool moonSepbool;
    bool merFlip;

    bool whenSeqComp;
    bool loop;
    bool onTime;
};
