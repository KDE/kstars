#include "kstars.h"

class Schedulerjob {
public:
    Schedulerjob();
    enum StateChoice{IDLE, READY, SLEWING, SLEW_COMPLETE, FOCUSING, FOCUSING_COMPLETE, ALIGNING, ALIGNING_COMPLETE, GUIDING, GUIDING_COMPLETE,
                     CAPTURING, CAPTURING_COMPLETE, ABORTED};
    enum ActionChoice{NO_ACTION, START_SLEW, START_FOCUSING, START_GUIDING, START_ASTROMETRY, START_CAPTURING};
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

    int getScore() const;
    void setScore(int value);

    int getFinishingHour() const;
    void setFinishingHour(int value);

    int getFinishingMinute() const;
    void setFinishingMinute(int value);

    bool getFocusCheck() const;
    void setFocusCheck(bool value);

    bool getAlignCheck() const;
    void setAlignCheck(bool value);

    StateChoice getState() const;
    void setState(const StateChoice &value);

    int getRowNumber() const;
    void setRowNumber(int value);

    bool getGuideCheck() const;
    void setGuideCheck(bool value);

    int getIsOk() const;
    void setIsOk(int value);

    bool getParkTelescopeCheck() const;
    void setParkTelescopeCheck(bool value);

    bool getWarmCCDCheck() const;
    void setWarmCCDCheck(bool value);

    bool getCloseDomeCheck() const;
    void setCloseDomeCheck(bool value);

private:
    StateChoice state = IDLE;
    ActionChoice action = NO_ACTION;
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
    int finishingHour;
    int finishingMinute;
    int score;
    int isOk;
    int rowNumber;

    bool NowCheck;
    bool specificTime;
    bool specificAlt;
    bool moonSeparationCheck;
    bool meridianFlip;

    bool whenSeqCompCheck;
    bool loopCheck;
    bool onTimeCheck;

    bool focusCheck;
    bool alignCheck;
    bool guideCheck;

    bool parkTelescopeCheck;
    bool warmCCDCheck;
    bool closeDomeCheck;
};
