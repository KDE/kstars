/*  Astrometry.net Parser
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QUrl>
#include <QVariantMap>
#include <QDebug>
#include <QHttpMultiPart>
#include <QFile>

#include "onlineastrometryparser.h"
#include "align.h"

#include "Options.h"

#define JOB_RETRY_DURATION      2000 /* 2000 ms */
#define JOB_RETRY_ATTEMPTS      10
#define SOLVER_RETRY_DURATION   2000 /* 2000 ms */
#define SOLVER_RETRY_ATTEMPTS   90

namespace Ekos
{


OnlineAstrometryParser::OnlineAstrometryParser() : AstrometryParser()
{
    job_retries=0;
    solver_retries=0;

    connect(&networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onResult(QNetworkReply*)));

    connect(this, SIGNAL(authenticateFinished()), this, SLOT(uploadFile()));
    connect(this, SIGNAL(uploadFinished()), this, SLOT(getJobID()));
    connect(this, SIGNAL(jobIDFinished()), this, SLOT(checkJobs()));
    connect(this, SIGNAL(jobFinished()), this, SLOT(checkJobCalibration()));

    apiURL = QString("%1/api/").arg(Options::astrometryAPIURL());

}

OnlineAstrometryParser::~OnlineAstrometryParser()
{
}

bool OnlineAstrometryParser::init()
{
    return true;
}

void OnlineAstrometryParser::verifyIndexFiles(double fov_x, double fov_y)
{
    (void)fov_x;
    (void)fov_y;
}

bool OnlineAstrometryParser::startSovler(const QString &in_filename, const QStringList &args)
{
    bool ok;

    if (networkManager.networkAccessible() == false)
    {
        align->appendLogText(i18n("Error: No connection to the internet."));
        emit solverFailed();
        return false;
    }

    solverTimer.start();

    filename = in_filename;
    for (int i=0; i < args.count(); i++)
    {
        if (args[i] == "-L")
            lowerScale = args[i+1].toDouble(&ok);
        else if (args[i] == "-H")
            upperScale = args[i+1].toDouble(&ok);
        else if (args[i] == "-3")
            center_ra = args[i+1].toDouble(&ok);
        else if (args[i] == "-4")
            center_dec = args[i+1].toDouble(&ok);
        else if (args[i] == "-5")
            radius = args[i+1].toDouble(&ok);
    }

    if (sessionKey.isEmpty())
        authenticate();
    else
        uploadFile();


   return true;
}

bool OnlineAstrometryParser::stopSolver()
{

    workflowStage = NO_STAGE;

    return true;
}

void OnlineAstrometryParser::authenticate()
{
    QNetworkRequest request;
    bool ok;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QUrl url(apiURL);
    QString method = "login";
    url.setPath(QString("%1%2").arg(url.path()).arg(method));
    request.setUrl(url);

    QVariantMap apiReq;
    apiReq.insert("apikey", Options::astrometryAPIKey());
    QByteArray json = serializer.serialize(apiReq, &ok);

    QString json_request = QString("request-json=%1").arg(QString(json));

    workflowStage = AUTH_STAGE;

    networkManager.post(request, json_request.toUtf8());
}

void OnlineAstrometryParser::uploadFile()
{
    bool ok;
    QNetworkRequest request;

    QFile fitsFile(filename);
    bool rc = fitsFile.open(QIODevice::ReadOnly);
    if (rc == false)
    {
        align->appendLogText(i18n("Failed to open file %1. %2", filename, fitsFile.errorString()));
        emit solverFailed();
        return;
    }

    QUrl url(apiURL);
    QString method = "upload/";
    url.setPath(QString("%1%2").arg(url.path()).arg(method));
    request.setUrl(url);

    QHttpMultiPart reqEntity(QHttpMultiPart::FormDataType);

    QVariantMap uploadReq;
    uploadReq.insert("publicly_visible", "n");
    uploadReq.insert("allow_modifications", "n");
    uploadReq.insert("session", sessionKey);
    uploadReq.insert("allow_commercial_use", "n");
    uploadReq.insert("scale_units", "arcminwidth");
    uploadReq.insert("scale_type", "ul");
    uploadReq.insert("scale_lower", lowerScale);
    uploadReq.insert("scale_upper", upperScale);
    uploadReq.insert("center_ra", center_ra);
    uploadReq.insert("center_dec", center_dec);
    uploadReq.insert("radius", radius);

    QByteArray json = serializer.serialize(uploadReq, &ok);
    QHttpPart jsonPart;

    jsonPart.setHeader(QNetworkRequest::ContentTypeHeader, "application/text/plain");
    jsonPart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"request-json\"");
    jsonPart.setBody(json);

    QHttpPart filePart;

    filePart.setHeader(QNetworkRequest::ContentTypeHeader,"application/octet-stream");
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QString("form-data; name=\"file\"; filename=\"%1\"").arg(filename));
    filePart.setBodyDevice(&fitsFile);

    reqEntity.append(jsonPart);
    reqEntity.append(filePart);

    workflowStage = UPLOAD_STAGE;

    align->appendLogText(i18n("Uploading file..."));

    QEventLoop loop;
    networkManager.post(request, &reqEntity);
    loop.exec();

}

