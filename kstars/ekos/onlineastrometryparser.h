/*  Astrometry.net Parser
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef ONLINEASTROMETRYPARSER_H
#define ONLINEASTROMETRYPARSER_H

#include <qjson/parser.h>
#include <qjson/serializer.h>

#include <QTime>
#include <QtNetwork/QNetworkAccessManager>

#include "astrometryparser.h"

class QNetworkReply;

namespace Ekos
{

class Align;

class OnlineAstrometryParser: public AstrometryParser
{
        Q_OBJECT

public:
    OnlineAstrometryParser();
    virtual ~OnlineAstrometryParser();

    virtual void setAlign(Align *_align) { align = _align;}
    virtual bool init();
    virtual void verifyIndexFiles(double fov_x, double fov_y);
    virtual bool startSovler(const QString &filename, const QStringList &args);
    virtual bool stopSolver();

    typedef enum { NO_STAGE, AUTH_STAGE, UPLOAD_STAGE, JOB_ID_STAGE, JOB_STATUS_STAGE, JOB_CALIBRATION_STAGE } WorkflowStage;

public slots:
    void onResult(QNetworkReply* reply);
    void uploadFile();
    void getJobID();
    void checkJobs();
    void checkJobCalibration();
signals:
    void authenticateFinished();
    void uploadFinished();
    void jobIDFinished();
    void jobFinished();
    void solverFinished(double orientation, double ra, double dec);
    void solverFailed();
private:

    WorkflowStage workflowStage;
    QNetworkAccessManager networkManager;

    QString sessionKey;
    int subID;
    int jobID;

    int job_retries, solver_retries;


    QTime solverTimer;
    //QEventLoop *pELoop;

    QJson::Parser parser;
    QJson::Serializer serializer;

    QString filename, apiURL;
    double lowerScale, upperScale, center_ra, center_dec, radius;
    double parity,ra,dec,orientation;

    void authenticate();

    Align *align;

};

}

#endif // ONLINEASTROMETRYPARSER_H
