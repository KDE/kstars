/*  Ekos Capture tool
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef CAPTURE_H
#define CAPTURE_H

#include "capture.h"

#include <QTimer>

#include <KFileItemList>
#include <KDirLister>

#include "ui_capture.h"

#include "fitsviewer/fitscommon.h"
#include "indi/indistd.h"
#include "indi/indiccd.h"

class QProgressIndicator;
class QTableWidgetItem;

namespace Ekos
{


class SequenceJob
{
    public:

    typedef enum { JOB_IDLE, JOB_BUSY, JOB_ERROR, JOB_ABORTED, JOB_DONE } JOBStatus;
    typedef enum { CAPTURE_OK, CAPTURE_FRAME_ERROR, CAPTURE_BIN_ERROR} CAPTUREResult;

    SequenceJob();

    CAPTUREResult capture(bool isDark=false);
    void abort();
    void done();
    void prepareCapture();

    JOBStatus getStatus() { return status; }
    const QString & getStatusString() { return statusStrings[status]; }
    bool isPreview() { return preview;}
    int getDelay() { return delay;}
    int getCount() { return count;}
    unsigned int getCompleted() { return completed; }
    const QString & getPrefix() { return prefix;}
    double getExposure() const { return exposure;}

    void setActiveCCD(ISD::CCD *ccd) { activeCCD = ccd; }
    ISD::CCD *getActiveCCD() { return activeCCD;}

    void setActiveFilter(ISD::GDInterface *filter) { activeFilter = filter; }
    ISD::GDInterface *getActiveFilter() { return activeFilter;}

    void setActiveChip(ISD::CCDChip *chip) { activeChip = chip; }
    ISD::CCDChip *getActiveChip() { return activeChip;}

    void setFilter(int pos, const QString & name);
    void setFrameType(int type, const QString & name);
    void setCaptureFilter(FITSScale capFilter) { captureFilter = capFilter; }
    void setISOMode(bool mode) { isoMode = mode; }
    void setPreview(bool enable) { preview = enable; }
    void setShowFITS(bool enable) { showFITS = enable; }
    void setPrefix(const QString &cprefix) { prefix = cprefix;}
    void setFrame(int in_x, int in_y, int in_w, int in_h) { x=in_x; y=in_y; w=in_w; h=in_h; }
    void setBin(int xbin, int ybin) { binX = xbin; binY=ybin;}
    void setDelay(int in_delay) { delay = in_delay; }
    void setCount(int in_count) { count = in_count;}
    void setImageType(int type) { imageType = type;}
    void setExposure(double duration) { exposure = duration;}
    void setStatusCell(QTableWidgetItem *cell) { statusCell = cell; }
    void setCompleted(unsigned int in_completed) { completed = in_completed;}

    double getExposeLeft() const;
    void setExposeLeft(double value);

private:

    QStringList statusStrings;
    ISD::CCDChip *activeChip;
    ISD::CCD *activeCCD;
    ISD::GDInterface *activeFilter;

    double exposure;
    int frameType;
    QString frameTypeName;
    int filterPos;
    QString filter;
    int imageType;
    int binX, binY;
    int x,y,w,h;
    QString prefix;
    int count;
    int delay;
    bool isoMode;
    bool preview;
    bool showFITS;
    unsigned int completed;
    double exposeLeft;
    FITSScale captureFilter;
    QTableWidgetItem *statusCell;

    JOBStatus status;


};

class Capture : public QWidget, public Ui::Capture
{

    Q_OBJECT

public:

    enum { CALIBRATE_NONE, CALIBRATE_START, CALIBRATE_DONE };

    Capture();
    ~Capture();
    void addCCD(ISD::GDInterface *newCCD);
    void addFilter(ISD::GDInterface *newFilter);
    void addGuideHead(ISD::GDInterface *newCCD);

    void syncFrameType(ISD::GDInterface *ccd);

    void appendLogText(const QString &);
    void clearLog();
    QString getLogText() { return logText.join("\n"); }

    /* Capture */
    void updateSequencePrefix( const QString &newPrefix);
public slots:

    /* Capture */
    void startSequence();
    void stopSequence();
    void captureOne();
    void captureImage();
    void newFITS(IBLOB *bp);
    void checkCCD(int CCDNum);
    void checkFilter(int filterNum);

    void addJob(bool preview=false);
    void removeJob();

    void moveJobUp();
    void moveJobDown();

    void enableGuideLimits();
    void setGuideDeviation(int axis, double deviation);

    void updateCaptureProgress(ISD::CCDChip *tChip, double value);
    void checkSeqBoundary(const KFileItemList & items);

signals:
        void newLog();

private:

    void executeJob(SequenceJob *job);

    /* Capture */
    KDirLister          *seqLister;
    double	seqExpose;
    int	seqTotalCount;
    int	seqCurrentCount;
    int	seqDelay;
    int     retries;
    QTimer *seqTimer;
    QString		seqPrefix;
    int			seqCount;

    int calibrationState;
    bool useGuideHead;

    SequenceJob *activeJob;

    QList<ISD::CCD *> CCDs;

    ISD::CCDChip *targetChip;

    // They're generic GDInterface because they could be either ISD::CCD or ISD::Filter
    QList<ISD::GDInterface *> Filters;

    QList<SequenceJob *> jobs;

    ISD::CCD *currentCCD;
    ISD::GDInterface *currentFilter;

    ITextVectorProperty *filterName;
    INumberVectorProperty *filterSlot;

    QStringList logText;

    QProgressIndicator *pi;

    // Guide Deviation
    bool deviationDetected;
    double deviation_axis[2];

};

}

#endif  // CAPTURE_H