void OnlineAstrometryParser::getJobID()
{
    //qDebug() << "With subID " << subID << endl;

    QNetworkRequest request;
    QUrl getJobID(QString("%1/submissions/%2").arg(apiURL).arg(subID));

    request.setUrl(getJobID);

    workflowStage = JOB_ID_STAGE;

    networkManager.get(request);
}

void OnlineAstrometryParser::checkJobs()
{
    //qDebug() << "with jobID " << jobID << endl;

    QNetworkRequest request;
    QUrl getJobStatus(QString("%1/jobs/%2").arg(apiURL).arg(jobID));

    request.setUrl(getJobStatus);

    workflowStage = JOB_STATUS_STAGE;

    networkManager.get(request);

}

void OnlineAstrometryParser::checkJobCalibration()
{

    QNetworkRequest request;
    QUrl getCablirationResult(QString("%1/jobs/%2/calibration").arg(apiURL).arg(jobID));

    request.setUrl(getCablirationResult);

    workflowStage = JOB_CALIBRATION_STAGE;

    networkManager.get(request);
}

void OnlineAstrometryParser::onResult(QNetworkReply* reply)
{
    bool ok;
    QString status;
    QList<QVariant> jsonArray;
    int elapsed;

    if (workflowStage == NO_STAGE)
        return;

    if (reply->error() != QNetworkReply::NoError)
    {
        align->appendLogText(reply->errorString());
        emit solverFailed();
        return;
    }

    QString json = (QString) reply->readAll();

    QVariantMap result = parser.parse (json.toUtf8(), &ok).toMap();

     if (ok == false)
     {
         align->appendLogText(QString("JSon error during parsing."));
         emit solverFailed();
         return;
     }

     if (align->isVerbose())
         align->appendLogText(json);

     switch (workflowStage)
     {
        case AUTH_STAGE:
         status = result["status"].toString();
         if (status != "success")
         {
             align->appendLogText(i18n("Astrometry.net authentication failed. Check the validity of the Astrometry.net API Key."));
             emit solverFailed();
             return;
         }

         sessionKey = result["session"].toString();

         if (align->isVerbose())
            align->appendLogText(i18n("Authentication to astrometry.net is successful. Session: %1", sessionKey));

         emit authenticateFinished();
         break;

       case UPLOAD_STAGE:
         status = result["status"].toString();
         if (status != "success")
         {
             align->appendLogText(i18n("Upload failed."));
             emit solverFailed();
             return;
         }

         subID = result["subid"].toInt(&ok);

         if (ok == false)
         {
             align->appendLogText(i18n("Parsing submission ID failed."));
             emit solverFailed();
             return;
         }

         align->appendLogText(i18n("Upload complete. Waiting for astrometry.net solver to complete..."));
         emit uploadFinished();
         break;

       case JOB_ID_STAGE:
         jsonArray = result["jobs"].toList();

         if (jsonArray.isEmpty())
             jobID = 0;
         else
             jobID = jsonArray.first().toInt(&ok);

         if (jobID == 0 || !ok)
         {
             if (job_retries++ < JOB_RETRY_ATTEMPTS)
             {
                QTimer::singleShot(JOB_RETRY_DURATION, this, SLOT(getJobID()));
                return;
             }
             else
             {
                 align->appendLogText(i18n("Failed to retrieve job ID."));
                 emit solverFailed();
                 return;
             }
         }

         job_retries=0;
         emit  jobIDFinished();
         break;

       case JOB_STATUS_STAGE:
         status = result["status"].toString();
         if (status == "success")
             emit jobFinished();
         else if (status == "solving" || status == "error")
         {
             if (status == "solving" && solver_retries++ < SOLVER_RETRY_ATTEMPTS)
             {
                QTimer::singleShot(SOLVER_RETRY_DURATION, this, SLOT(checkJobs()));
                return;
             }
             else
             {
                 align->appendLogText(i18n("Solver timed out."));
                 emit solverFailed();
                 return;
             }
         }
         solver_retries=0;
         break;

     case JOB_CALIBRATION_STAGE:
         parity = result["parity"].toDouble(&ok);
         if (ok == false)
         {
             align->appendLogText(i18n("Error parsing parity."));
             emit solverFailed();
             return;
         }
         orientation = result["orientation"].toDouble(&ok);
         if (ok == false)
         {
             align->appendLogText(i18n("Error parsing orientation."));
             emit solverFailed();
             return;
         }
         orientation *= parity;
         ra  = result["ra"].toDouble(&ok);
         if (ok == false)
         {
             align->appendLogText(i18n("Error parsing RA."));
             emit solverFailed();
             return;
         }
         dec = result["dec"].toDouble(&ok);
         if (ok == false)
         {
             align->appendLogText(i18n("Error parsing DEC."));
             emit solverFailed();
             return;
         }

         elapsed = (int) round(solverTimer.elapsed()/1000.0);
         align->appendLogText(i18np("Solver completed in %1 second.", "Solver completed in %1 seconds.", elapsed));
         emit solverFinished(orientation, ra, dec);

         break;

     default:
         break;
     }

}

}

#include "onlineastrometryparser.moc"
