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
#include <QtDBus/QtDBus>

#include <config-kstars.h>

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
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Align")

public:
    Align();
    ~Align();

    typedef enum { AZ_INIT, AZ_FIRST_TARGET, AZ_SYNCING, AZ_SLEWING, AZ_SECOND_TARGET, AZ_CORRECTING, AZ_FINISHED } AZStage;
    typedef enum { ALT_INIT, ALT_FIRST_TARGET, ALT_SYNCING, ALT_SLEWING, ALT_SECOND_TARGET, ALT_CORRECTING, ALT_FINISHED } ALTStage;

    Q_SCRIPTABLE bool selectCCD(QString device);
    Q_SCRIPTABLE Q_NOREPLY void startSovling(const QString &filename, bool isGenerated=true);
    Q_SCRIPTABLE Q_NOREPLY void selectGOTOMode(int mode);
    Q_SCRIPTABLE Q_NOREPLY void getSolutionResult(double &orientation, double &ra, double &dec);
    Q_SCRIPTABLE bool isSolverComplete() { return isSolverComplete_m; }
    Q_SCRIPTABLE bool isSolverSuccessful() { return isSolverSuccessful_m; }
    Q_SCRIPTABLE Q_NOREPLY void setExposure(double value);
    Q_SCRIPTABLE Q_NOREPLY void setBinning(int binX, int binY);
    Q_SCRIPTABLE Q_NOREPLY void setSolverArguments(const QString & value);
    Q_SCRIPTABLE Q_NOREPLY void setSolverSearchOptions(double ra, double dec, double radius);
    Q_SCRIPTABLE Q_NOREPLY void setSolverOptions(bool updateCoords, bool previewImage, bool verbose, bool useOAGT);


    void addCCD(ISD::GDInterface *newCCD, bool isPrimaryCCD);
    void setTelescope(ISD::GDInterface *newTelescope);
    void addGuideHead();    
    void syncCCDInfo();
    void generateArgs();    
    void appendLogText(const QString &);
    void clearLog();
    bool parserOK();
    bool isVerbose();

    QString getLogText() { return logText.join("\n"); }

public slots:


    void updateScopeCoords(INumberVectorProperty *coord);
    void checkCCD(int CCDNum=-1);
    void newFITS(IBLOB *bp);

    /* Solver */
    Q_SCRIPTABLE Q_NOREPLY void stopSolving();
    Q_SCRIPTABLE Q_NOREPLY void updateSolverType(bool useOnline);
    Q_SCRIPTABLE bool captureAndSolve();
    void solverFinished(double orientation, double ra, double dec);
    void solverFailed();

    /* CCD & Telescope Info */
    void syncTelescopeInfo();

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

    /* Reference Slew */
     Q_SCRIPTABLE Q_NOREPLY void loadAndSlew(QUrl fileURL = QUrl());

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
    bool loadSlewMode;
    bool isSolverComplete_m;
    bool isSolverSuccessful_m;
    QStringList logText;

    double ccd_hor_pixel, ccd_ver_pixel, focal_length, aperture, fov_x, fov_y;
    int ccd_width, ccd_height;
    double sOrientation, sRA, sDEC;

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
    OnlineAstrometryParser *onlineParser;
    OfflineAstrometryParser *offlineParser;


};

}

#endif  // ALIGN_H
