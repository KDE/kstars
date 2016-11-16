/*  Ekos Capture tool
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef SEQUENCEJOB_H
#define SEQUENCEJOB_H

#include <QTableWidgetItem>

#include "indi/indistd.h"
#include "indi/indiccd.h"

/**
 *@class SequenceJob
 *@short Sequence Job is a container for the details required to capture a series of images.
 *@author Jasem Mutlaq
 *@version 1.0
 */
namespace Ekos
{

class SequenceJob : public QObject
{
    Q_OBJECT    

    public:

    typedef enum { JOB_IDLE, JOB_BUSY, JOB_ERROR, JOB_ABORTED, JOB_DONE } JOBStatus;
    typedef enum { CAPTURE_OK, CAPTURE_FRAME_ERROR, CAPTURE_BIN_ERROR, CAPTURE_FILTER_BUSY, CAPTURE_FOCUS_ERROR} CAPTUREResult;

    SequenceJob();
    ~SequenceJob() {}

    CAPTUREResult capture(bool noCaptureFilter);
    void reset();
    void abort();
    void done();
    void prepareCapture();

    JOBStatus getStatus() { return status; }
    const QString & getStatusString() { return statusStrings[status]; }
    bool isPreview() { return preview;}
    int getDelay() { return delay;}
    int getCount() { return count;}
    unsigned int getCompleted() { return completed; }    
    const QString & getRawPrefix() { return rawPrefix;}
    double getExposure() const { return exposure;}

    void setActiveCCD(ISD::CCD *ccd) { activeCCD = ccd; }
    ISD::CCD *getActiveCCD() { return activeCCD;}

    void setActiveFilter(ISD::GDInterface *filter) { activeFilter = filter; }
    ISD::GDInterface *getActiveFilter() { return activeFilter;}

    void setActiveChip(ISD::CCDChip *chip) { activeChip = chip; }
    ISD::CCDChip *getActiveChip() { return activeChip;}

    void setFITSDir(const QString &dir) { fitsDir = dir;}
    const QString & getFITSDir() { return fitsDir; }

    void setTargetFilter(int pos, const QString & name);
    int getTargetFilter() { return targetFilter;}
    int getCurrentFilter() const;
    void setCurrentFilter(int value);

    const QString &getFilterName() { return filter; }

    void setFrameType(int type, const QString & name);
    int getFrameType() { return frameType;}

    void setCaptureFilter(FITSScale capFilter) { captureFilter = capFilter; }
    FITSScale getCaptureFilter() { return captureFilter;}

    void setPreview(bool enable) { preview = enable; }
    void setFullPrefix(const QString &cprefix) { fullPrefix = cprefix;}
    const QString & getFullPrefix() { return fullPrefix;}
    void setFrame(int in_x, int in_y, int in_w, int in_h) { x=in_x; y=in_y; w=in_w; h=in_h; }

    int getSubX() { return x;}
    int getSubY() { return y;}
    int getSubW() { return w;}
    int getSubH() { return h;}

    void setBin(int xbin, int ybin) { binX = xbin; binY=ybin;}
    int getXBin() { return binX; }
    int getYBin() { return binY; }

    void setDelay(int in_delay) { delay = in_delay; }
    void setCount(int in_count) { count = in_count;}    
    void setExposure(double duration) { exposure = duration;}
    void setStatusCell(QTableWidgetItem *cell) { statusCell = cell; }
    void setCompleted(unsigned int in_completed) { completed = in_completed;}
    int getISOIndex() const;
    void setISOIndex(int value);

    double getExposeLeft() const;
    void setExposeLeft(double value);
    void resetStatus();

    void setPrefixSettings(const QString &rawFilePrefix, bool filterEnabled, bool exposureEnabled, bool tsEnabled);
    void getPrefixSettings(QString &rawFilePrefix, bool &filterEnabled, bool &exposureEnabled, bool &tsEnabled);

    bool isFilterPrefixEnabled() { return filterPrefixEnabled; }
    bool isExposurePrefixEnabled() { return expPrefixEnabled; }
    bool isTimestampPrefixEnabled() { return timeStampPrefixEnabled;}

    double getCurrentTemperature() const;
    void setCurrentTemperature(double value);

    double getTargetTemperature() const;
    void setTargetTemperature(double value);    

    double getTargetADU() const;
    void setTargetADU(double value);

    int getCaptureRetires() const;
    void setCaptureRetires(int value);

    FlatFieldSource getFlatFieldSource() const;
    void setFlatFieldSource(const FlatFieldSource &value);

    FlatFieldDuration getFlatFieldDuration() const;
    void setFlatFieldDuration(const FlatFieldDuration &value);

    SkyPoint getWallCoord() const;
    void setWallCoord(const SkyPoint &value);

    bool isPreMountPark() const;
    void setPreMountPark(bool value);

    bool isPreDomePark() const;
    void setPreDomePark(bool value);

    bool getEnforceTemperature() const;
    void setEnforceTemperature(bool value);

    QString getRootFITSDir() const;
    void setRootFITSDir(const QString &value);

    bool getFilterPostFocusReady() const;
    void setFilterPostFocusReady(bool value);

    QString getPostCaptureScript() const;
    void setPostCaptureScript(const QString &value);            

    ISD::CCD::UploadMode getUploadMode() const;
    void setUploadMode(const ISD::CCD::UploadMode &value);

    QString getRemoteDir() const;
    void setRemoteDir(const QString &value);

signals:
    void prepareComplete();
    void checkFocus();

private:

    QStringList statusStrings;
    ISD::CCDChip *activeChip;
    ISD::CCD *activeCCD;
    ISD::GDInterface *activeFilter;

    double exposure;
    int frameType;
    QString frameTypeName;
    int targetFilter;
    int currentFilter;

    QString filter;    
    int binX, binY;
    int x,y,w,h;
    QString fullPrefix;
    int count;
    int delay;    
    bool preview;
    bool filterReady, temperatureReady, filterPostFocusReady, prepareReady;
    bool enforceTemperature;
    int isoIndex;
    int captureRetires;
    unsigned int completed;
    double exposeLeft;
    double currentTemperature, targetTemperature;
    FITSScale captureFilter;
    QTableWidgetItem *statusCell;
    QString fitsDir;
    QString rootFITSDir;
    QString postCaptureScript;

    //TODO getters and setters
    ISD::CCD::UploadMode uploadMode = ISD::CCD::UPLOAD_CLIENT;

    // TODO getters and settings
    QString remoteDir;

    bool typePrefixEnabled, filterPrefixEnabled, expPrefixEnabled, timeStampPrefixEnabled;
    QString rawPrefix;

    JOBStatus status;

    // Flat field variables
    struct
    {
        double targetADU;
        FlatFieldSource flatFieldSource;
        FlatFieldDuration flatFieldDuration;
        SkyPoint wallCoord;
        bool preMountPark;
        bool preDomePark;

    } calibrationSettings;



};

}

#endif  // SEQUENCEJOB_H
