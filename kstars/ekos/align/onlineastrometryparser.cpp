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
#include <QJsonObject>
#include <QJsonDocument>

#include <KLocalizedString>

#include "onlineastrometryparser.h"
#include "align.h"

#include "Options.h"

#include "fitsviewer/fitsdata.h"

#define JOB_RETRY_DURATION      2000 /* 2000 ms */
#define JOB_RETRY_ATTEMPTS      15
#define SOLVER_RETRY_DURATION   2000 /* 2000 ms */
#define SOLVER_RETRY_ATTEMPTS   90

namespace Ekos
{


OnlineAstrometryParser::OnlineAstrometryParser() : AstrometryParser()
{
    job_retries=0;
    solver_retries=0;

    networkManager = new QNetworkAccessManager(this);

    connect(this, SIGNAL(authenticateFinished()), this, SLOT(uploadFile()));
    connect(this, SIGNAL(uploadFinished()), this, SLOT(getJobID()));
    connect(this, SIGNAL(jobIDFinished()), this, SLOT(checkJobs()));
    connect(this, SIGNAL(jobFinished()), this, SLOT(checkJobCalibration()));

    // Reset parity on solver failure
    connect(this, &OnlineAstrometryParser::solverFailed, this, [&]() { parity = -1;});

    connect(this, SIGNAL(solverFailed()), this, SLOT(resetSolver()));
    connect(this, SIGNAL(solverFinished(double,double,double, double)), this, SLOT(resetSolver()));

    downsample_factor = 0;
    isGenerated = true;

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

bool OnlineAstrometryParser::startSovler(const QString &in_filename, const QStringList &args, bool generated)
{
    bool ok;

    isGenerated = generated;

    job_retries=0;

    if (networkManager->networkAccessible() == false)
    {
        align->appendLogText(i18n("Error: No connection to the internet."));
        emit solverFailed();
        return false;
    }

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
        else if (args[i] == "--downsample")
            downsample_factor = args[i+1].toInt(&ok);
        else if (args[i] == "--parity")
        {
            QString arg = args[i+1];
            if (arg == "both")
                parity = 2;
            else if (arg == "pos")
                parity = 0;
            else
                parity = 1;
        }
    }

    connect(networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onResult(QNetworkReply*)));

    solverTimer.start();

    if (sessionKey.isEmpty())
        authenticate();
    else
        uploadFile();


   return true;
}

void OnlineAstrometryParser::resetSolver()
{
    stopSolver();
}

bool OnlineAstrometryParser::stopSolver()
{

    workflowStage = NO_STAGE;
    solver_retries=0;

    disconnect(networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onResult(QNetworkReply*)));

    return true;
}

void OnlineAstrometryParser::authenticate()
{
    QNetworkRequest request;
    //request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrl url(Options::astrometryAPIURL());
    url.setPath("/api/login");
    request.setUrl(url);

    QVariantMap apiReq;
    apiReq.insert("apikey", Options::astrometryAPIKey());
    //QByteArray json = serializer.serialize(apiReq, &ok);
    QJsonObject json = QJsonObject::fromVariantMap(apiReq);
    QJsonDocument json_doc(json);

    QString json_request = QString("request-json=%1").arg(QString(json_doc.toJson(QJsonDocument::Compact)));

    workflowStage = AUTH_STAGE;

    networkManager->post(request, json_request.toUtf8());
}

void OnlineAstrometryParser::uploadFile()
{
    QNetworkRequest request;

    QFile *fitsFile = new QFile(filename);
    bool rc = fitsFile->open(QIODevice::ReadOnly);
    if (rc == false)
    {
        align->appendLogText(i18n("Failed to open file %1. %2", filename, fitsFile->errorString()));
        delete (fitsFile);
        emit solverFailed();
        return;
    }

    QUrl url(Options::astrometryAPIURL());
    url.setPath("/api/upload");
    request.setUrl(url);

    QHttpMultiPart *reqEntity = new QHttpMultiPart(QHttpMultiPart::FormDataType);

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
    if (downsample_factor != 0)
        uploadReq.insert("downsample_factor", downsample_factor);
    if (parity != -1)
        uploadReq.insert("parity", parity);

    QJsonObject json = QJsonObject::fromVariantMap(uploadReq);
    QJsonDocument json_doc(json);

    QHttpPart jsonPart;

    jsonPart.setHeader(QNetworkRequest::ContentTypeHeader, "application/text/plain");
    jsonPart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"request-json\"");
    jsonPart.setBody(json_doc.toJson(QJsonDocument::Compact));

    QHttpPart filePart;

    filePart.setHeader(QNetworkRequest::ContentTypeHeader,"application/octet-stream");
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QString("form-data; name=\"file\"; filename=\"%1\"").arg(filename));
    filePart.setBodyDevice(fitsFile);

    // Re-parent so that it get deleted later
    fitsFile->setParent(reqEntity);

    reqEntity->append(jsonPart);
    reqEntity->append(filePart);

    workflowStage = UPLOAD_STAGE;

    align->appendLogText(i18n("Uploading file..."));

    QNetworkReply *reply = networkManager->post(request, reqEntity);

    // The entity should be deleted when reply is finished
    reqEntity->setParent(reply);
}

