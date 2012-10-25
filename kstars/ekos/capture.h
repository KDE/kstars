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

    typedef enum { JOB_IDLE, JOB_BUSY, JOB_ERROR, JOB_DONE } JOBStatus;

    static QStringList statusStrings;

    SequenceJob();

    private:

    ISD::CCDChip *activeChip;
    ISD::CCD *activeCCD;
    ISD::GDInterface *activeFilter;

    double exposure;
    int frameType;
    int filterPos;
    int imageType;
    int binX, binY;
    int x,y,w,h;
    QString prefix;
    int count;
    int delay;
    bool isoMode;
    bool preview;
    bool showFITS;
    QTableWidgetItem *statusCell;

    JOBStatus status;

    friend class Capture;
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
    int         jobCount;
    int         jobIndex;

    ISD::CCD *currentCCD;
    ISD::GDInterface *currentFilter;

    ITextVectorProperty *filterName;
    INumberVectorProperty *filterSlot;

    QStringList logText;

    QProgressIndicator *pi;

};

}

#endif  // CAPTURE_H
