/*  Ekos PHD2 Handler
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "phd2.h"

#include "Options.h"
#include "kspaths.h"
#include "kstars.h"

#include "ekos/ekosmanager.h"
#include "fitsviewer/fitsdata.h"

#include <cassert>
#include <fitsio.h>
#include <KMessageBox>
#include <QImage>

#include <QJsonDocument>
#include <QNetworkReply>

#include <ekos_guide_debug.h>

#define MAX_SET_CONNECTED_RETRIES   3

namespace Ekos
{
PHD2::PHD2()
{
    tcpSocket = new QTcpSocket(this);

    connect(tcpSocket, SIGNAL(readyRead()), this, SLOT(readPHD2()));
    connect(tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this,
            SLOT(displayError(QAbstractSocket::SocketError)));

    //This list of available PHD Events is on https://github.com/OpenPHDGuiding/phd2/wiki/EventMonitoring

    events["Version"]                 = Version;
    events["LockPositionSet"]         = LockPositionSet;
    events["CalibrationComplete"]     = CalibrationComplete;
    events["StarSelected"]            = StarSelected;
    events["StartGuiding"]            = StartGuiding;
    events["Paused"]                  = Paused;
    events["StartCalibration"]        = StartCalibration;
    events["AppState"]                = AppState;
    events["CalibrationFailed"]       = CalibrationFailed;
    events["CalibrationDataFlipped"]  = CalibrationDataFlipped;
    events["LoopingExposures"]        = LoopingExposures;
    events["LoopingExposuresStopped"] = LoopingExposuresStopped;
    events["SettleBegin"]             = SettleBegin;
    events["Settling"]                = Settling;
    events["SettleDone"]              = SettleDone;
    events["StarLost"]                = StarLost;
    events["GuidingStopped"]          = GuidingStopped;
    events["Resumed"]                 = Resumed;
    events["GuideStep"]               = GuideStep;
    events["GuidingDithered"]         = GuidingDithered;
    events["LockPositionLost"]        = LockPositionLost;
    events["Alert"]                   = Alert;
    events["GuideParamChange"]        = GuideParamChange;

    //This list of available PHD Methods is on https://github.com/OpenPHDGuiding/phd2/wiki/EventMonitoring
    //Only some of the methods are implemented.  The ones that say COMMAND_RECEIVED simply return a 0 saying the command was received.
        //capture_single_frame
    methodResults["clear_calibration"]      = CLEAR_CALIBRATION_COMMAND_RECEIVED;
    methodResults["dither"]                 = DITHER_COMMAND_RECEIVED;
        //find_star
        //flip_calibration
        //get_algo_param_names
        //get_algo_param
        //get_app_state
        //get_calibrated
        //get_calibration_data
    methodResults["get_connected"]          = IS_EQUIPMENT_CONNECTED;
        //get_cooler_status
        //get_current_equipment
    methodResults["get_dec_guide_mode"]     = DEC_GUIDE_MODE;
    methodResults["get_exposure"]           = EXPOSURE_TIME;
    methodResults["get_exposure_durations"] = EXPOSURE_DURATIONS;
        //get_lock_position
        //get_lock_shift_enabled
        //get_lock_shift_params
        //get_paused
    methodResults["get_pixel_scale"]        = PIXEL_SCALE;
        //get_profile
        //get_profiles
        //get_search_region
        //get_sensor_temperature
    methodResults["get_star_image"]         = STAR_IMAGE;
        //get_use_subframes
    methodResults["guide"]                  = GUIDE_COMMAND_RECEIVED;
        //guide_pulse
        //loop
        //save_image
        //set_algo_param
    methodResults["set_connected"]          = CONNECTION_RESULT;
    methodResults["set_dec_guide_mode"]     = SET_DEC_GUIDE_MODE_COMMAND_RECEIVED;
    methodResults["set_exposure"]           = SET_EXPOSURE_COMMAND_RECEIVED;
        //set_lock_position
        //set_lock_shift_enabled
        //set_lock_shift_params
    methodResults["set_paused"]             = SET_PAUSED_COMMAND_RECEIVED;
        //set_profile
        //shutdown
    methodResults["stop_capture"]           = STOP_CAPTURE_COMMAND_RECEIVED;

    QDir writableDir;
    writableDir.mkdir(KSPaths::writableLocation(QStandardPaths::TempLocation));

    abortTimer = new QTimer(this);
    connect(abortTimer, &QTimer::timeout, this, [=]
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Lost Star timeout expired";
        abort();
    });

    ditherTimer = new QTimer(this);
    connect(ditherTimer, &QTimer::timeout, this, [=]
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "ditherTimer expired, state" << state << "dithering" << isDitherActive << "settling" << isSettling;
        ditherTimer->stop();
        isDitherActive = false;
        isSettling = false;
        if (Options::ditherFailAbortsAutoGuide())
        {
            abort();
            emit newStatus(GUIDE_DITHERING_ERROR);
        }
        else
        {
            emit newLog(i18n("PHD2: There was no dithering response from PHD2, but continue guiding."));
            emit newStatus(Ekos::GUIDE_DITHERING_SUCCESS);
        }
    });
}

PHD2::~PHD2()
{
    delete abortTimer;
    delete ditherTimer;
}

bool PHD2::Connect()
{
    if (connection == DISCONNECTED)
    {
        emit newLog(i18n("Connecting to PHD2 Host: %1, on port %2. . .", Options::pHD2Host(), Options::pHD2Port()));
        tcpSocket->connectToHost(Options::pHD2Host(), Options::pHD2Port());
    }
    else    // Already connected, let's connect equipment
        connectEquipment(true);
    return true;
}

void PHD2::ResetConnectionState()
{
    connection = DISCONNECTED;

    // clear the outstanding and queued RPC requests
    pendingRpcResultType = NO_RESULT;
    rpcRequestQueue.clear();

    starImageRequested = false;
    isSettling = false;
    isDitherActive = false;

    ditherTimer->stop();
    abortTimer->stop();
}

bool PHD2::Disconnect()
{
    if (connection == EQUIPMENT_CONNECTED)
    {
        // !FIXME-ag this is not reliable because we are just sending
        // a blind request to disconnect equipment then close the
        // socket. PHD2 will refuse to disconnect the equipment if it
        // is still looping exposures
        //
        // to be reliable it should:
        //    call stop_capture
        //    wait for the stop to complete by polling get_app_state
        //    disconnect equipment
        //    disconnect from phd2

        connectEquipment(false);
    }

    ResetConnectionState();

    tcpSocket->disconnectFromHost();

    emit newStatus(GUIDE_DISCONNECTED);

    return true;
}

void PHD2::displayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError)
    {
        case QAbstractSocket::RemoteHostClosedError:
            break;
        case QAbstractSocket::HostNotFoundError:
            emit newLog(i18n("The host was not found. Please check the host name and port settings in Guide options."));
            emit newStatus(GUIDE_DISCONNECTED);
            break;
        case QAbstractSocket::ConnectionRefusedError:
            emit newLog(i18n("The connection was refused by the peer. Make sure the PHD2 is running, and check that "
                             "the host name and port settings are correct."));
            emit newStatus(GUIDE_DISCONNECTED);
            break;
        default:
            emit newLog(i18n("The following error occurred: %1.", tcpSocket->errorString()));
    }

    ResetConnectionState();
}

void PHD2::readPHD2()
{
    while (!tcpSocket->atEnd() && tcpSocket->canReadLine())
    {
        QByteArray line = tcpSocket->readLine();
        if (line.isEmpty())
            continue;

        QJsonParseError qjsonError;

        QJsonDocument jdoc = QJsonDocument::fromJson(line, &qjsonError);

        if (qjsonError.error != QJsonParseError::NoError)
        {
            emit newLog(i18n("PHD2: invalid response received: %1", QString(line)));
            emit newLog(i18n("PHD2: JSON error: %1", qjsonError.errorString()));
            continue;
        }

        QJsonObject jsonObj = jdoc.object();

        if (jsonObj.contains("Event"))
            processPHD2Event(jsonObj, line);
        else if (jsonObj.contains("error"))
            processPHD2Error(jsonObj, line);
        else if (jsonObj.contains("result"))
            processPHD2Result(jsonObj, line);
    }
}

void PHD2::processPHD2Event(const QJsonObject &jsonEvent, const QByteArray &line)
{
    if (Options::verboseLogging())
        qCDebug(KSTARS_EKOS_GUIDE) << "PHD2: event:" << line;

    QString eventName = jsonEvent["Event"].toString();

    if (!events.contains(eventName))
    {
        emit newLog(i18n("Unknown PHD2 event: %1", eventName));
        return;
    }

    event = events.value(eventName);

    switch (event)
    {
        case Version:
            emit newLog(i18n("PHD2: Version %1", jsonEvent["PHDVersion"].toString()));
            connection = CONNECTED;
            break;

        case CalibrationComplete:
            setEquipmentConnected();
            emit newLog(i18n("PHD2: Calibration Complete."));
            emit newStatus(Ekos::GUIDE_CALIBRATION_SUCESS);
            break;

        case StartGuiding:
            setEquipmentConnected();
            updateGuideParameters();
            emit newLog(i18n("PHD2: Guiding Started."));
            emit newStatus(Ekos::GUIDE_GUIDING);
            break;

        case Paused:
            state = PAUSED;
            emit newLog(i18n("PHD2: Guiding Paused."));
            emit newStatus(Ekos::GUIDE_SUSPENDED);
            break;

        case StartCalibration:
            state = CALIBRATING;
            emit newLog(i18n("PHD2: Calibration Started."));
            emit newStatus(Ekos::GUIDE_CALIBRATING);
            break;

        case AppState:
            // AppState is the last of the initial messages received when we first connect to PHD2
            processPHD2State(jsonEvent["State"].toString());
            // if the equipment is not already connected, then try to connect it.
            if (connection != EQUIPMENT_CONNECTED)
                connectEquipment(true);
            break;

        case CalibrationFailed:
            emit newLog(i18n("PHD2: Calibration Failed (%1).", jsonEvent["Reason"].toString()));
            emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);
            break;

        case CalibrationDataFlipped:
            emit newLog(i18n("Calibration Data Flipped."));
            break;

        case LoopingExposures:
            state = LOOPING;
            //emit newLog(i18n("PHD2: Looping Exposures."));
            break;

        case LoopingExposuresStopped:
            state = STOPPED;
            emit newLog(i18n("PHD2: Looping Exposures Stopped."));
            break;

        case Settling:        
        case SettleBegin:
            //This can happen for guiding or for dithering.  A Settle done event will arrive when it finishes.
            break;

        case SettleDone:
        {
            bool error = false;

            if (jsonEvent["Status"].toInt() != 0)
            {
                error = true;
                emit newLog(i18n("PHD2: Settling failed (%1).", jsonEvent["Error"].toString()));
            }

            bool wasDithering = isDitherActive;

            isDitherActive = false;
            isSettling = false;

            if (wasDithering)
            {
                ditherTimer->stop();
                if (error && Options::ditherFailAbortsAutoGuide())
                {
                    abort();
                    emit newStatus(GUIDE_DITHERING_ERROR);
                }
                else
                {
                    if (error)
                        emit newLog(i18n("PHD2: There was a dithering error, but continue guiding."));

                    emit newStatus(Ekos::GUIDE_DITHERING_SUCCESS);
                }
            }
            else
            {
                // settle completed after "guide" command

                if (!error)
                {
                    emit newLog(i18n("PHD2: Settling complete, Guiding Started."));
                }
            }
        }
        break;

        case StarSelected:
            emit newLog(i18n("PHD2: Star Selected."));
            break;

        case StarLost:
            emit newLog(i18n("PHD2: Star Lost. Trying to reacquire."));
            if (state != LOSTLOCK)
            {
                state = LOSTLOCK;
                abortTimer->start(Options::guideLostStarTimeout() * 1000);
            }
            break;

        case GuidingStopped:
            state = STOPPED;
            emit newLog(i18n("PHD2: Guiding Stopped."));
            emit newStatus(Ekos::GUIDE_ABORTED);
            break;

        case Resumed:
            emit newLog(i18n("PHD2: Guiding Resumed."));
            emit newStatus(Ekos::GUIDE_GUIDING);
            break;

        case GuideStep:
        {
            if (state == LOSTLOCK)
            {
                emit newLog(i18n("PHD2: Star found, guiding resumed."));
                abortTimer->stop();  
                state = GUIDING;
            }
            // JM 2018-08-05: GuideStep does not necessary mean we're guiding
            // It could be that we're settling. This needs to be double-checked.
//            else if (state != GUIDING)
//            {
//                emit newLog(i18n("PHD2: Guiding started up again."));
//                emit newStatus(Ekos::GUIDE_GUIDING);
//                state = GUIDING;
//            }
            if (isDitherActive)
                return;
            double diff_ra_pixels, diff_de_pixels, diff_ra_arcsecs, diff_de_arcsecs, pulse_ra, pulse_dec;
            QString RADirection, DECDirection;
            diff_ra_pixels = jsonEvent["RADistanceRaw"].toDouble();
            diff_de_pixels = jsonEvent["DECDistanceRaw"].toDouble();
            pulse_ra = jsonEvent["RADuration"].toDouble();
            pulse_dec = jsonEvent["DECDuration"].toDouble();
            RADirection = jsonEvent["RADirection"].toString();
            DECDirection = jsonEvent["DECDirection"].toString();

            if (RADirection == "East")
                pulse_ra = -pulse_ra;  //West Direction is Positive, East is Negative
            if (DECDirection == "South")
                pulse_dec = -pulse_dec; //South Direction is Negative, North is Positive

            //If the pixelScale is properly set from PHD2, the second block of code is not needed, but if not, we will attempt to calculate the ra and dec error without it.
            if (pixelScale != 0)
            {
                diff_ra_arcsecs = diff_ra_pixels * pixelScale;
                diff_de_arcsecs = diff_de_pixels * pixelScale;
            }
            else
            {
                diff_ra_arcsecs = 206.26480624709 * diff_ra_pixels * ccdPixelSizeX / mountFocalLength;
                diff_de_arcsecs = 206.26480624709 * diff_de_pixels * ccdPixelSizeY / mountFocalLength;
            }

            if (std::isfinite(diff_ra_arcsecs) && std::isfinite(diff_de_arcsecs))
            {
                errorLog.append(QPointF(diff_ra_arcsecs, diff_de_arcsecs));
                if(errorLog.size()>50)
                    errorLog.remove(0);

                emit newAxisDelta(diff_ra_arcsecs, diff_de_arcsecs);
                emit newAxisPulse(pulse_ra, pulse_dec);

                double total_sqr_RA_error = 0.0;
                double total_sqr_DE_error = 0.0;

                for (auto &point : errorLog)
                {
                    total_sqr_RA_error+=point.x()*point.x();
                    total_sqr_DE_error+=point.y()*point.y();
                }

                emit newAxisSigma(sqrt(total_sqr_RA_error/errorLog.size()), sqrt(total_sqr_DE_error/errorLog.size()));

            }

            requestStarImage(32); //This requests a star image for the guide view.  32 x 32 pixels
        }
        break;

        case GuidingDithered:
            break;

        case LockPositionSet:
            emit newLog(i18n("PHD2: Lock Position Set."));
            break;

        case LockPositionLost:
            emit newLog(i18n("PHD2: Lock Position Lost."));
            if (state == CALIBRATING)
                emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);
            break;

        case Alert:
            emit newLog(i18n("PHD2 %1: %2", jsonEvent["Type"].toString(), jsonEvent["Msg"].toString()));
            break;

        case GuideParamChange:
            //Don't do anything for now, might change this later.
            //Some Possible Parameter Names:
            //Backlash comp enabled, Backlash comp amount,
            //For Each Axis: MinMove, Max Duration,
            //PPEC aggressiveness, PPEC prediction weight,
            //Resist switch minimum motion, Resist switch aggression,
            //Low-pass minimum move, Low-pass slope weight,
            //Low-pass2 minimum move, Low-pass2 aggressiveness,
            //Hysteresis hysteresis, Hysteresis aggression
        break;
    }
}

void PHD2::processPHD2State(const QString &phd2State)
{
    if (phd2State == "Stopped")
        state = STOPPED;
    else if (phd2State == "Selected")
        state = SELECTED;
    else if (phd2State == "Calibrating")
        state = CALIBRATING;
    else if (phd2State == "Guiding")
        state = GUIDING;
    else if (phd2State == "LostLock")
        state = LOSTLOCK;
    else if (phd2State == "Paused")
        state = PAUSED;
    else if (phd2State == "Looping")
        state = LOOPING;
}

void PHD2::processPHD2Result(const QJsonObject &jsonObj, const QByteArray &line)
{
    PHD2ResultType resultType = takeRequestFromList(jsonObj);

    if (resultType == STAR_IMAGE)
        qCDebug(KSTARS_EKOS_GUIDE) << "PHD2: received star image response, id" << jsonObj["id"].toInt();   // don't spam the log with image data
    else
        qCDebug(KSTARS_EKOS_GUIDE) << "PHD2: response:" << line;

    switch (resultType)
    {
        case NO_RESULT:
            //Ekos didn't ask for this result?
            break;

                                                    //capture_single_frame
        case CLEAR_CALIBRATION_COMMAND_RECEIVED:    //clear_calibration
            break;

        case DITHER_COMMAND_RECEIVED:               //dither
            emit newStatus(Ekos::GUIDE_DITHERING);
            break;

                                                    //find_star
                                                    //flip_calibration
                                                    //get_algo_param_names
                                                    //get_algo_param
                                                    //get_app_state
                                                    //get_calibrated
                                                    //get_calibration_data

        case IS_EQUIPMENT_CONNECTED:                //get_connected
        {
            bool isConnected = jsonObj["result"].toBool();
            if(isConnected)
            {
                setEquipmentConnected();
            }
            else
            {
                connection = EQUIPMENT_DISCONNECTED;
                emit newStatus(Ekos::GUIDE_DISCONNECTED);
            }
        }
            break;

                                                    //get_cooler_status
                                                    //get_current_equipment

        case DEC_GUIDE_MODE:                        //get_dec_guide_mode
        {
            QString mode = jsonObj["result"].toString();
            KStars::Instance()->ekosManager()->guideModule()->updateDirectionsFromPHD2(mode);
            emit newLog(i18n("PHD2: DEC Guide Mode is Set to: %1", mode));
        }
            break;


        case EXPOSURE_TIME:                         //get_exposure
        {
            int exposurems=jsonObj["result"].toInt();
            double exposureTime=exposurems/1000.0;
            KStars::Instance()->ekosManager()->guideModule()->setExposure(exposureTime);
            emit newLog(i18n("PHD2: Exposure Time set to: ") + QString::number(exposureTime, 'f', 2));
            break;
        }


        case EXPOSURE_DURATIONS:                    //get_exposure_durations
        {
            QVariantList exposureListArray=jsonObj["result"].toArray().toVariantList();
            logValidExposureTimes = i18n("PHD2: Valid Exposure Times: Auto, ");
            QList<double> values;
            for(int i=1; i < exposureListArray.size(); i ++) //For some reason PHD2 has a negative exposure time of 1 at the start of the array?
                values << exposureListArray.at(i).toDouble()/1000.0;//PHD2 reports in ms.
            logValidExposureTimes += KStars::Instance()->ekosManager()->guideModule()->setRecommendedExposureValues(values);
            emit newLog(logValidExposureTimes);
            break;
        }
                                                    //get_lock_position
                                                    //get_lock_shift_enabled
                                                    //get_lock_shift_params
                                                    //get_paused

        case PIXEL_SCALE:                           //get_pixel_scale
            pixelScale=jsonObj["result"].toDouble();
            if (pixelScale == 0)
                emit newLog(i18n("PHD2: Please set CCD and telescope parameters in PHD2, Pixel Scale is invalid."));
            else
                emit newLog(i18n("PHD2: Pixel Scale is %1 arcsec per pixel", QString::number(pixelScale, 'f', 2)));
            break;

                                                    //get_profile
                                                    //get_profiles
                                                    //get_search_region
                                                    //get_sensor_temperature

        case STAR_IMAGE:                            //get_star_image
        {
            starImageRequested = false;
            QJsonObject jsonResult = jsonObj["result"].toObject();
            processStarImage(jsonResult);
            break;
        }

                                                    //get_use_subframes

        case GUIDE_COMMAND_RECEIVED:                //guide
            break;
                                                    //guide_pulse
                                                    //loop
                                                    //save_image
                                                    //set_algo_param

        case CONNECTION_RESULT:                     //set_connected
            checkIfEquipmentConnected();
            break;

        case SET_DEC_GUIDE_MODE_COMMAND_RECEIVED:   //set_dec_guide_mode
            checkDEGuideMode();
            break;

        case SET_EXPOSURE_COMMAND_RECEIVED:         //set_exposure
            requestExposureTime(); //This will check what it was set to and print a message as to what it is.
            break;

                                                    //set_lock_position
                                                    //set_lock_shift_enabled
                                                    //set_lock_shift_params

        case SET_PAUSED_COMMAND_RECEIVED:           //set_paused
            break;
                                                    //set_profile
                                                    //shutdown

        case STOP_CAPTURE_COMMAND_RECEIVED:         //stop_capture
            break;
    }

    // send the next pending call
    sendNextRpcCall();
}

void PHD2::processPHD2Error(const QJsonObject &jsonError, const QByteArray &line)
{
    qCDebug(KSTARS_EKOS_GUIDE) << "PHD2: error:" << line;

    QJsonObject jsonErrorObject = jsonError["error"].toObject();

    emit newLog(i18n("PHD2 Error: %1", jsonErrorObject["message"].toString()));

    PHD2ResultType resultType = takeRequestFromList(jsonError);

    // This means the user mistakenly entered an invalid exposure time.
    if (resultType == SET_EXPOSURE_COMMAND_RECEIVED)
    {
        emit newLog(logValidExposureTimes);  //This will let the user know the valid exposure durations
        QTimer::singleShot(300, [=]{requestExposureTime();}); //This will reset the Exposure time in Ekos to PHD2's current exposure time after a third of a second.
    }
    else if (resultType == CONNECTION_RESULT)
    {
        connection = EQUIPMENT_DISCONNECTED;
        emit newStatus(Ekos::GUIDE_DISCONNECTED);
    }
    else if (resultType == DITHER_COMMAND_RECEIVED)
    {
        ditherTimer->stop();
        isSettling = false;
        isDitherActive = false;
        emit newStatus(GUIDE_DITHERING_ERROR);

        if (Options::ditherFailAbortsAutoGuide())
        {
            abort();
            emit newStatus(GUIDE_ABORTED);
        }
        else
        {
            // !FIXME-ag why is this trying to resume (un-pause)?
            resume();
        }
    }
    else if (resultType == GUIDE_COMMAND_RECEIVED)
    {
        isSettling = false;
    }

    // send the next pending call
    sendNextRpcCall();
}

//These methods process the Star Images the PHD2 provides

void PHD2::setGuideView(FITSView *guideView)
{
    guideFrame = guideView;
}

void PHD2::processStarImage(const QJsonObject &jsonStarFrame)
{
    //The width and height of the received PHD2 Star Image
   int width =  jsonStarFrame["width"].toInt();
   int height = jsonStarFrame["height"].toInt();

   QTemporaryFile tempfile(KSPaths::writableLocation(QStandardPaths::TempLocation) + QLatin1Literal("phd2_XXXXXX"));
   if (!tempfile.open())
   {
       qCWarning(KSTARS_EKOS_GUIDE) << "could not create temp file for PHD2 star image";
       return;
   }
   QString filename = tempfile.fileName();

   //This section sets up the FITS File
   fitsfile *fptr = nullptr;
   int status=0;
   long  fpixel = 1, naxis = 2, nelements, exposure;
   long naxes[2] = { width, height };
   char error_status[512] = {0};

   if (fits_create_file(&fptr, QString('!' + filename).toLatin1().data(), &status))
   {
       qCWarning(KSTARS_EKOS_GUIDE) << "fits_create_file failed:" << error_status;
       return;
   }

   if (fits_create_img(fptr, USHORT_IMG, naxis, naxes, &status))
   {
       qCWarning(KSTARS_EKOS_GUIDE) << "fits_create_img failed:" << error_status;
       status = 0;
       fits_close_file(fptr, &status);
       return;
   }

    //Note, this is made up.  If you want the actual exposure time, you have to request it from PHD2
   exposure = 1;
   fits_update_key(fptr, TLONG, "EXPOSURE", &exposure,"Total Exposure Time", &status);

   //This section takes the Pixels from the JSON Document
   //Then it converts from base64 to a QByteArray
   //Then it creates a datastream from the QByteArray to the pixel array for the FITS File
   QByteArray converted = QByteArray::fromBase64(jsonStarFrame["pixels"].toString().toLocal8Bit());

   //This finishes up and closes the FITS file
   nelements = naxes[0] * naxes[1];
   if (fits_write_img(fptr, TUSHORT, fpixel, nelements, converted.data(), &status))
   {
       fits_get_errstatus(status, error_status);
       qCWarning(KSTARS_EKOS_GUIDE) << "fits_write_img failed:" << error_status;
       status = 0;
       fits_close_file(fptr, &status);
       return;
   }

   if (fits_flush_file(fptr, &status))
   {
       fits_get_errstatus(status, error_status);
       qCWarning(KSTARS_EKOS_GUIDE) << "fits_flush_file failed:" << error_status;
       status = 0;
       fits_close_file(fptr, &status);
       return;
   }

   if (fits_close_file(fptr, &status))
   {
       fits_get_errstatus(status, error_status);
       qCWarning(KSTARS_EKOS_GUIDE) << "fits_close_file failed:" << error_status;
       return;
   }

   //This loads the FITS file in the Guide FITSView
   //Then it updates the Summary Screen
   bool imageLoaded = guideFrame->loadFITS(filename, true);
   if (imageLoaded)
   {
       guideFrame->getImageData()->setAutoRemoveTemporaryFITS(false); // we'll take care of deleting the temp file
       guideFrame->updateFrame();
       guideFrame->setTrackingBox(QRect(0,0,width,height));
       emit newStarPixmap(guideFrame->getTrackingBoxPixmap());
   }
}

void PHD2::setEquipmentConnected()
{
    if (connection != EQUIPMENT_CONNECTED)
    {
        setConnectedRetries = 0;
        connection = EQUIPMENT_CONNECTED;
        emit newStatus(Ekos::GUIDE_CONNECTED);
        updateGuideParameters();
        requestExposureDurations();
    }
}

void PHD2::updateGuideParameters()
{
    if (pixelScale == 0)
        requestPixelScale();
    requestExposureTime();
    checkDEGuideMode();
}

//This section handles the methods/requests sent to PHD2, some are not implemented.

//capture_single_frame

//clear_calibration
bool PHD2::clearCalibration()
{
    if (connection != EQUIPMENT_CONNECTED)
    {
        emit newLog(i18n("PHD2 Error: Equipment not connected."));
        return false;
    }

    QJsonArray args;
    //This instructs PHD2 which calibration to clear.
    args << "mount";
    sendPHD2Request("clear_calibration", args);

    return true;
}

//dither
bool PHD2::dither(double pixels)
{
    if (connection != EQUIPMENT_CONNECTED)
    {
        emit newLog(i18n("PHD2 Error: Equipment not connected."));
        return false;
    }

    if (isSettling)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "PHD2: ignoring dither requested while already settling";

        if (!isDitherActive)
        {
            // act like we just dithered so we get the appropriate
            // effects after the settling completes
            emit newStatus(Ekos::GUIDE_DITHERING);
            isDitherActive = true;
        }
        return true;
    }

    QJsonArray args;
    QJsonObject settle;

    int ditherTimeout = static_cast<int>(Options::ditherTimeout());

    settle.insert("pixels", static_cast<double>(Options::ditherThreshold()));
    settle.insert("time", static_cast<int>(Options::ditherSettle()));
    settle.insert("timeout", ditherTimeout);

    // Pixels
    args << pixels;
    // RA Only?
    args << false;
    // Settle
    args << settle;

    isSettling = true;
    isDitherActive = true;

    // PHD2 will send a SettleDone event shortly after the settling
    // timeout in PHD2. We don't really need a timer here, but we'll
    // set one anyway (belt and suspenders). Make sure to give an
    // extra time allowance since PHD2 won't report its timeout until
    // the completion of the next guide exposure after the timeout
    // period expires.
    enum { TIMEOUT_EXTRA_SECONDS = 60 };  // at least as long as any reasonable guide exposure
    int millis = (ditherTimeout + TIMEOUT_EXTRA_SECONDS) * 1000;
    ditherTimer->start(millis);

    sendPHD2Request("dither", args);

    emit newStatus(Ekos::GUIDE_DITHERING);

    return true;
}

//find_star
//flip_calibration
//get_algo_param_names
//get_algo_param
//get_app_state
//get_calibrated
//get_calibration_data

//get_connected
void PHD2::checkIfEquipmentConnected()
{
    sendPHD2Request("get_connected");
}

//get_cooler_status
//get_current_equipment

//get_dec_guide_mode
void PHD2::checkDEGuideMode()
{
    sendPHD2Request("get_dec_guide_mode");
}

//get_exposure
void PHD2::requestExposureTime()
{
    sendPHD2Request("get_exposure");
}

//get_exposure_durations
void PHD2::requestExposureDurations()
{
    sendPHD2Request("get_exposure_durations");
}

//get_lock_position
//get_lock_shift_enabled
//get_lock_shift_params
//get_paused

//get_pixel_scale
void PHD2::requestPixelScale()
{
    sendPHD2Request("get_pixel_scale");
}

//get_profile
//get_profiles
//get_search_region
//get_sensor_temperature

//get_star_image
void PHD2::requestStarImage(int size)
{
    if (!Options::guideRemoteImagesEnabled())
        return;

    if (starImageRequested)
    {
        if (Options::verboseLogging())
            qCDebug(KSTARS_EKOS_GUIDE) << "PHD2: skip extra star image request";
        return;
    }

    QJsonArray args2;
    args2 << size; // This is both the width and height.
    sendPHD2Request("get_star_image", args2);

    starImageRequested = true;
}

//get_use_subframes

//guide
bool PHD2::guide()
{
    if (state == GUIDING)
    {
        emit newLog(i18n("PHD2: Guiding is already running."));
        emit newStatus(Ekos::GUIDE_GUIDING);
        return true;
    }

    if (connection != EQUIPMENT_CONNECTED)
    {
        emit newLog(i18n("PHD2 Error: Equipment not connected."));
        return false;
    }

    QJsonArray args;
    QJsonObject settle;

    settle.insert("pixels", static_cast<double>(Options::ditherThreshold()));
    settle.insert("time", static_cast<int>(Options::ditherSettle()));
    settle.insert("timeout", static_cast<int>(Options::ditherTimeout()));

    // Settle param
    args << settle;
    // Recalibrate param
    args << false;

    errorLog.clear();

    isSettling = true;

    sendPHD2Request("guide", args);

    return true;
}

//guide_pulse
//loop
//save_image
//set_algo_param

//set_connected
void PHD2::connectEquipment(bool enable)
{
    if (setConnectedRetries++ > MAX_SET_CONNECTED_RETRIES)
    {
        setConnectedRetries = 0;
        connection = EQUIPMENT_DISCONNECTED;
        emit newStatus(Ekos::GUIDE_DISCONNECTED);
        return;
    }

    if ((connection == EQUIPMENT_CONNECTED && enable == true) ||
        (connection == EQUIPMENT_DISCONNECTED && enable == false))
        return;

    pixelScale = 0 ;

    QJsonArray args;

    // connected = enable
    args << enable;

    if (enable)
        emit newLog(i18n("PHD2: Connecting Equipment. . ."));
    else
        emit newLog(i18n("PHD2: Disconnecting Equipment. . ."));

    sendPHD2Request("set_connected", args);
}

//set_dec_guide_mode
void PHD2::requestSetDEGuideMode(bool deEnabled, bool nEnabled, bool sEnabled) //Possible Settings Off, Auto, North, and South
{
    QJsonArray args;

    if(deEnabled)
    {
        if(nEnabled && sEnabled)
            args << "Auto";
        else if(nEnabled)
            args << "North";
        else if(sEnabled)
            args << "South";
        else
            args << "Off";
    }
    else
    {
        args << "Off";
    }

    sendPHD2Request("set_dec_guide_mode", args);
}

//set_exposure
void PHD2::requestSetExposureTime(int time) //Note: time is in milliseconds
{
    QJsonArray args;
    args << time;
    sendPHD2Request("set_exposure", args);
}

//set_lock_position
//set_lock_shift_enabled
//set_lock_shift_params

//set_paused
bool PHD2::suspend()
{
    if (connection != EQUIPMENT_CONNECTED)
    {
        emit newLog(i18n("PHD2 Error: Equipment not connected."));
        return false;
    }

    QJsonArray args;

    // Paused param
    args << true;
    // FULL param
    args << "full";

    sendPHD2Request("set_paused", args);

    return true;
}

//set_paused (also)
bool PHD2::resume()
{
    if (connection != EQUIPMENT_CONNECTED)
    {
        emit newLog(i18n("PHD2 Error: Equipment not connected."));
        return false;
    }

    QJsonArray args;

    // Paused param
    args << false;

    sendPHD2Request("set_paused", args);

    return true;
}

//set_profile
//shutdown

//stop_capture
bool PHD2::abort()
{
    if (connection != EQUIPMENT_CONNECTED)
    {
        emit newLog(i18n("PHD2 Error: Equipment not connected."));
        return false;
    }

    abortTimer->stop();

    sendPHD2Request("stop_capture");
    return true;
}

//This method is not handled by PHD2
bool PHD2::calibrate()
{
    // We don't explicitly do calibration since it is done in the guide step by PHD2 anyway
    //emit newStatus(Ekos::GUIDE_CALIBRATION_SUCESS);
    return true;
}

//This is how information requests and commands for PHD2 are handled

void PHD2::sendRpcCall(QJsonObject &call, PHD2ResultType resultType)
{
    assert(resultType != NO_RESULT); // should be a real request
    assert(pendingRpcResultType == NO_RESULT);  // only one pending RPC call at a time

    int rpcId = nextRpcId++;
    call.insert("id", rpcId);

    pendingRpcId = rpcId;
    pendingRpcResultType = resultType;

    QByteArray request = QJsonDocument(call).toJson(QJsonDocument::Compact);

    qCDebug(KSTARS_EKOS_GUIDE) << "PHD2: request:" << request;

    request.append("\r\n");
    qint64 n = tcpSocket->write(request);

    if ((int) n != request.size())
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "PHD2: unexpected short write:" << n << "bytes of" << request.size();
    }
}

void PHD2::sendNextRpcCall()
{
    if (pendingRpcResultType != NO_RESULT)
        return; // a request is currently outstanding

    if (rpcRequestQueue.empty())
        return; // no queued requests

    RpcCall& call = rpcRequestQueue.front();
    sendRpcCall(call.call, call.resultType);
    rpcRequestQueue.pop_front();
}

void PHD2::sendPHD2Request(const QString &method, const QJsonArray &args)
{
    assert(methodResults.contains(method));

    PHD2ResultType resultType = methodResults[method];

    QJsonObject jsonRPC;

    jsonRPC.insert("jsonrpc", "2.0");
    jsonRPC.insert("method", method);

    if (!args.empty())
        jsonRPC.insert("params", args);

    if (pendingRpcResultType == NO_RESULT)
    {
        // no outstanding rpc call, send it right off
        sendRpcCall(jsonRPC, resultType);
    }
    else
    {
        // there is already an oustanding call, enqueue this call
        // until the prior call completes

        if (Options::verboseLogging())
            qCDebug(KSTARS_EKOS_GUIDE) << "PHD2: defer call" << method;

        rpcRequestQueue.push_back(RpcCall(jsonRPC, resultType));
    }
}

PHD2::PHD2ResultType PHD2::takeRequestFromList(const QJsonObject &response)
{
    if (Q_UNLIKELY(!response.contains("id")))
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "PHD2: ignoring unexpected response with no id";
        return NO_RESULT;
    }

    int id = response["id"].toInt();

    if (Q_UNLIKELY(id != pendingRpcId))
    {
        // RPC id mismatch -- this should never happen, something is
        // seriously wrong
        qCDebug(KSTARS_EKOS_GUIDE) << "PHD2: ignoring unexpected response with id" << id;
        return NO_RESULT;
    }

    PHD2ResultType val = pendingRpcResultType;
    pendingRpcResultType = NO_RESULT;
    return val;
}

}
