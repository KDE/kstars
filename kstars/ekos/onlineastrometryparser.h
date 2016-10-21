/*  Astrometry.net Parser
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef ONLINEASTROMETRYPARSER_H
#define ONLINEASTROMETRYPARSER_H

#include <QTime>
#include <QtNetwork/QNetworkAccessManager>

#include "astrometryparser.h"

class QNetworkReply;

namespace Ekos
{

class Align;

/**
 * @class  OnlineAstrometryParser
 * OnlineAstrometryParser invokes the online services provided by astrometry.net solver to find solutions to captured images.
 *
 * @authro Jasem Mutlaq
 */
class OnlineAstrometryParser: public AstrometryParser
{
        Q_OBJECT

public:
    OnlineAstrometryParser();
    virtual ~OnlineAstrometryParser();

    virtual void setAlign(Align *_align) { align = _align;}
    virtual bool init();
    virtual void verifyIndexFiles(double fov_x, double fov_y);
    virtual bool startSovler(const QString &filename, const QStringList &args, bool generated=true);
    virtual bool stopSolver();

    typedef enum { NO_STAGE, AUTH_STAGE, UPLOAD_STAGE, JOB_ID_STAGE, JOB_STATUS_STAGE, JOB_CALIBRATION_STAGE } WorkflowStage;

public slots:
    void onResult(QNetworkReply* reply);
    void uploadFile();
    void getJobID();
    void checkJobs();
    void checkJobCalibration();
    void resetSolver();
signals:
    void authenticateFinished();
    void uploadFinished();
    void jobIDFinished();
    void jobFinished();
private:

    void authenticate();

    WorkflowStage workflowStage;
    QNetworkAccessManager *networkManager;

    QString sessionKey;
    int subID;
    int jobID;
    int job_retries, solver_retries;

    QTime solverTimer;
    QString filename;
    double lowerScale, upperScale, center_ra, center_dec, radius, pixscale;
    double parity,ra,dec,orientation;
    int downsample_factor;
    bool isGenerated;
    Align *align;

};

}

#endif // ONLINEASTROMETRYPARSER_H
