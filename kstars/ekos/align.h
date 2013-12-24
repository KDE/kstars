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

class AstrometryParser;
class OnlineAstrometryParser;
class OfflineAstrometryParser;

class Align : public QWidget, public Ui::Align
{

    Q_OBJECT

public:
    Align();
    ~Align();

    typedef enum { AZ_INIT, AZ_FIRST_TARGET, AZ_SYNCING, AZ_SLEWING, AZ_SECOND_TARGET, AZ_CORRECTING, AZ_FINISHED } AZStage;
    typedef enum { ALT_INIT, ALT_FIRST_TARGET, ALT_SYNCING, ALT_SLEWING, ALT_SECOND_TARGET, ALT_CORRECTING, ALT_FINISHED } ALTStage;

    void addCCD(ISD::GDInterface *newCCD);
    void setTelescope(ISD::GDInterface *newTelescope);
    void addGuideHead();
    void syncTelescopeInfo();
    void syncCCDInfo();
    void generateArgs();
    void startSovling(const QString &filename);
    void appendLogText(const QString &);
    void clearLog();
    bool parserOK();
    bool isVerbose();

    QString getLogText() { return logText.join("\n"); }

public slots:


    void updateScopeCoords(INumberVectorProperty *coord);
    void checkCCD(int CCDNum);
    void newFITS(IBLOB *bp);

    /* Solver */
    bool capture();
    void stopSolving();
    void solverFinished(double orientation, double ra, double dec);
    void solverFailed();
    void updateSolverType(bool useOnline);

    /* Solver Options */
    void checkLineEdits();
    void copyCoordsToBoxes();
    void clearCoordBoxes();


    /* Polar Alignment */
    void checkPolarAlignment();
    void measureAltError();
    void measureAzError();
    void correctAzError();
    void correctAltError();

signals:
        void newLog();

private:
    void calculateFOV();
    void executeMode();
    void executeGOTO();
    void executePolarAlign();
    void Sync();
    void SlewToTarget();
    void calculatePolarError(double initRA, double initDEC, double finalRA, double finalDEC);
    void getFormattedCoords(double ra, double dec, QString &ra_str, QString &dec_str);

    ISD::Telescope *currentTelescope;
    ISD::CCD *currentCCD;
    QList<ISD::CCD *> CCDs;

    Ekos::Capture *captureP;

    bool useGuideHead;
    bool canSync;
    QStringList logText;

    double ccd_hor_pixel, ccd_ver_pixel, focal_length, aperture, fov_x, fov_y;
    int ccd_width, ccd_height;

    SkyPoint alignCoord;
    SkyPoint targetCoord;
    SkyPoint telescopeCoord;

    QProgressIndicator *pi;


    QTime solverTimer;
    AZStage azStage;
    ALTStage altStage;
    double azDeviation, altDeviation;
    double decDeviation;

    static const double RAMotion;
    static const float SIDRATE;

    AstrometryParser *parser;
    #ifdef HAVE_QJSON
    OnlineAstrometryParser *onlineParser;
    #endif
    OfflineAstrometryParser *offlineParser;


};

}

#endif  // ALIGN_H