void OnlineAstrometryParser::getJobID()
{
    QNetworkRequest request;
    QUrl getJobID = QUrl(QString("%1/api/submissions/%2").arg(Options::astrometryAPIURL()).arg(subID));

    request.setUrl(getJobID);

    workflowStage = JOB_ID_STAGE;

    networkManager->get(request);
}

void OnlineAstrometryParser::checkJobs()
{
    //qDebug() << "with jobID " << jobID << endl;

    QNetworkRequest request;
    QUrl getJobStatus = QUrl(QString("%1/api/jobs/%2").arg(Options::astrometryAPIURL()).arg(jobID));

    request.setUrl(getJobStatus);

    workflowStage = JOB_STATUS_STAGE;

    networkManager->get(request);

}

void OnlineAstrometryParser::checkJobCalibration()
{

    QNetworkRequest request;
    QUrl getCablirationResult = QUrl(QString("%1/api/jobs/%2/calibration").arg(Options::astrometryAPIURL()).arg(jobID));

    request.setUrl(getCablirationResult);

    workflowStage = JOB_CALIBRATION_STAGE;

    networkManager->get(request);
}

void OnlineAstrometryParser::onResult(QNetworkReply* reply)
{
    bool ok;
    QJsonParseError parseError;
    QString status;
    QList<QVariant> jsonArray;
    int elapsed;

    if (workflowStage == NO_STAGE)
    {
        reply->abort();
        return;
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        align->appendLogText(reply->errorString());
        emit solverFailed();
        return;
    }

    QString json = (QString) reply->readAll();

    QJsonDocument json_doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);

     if (parseError.error != QJsonParseError::NoError)
     {
         align->appendLogText(i18n("JSon error during parsing (%1).", parseError.errorString()));
         emit solverFailed();
         return;
     }

     QVariant json_result = json_doc.toVariant();
     QVariantMap result = json_result.toMap();

     if (Options::solverVerbose())
         align->appendLogText(json_doc.toJson(QJsonDocument::Compact));

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

         if (Options::solverVerbose())
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
         if (isGenerated)
            QFile::remove(filename);
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
         else if (status == "solving")
         {
             if (status == "solving" && solver_retries++ < SOLVER_RETRY_ATTEMPTS)
             {
                QTimer::singleShot(SOLVER_RETRY_DURATION, this, SLOT(checkJobs()));
                return;
             }
             else
             {
                 align->appendLogText(i18n("Solver timed out."));
                 solver_retries=0;
                 emit solverFailed();
                 return;
             }
         }
         else if (status == "failure")
         {
             elapsed = (int) round(solverTimer.elapsed()/1000.0);
             align->appendLogText(i18np("Solver failed after %1 second.", "Solver failed after %1 seconds.", elapsed));
             emit solverFailed();
             return;
         }
         break;

     case JOB_CALIBRATION_STAGE:
         parity = result["parity"].toInt(&ok);
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

         pixscale = result["pixscale"].toDouble(&ok);
         if (ok == false)
         {
             align->appendLogText(i18n("Error parsing DEC."));
             emit solverFailed();
             return;
         }

         elapsed = (int) round(solverTimer.elapsed()/1000.0);
         align->appendLogText(i18np("Solver completed in %1 second.", "Solver completed in %1 seconds.", elapsed));
         emit solverFinished(orientation, ra, dec, pixscale);

         break;

     default:
         break;
     }

}

}


