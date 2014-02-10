/*  Ekos guide tool
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef guide_H
#define guide_H

#include <QTimer>

#include <KFileItemList>
#include <KDirLister>

#include "guide/common.h"
#include "guide.h"
#include "fitsviewer/fitscommon.h"
#include "indi/indistd.h"
#include "indi/inditelescope.h"
#include "indi/indiccd.h"
#include "ui_guide.h"

class QTabWidget;
class cgmath;
class rcalibration;
class rguider;
class FITSImage;

namespace Ekos
{


class Guide : public QWidget, public Ui::Guide
{

    Q_OBJECT

public:
    Guide();
    ~Guide();

    enum GuiderStage { CALIBRATION_STAGE, GUIDE_STAGE };

    void setCCD(ISD::GDInterface *newCCD);
    void setTelescope(ISD::GDInterface *newTelescope);
    void addST4(ISD::ST4 *newST4);
    void setAO(ISD::ST4 *newAO);

    void addGuideHead(ISD::GDInterface *ccd);
    void syncTelescopeInfo();
    void syncCCDInfo();

    void appendLogText(const QString &);
    void clearLog();


    void setDECSwap(bool enable);
    bool do_pulse( GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs );	// do dual-axis pulse (thread-safe)
    bool do_pulse( GuideDirection dir, int msecs );											// do single-axis pulse (thread-safe)

    QString getLogText() { return logText.join("\n"); }

    double getReticleAngle();

    void startRapidGuide();
    void stopRapidGuide();

public slots:

        void newFITS(IBLOB*);
        void newST4(int index);
        void processRapidStarData(ISD::CCDChip *targetChip, double dx, double dy, double fit);
        void setUseDarkFrame(bool enable) { useDarkFrame = enable;}
        void updateGuideDriver(double delta_ra, double delta_dec);
        bool capture();
        void viewerClosed();
        void dither();        
        void setSuspended(bool enable);
        void stopGuiding();

signals:
        void newLog();
        void guideReady();
        void newAxisDelta(double delta_ra, double delta_dec);
        void autoGuidingToggled(bool, bool);
        void ditherComplete();
        void ditherFailed();
        void ditherToggled(bool);
        void guideChipUpdated(ISD::CCDChip*);


private:
    void updateGuideParams();
    ISD::CCD *currentCCD;
    ISD::Telescope *currentTelescope;
    ISD::ST4* ST4Driver;
    ISD::ST4* AODriver;
    ISD::ST4* GuideDriver;

    QList<ISD::ST4*> ST4List;

    QTabWidget *tabWidget;

    GuiderStage guiderStage;

    cgmath *pmath;
    rcalibration *calibration;
    rguider *guider;

    bool useGuideHead;
    bool isSuspended;


    bool useDarkFrame;
    double darkExposure;
    FITSImage *darkImage;

    QStringList logText;

    double ccd_hor_pixel, ccd_ver_pixel, focal_length, aperture;
    bool rapidGuideReticleSet;

};

}

#endif  // guide_H
