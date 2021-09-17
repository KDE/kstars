/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "onlineastrometryparser.h"

#include "align.h"
#include "Options.h"

#include <KLocalizedString>

#include <QFile>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QVariantMap>

#define JOB_RETRY_DURATION    2000 /* 2000 ms */
#define JOB_RETRY_ATTEMPTS    90
#define SOLVER_RETRY_DURATION 2000 /* 2000 ms */

namespace Ekos
{
OnlineAstrometryParser::OnlineAstrometryParser() : AstrometryParser()
{
    networkManager = new QNetworkAccessManager(this);

    parity = Ekos::INVALID_VALUE;

    connect(this, SIGNAL(authenticateFinished()), this, SLOT(uploadFile()));
    connect(this, SIGNAL(uploadFinished()), this, SLOT(getJobID()));
    connect(this, SIGNAL(jobIDFinished()), this, SLOT(checkJobs()));
    connect(this, SIGNAL(jobFinished()), this, SLOT(checkJobCalibration()));

    // Reset parity on solver failure
    connect(this, &OnlineAstrometryParser::solverFailed, this, [&]()
    {
        parity = Ekos::INVALID_VALUE;
    });

    connect(this, SIGNAL(solverFailed()), this, SLOT(resetSolver()));
    connect(this, SIGNAL(solverFinished(double, double, double, double, bool)), this, SLOT(resetSolver()));
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

bool OnlineAstrometryParser::startSolver(const QString &in_filename, const QStringList &args, bool generated)
{
    bool ok = false;

    isGenerated = generated;

    job_retries = 0;

    //if (networkManager->networkAccessible() == false)
    // 2018-03-08 JM: Multiple users reported that network accessible is now failing if there is no internet connection
    // But LAN connection is established leading to failure of online solver with a local astrometry solver like ANSVR.
    // Therefore I am adding the exception below to remedy this situation for now.

    // 2018-07-13: Due to this bug --> https://bugreports.qt.io/browse/QTBUG-68613
    // The check will be disabled until it is resolved.
#if 0
    if (networkManager->networkAccessible() == false && Options::astrometryAPIURL().contains("127.0.0.1") == false)
    {
        align->appendLogText(i18n("Error: no connection to the Internet."));
        emit solverFailed();
        return false;
    }
#endif

    // Reset params
    center_ra = center_dec = downsample_factor = lowerScale = upperScale = Ekos::INVALID_VALUE;
    units.clear();
    useWCSCenter = false;

    filename = in_filename;

    for (int i = 0; i < args.count(); i++)
    {
        if (args[i] == "-L")
            lowerScale = args[i + 1].toDouble(&ok);
        else if (args[i] == "-H")
            upperScale = args[i + 1].toDouble(&ok);
        else if (args[i] == "-3")
            center_ra = args[i + 1].toDouble(&ok);
        else if (args[i] == "-4")
            center_dec = args[i + 1].toDouble(&ok);
        else if (args[i] == "-5")
            radius = args[i + 1].toDouble(&ok);
        else if (args[i] == "--downsample")
            downsample_factor = args[i + 1].toInt(&ok);
        else if (args[i] == "-u")
        {
            QString unitType = args[i + 1];
            if (unitType == "aw")
                units = "arcminwidth";
            else if (unitType == "dw")
                units = "degwidth";
            else
                units = "arcsecperpix";
        }
        else if (args[i] == "--crpix_center")
            useWCSCenter = true;
        else if (args[i] == "--parity")
        {
            QString arg = args[i + 1];
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
    workflowStage  = NO_STAGE;
    solver_retries = 0;

    disconnect(networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onResult(QNetworkReply*)));

    return true;
}

void OnlineAstrometryParser::authenticate()
{
    QNetworkRequest request;
    //request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QString astrometryURL = Options::astrometryAPIURL();

    // If pure IP, add http to it.
    if (!astrometryURL.startsWith("http"))
        astrometryURL = "http://" + astrometryURL;

    QUrl url(astrometryURL);
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
    bool rc         = fitsFile->open(QIODevice::ReadOnly);
    if (rc == false)
    {
        align->appendLogText(i18n("Failed to open the file %1: %2", filename, fitsFile->errorString()));
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

    // Do we have the units?
    if (units.isEmpty() == false)
    {
        uploadReq.insert("scale_type", "ul");
        uploadReq.insert("scale_units", units);
        uploadReq.insert("scale_lower", lowerScale);
        uploadReq.insert("scale_upper", upperScale);
    }

    // Do we send RA/DE?
    if (center_ra != Ekos::INVALID_VALUE)
    {
        uploadReq.insert("center_ra", center_ra);
        uploadReq.insert("center_dec", center_dec);
        uploadReq.insert("radius", radius);
    }

    if (useWCSCenter)
        uploadReq.insert("crpix_center", true);

    if (downsample_factor != Ekos::INVALID_VALUE)
        uploadReq.insert("downsample_factor", downsample_factor);

    // If we have parity and option is valid and this is NOT a blind solve (if ra or units exist then it is not a blind solve)
    // then we send parity
    if (Options::astrometryDetectParity() && parity != Ekos::INVALID_VALUE &&
            (center_ra != Ekos::INVALID_VALUE || units.isEmpty() == false))
        uploadReq.insert("parity", parity);

    QJsonObject json = QJsonObject::fromVariantMap(uploadReq);
    QJsonDocument json_doc(json);

    QHttpPart jsonPart;

    jsonPart.setHeader(QNetworkRequest::ContentTypeHeader, "application/text/plain");
    jsonPart.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"request-json\"");
    jsonPart.setBody(json_doc.toJson(QJsonDocument::Compact));

    QHttpPart filePart;

    filePart.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QString("form-data; name=\"file\"; filename=\"%1\"").arg(filename));
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

void OnlineAstrometryParser::onResult(QNetworkReply *reply)
{
    bool ok = false;
    QJsonParseError parseError;
    QString status;
    QList<QVariant> jsonArray;
    uint32_t elapsed = 0;

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

    QString json = (QString)reply->readAll();

    QJsonDocument json_doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        align->appendLogText(i18n("JSON error during parsing (%1).", parseError.errorString()));
        emit solverFailed();
        return;
    }

    QVariant json_result = json_doc.toVariant();
    QVariantMap result   = json_result.toMap();

    if (Options::alignmentLogging())
        align->appendLogText(json_doc.toJson(QJsonDocument::Compact));

    switch (workflowStage)
    {
        case AUTH_STAGE:
            status = result["status"].toString();
            if (status != "success")
            {
                align->appendLogText(
                    i18n("Astrometry.net authentication failed. Check the validity of the Astrometry.net API Key."));
                emit solverFailed();
                return;
            }

            sessionKey = result["session"].toString();

            if (Options::alignmentLogging())
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

            job_retries = 0;
            emit jobIDFinished();
            break;

        case JOB_STATUS_STAGE:
            status = result["status"].toString();
            elapsed = static_cast<uint32_t>(round(solverTimer.elapsed() / 1000.0));
            if (status == "success")
                emit jobFinished();
            else if (status == "solving" || status == "processing")
            {
                //if (status == "solving" && solver_retries++ < SOLVER_RETRY_ATTEMPTS)
                if (elapsed < Options::astrometryTimeout())
                {
                    QTimer::singleShot(SOLVER_RETRY_DURATION, this, SLOT(checkJobs()));
                    return;
                }
                else
                {
                    align->appendLogText(i18n("Solver timed out."));
                    solver_retries = 0;
                    emit solverFailed();
                    return;
                }
            }
            else if (status == "failure")
            {
                align->appendLogText(
                    i18np("Solver failed after %1 second.", "Solver failed after %1 seconds.", elapsed));
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
            ra = result["ra"].toDouble(&ok);
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

            elapsed = (int)round(solverTimer.elapsed() / 1000.0);
            align->appendLogText(i18np("Solver completed in %1 second.", "Solver completed in %1 seconds.", elapsed));
            // Parity == 0 is positive parity (East to the Left when North is up).
            emit solverFinished(orientation, ra, dec, pixscale, parity != 0);

            break;

        default:
            break;
    }
}
}
