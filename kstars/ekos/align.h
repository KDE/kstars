/*  Ekos Polar Alignment Tool
    Copyright (C) 2013 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef ALIGN_H
#define ALIGN_H

#include <QProcess>
#include <QTime>

#include "capture.h"

#include "ui_align.h"

#include "indi/inditelescope.h"
#include "indi/indistd.h"


namespace Ekos
{

class Align : public QWidget, public Ui::Align
{

    Q_OBJECT

public:
    Align();
    ~Align();

    void setCCD(ISD::GDInterface *newCCD);
    void setTelescope(ISD::GDInterface *newTelescope);
    void addGuideHead();
    void syncTelescopeInfo();
    void syncCCDInfo();
    void generateArgs();
    void startSovling(const QString &filename);
    void appendLogText(const QString &);
    void clearLog();
    QString getLogText() { return logText.join("\n"); }

public slots:

    void stopSolving();
    bool capture();
    void solverComplete(int exist_status);
    void logSolver();
    void wcsinfoComplete(int exist_status);
    void updateScopeCoords(INumberVectorProperty *coord);
    void checkCCD(int CCDNum);
    void newFITS(IBLOB *bp);

signals:
        void newLog();

private:
    void calculateFOV();
    void executeMode();
    void executeGOTO();
    void executePolarAlign();
    void Sync();
    void SlewToTarget();
    void getFormattedCoords(double ra, double dec, QString &ra_str, QString &dec_str);

    ISD::Telescope *currentTelescope;
    ISD::CCD *currentCCD;
    QList<ISD::CCD *> CCDs;

    Ekos::Capture *captureP;

    bool useGuideHead;

    QStringList logText;

    double ccd_hor_pixel, ccd_ver_pixel, focal_length, aperture, fov_x, fov_y;
    int ccd_width, ccd_height;

    SkyPoint alignCoord;
    SkyPoint targetCoord;

    QProgressIndicator *pi;
    QProcess solver;
    QProcess wcsinfo;
    QTime solverTimer;
    QString fitsFile;

};

}

#endif  // ALIGN_H
