/*  Ekos Internal Guider Class
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>.

    Based on lin_guider

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <KMessageBox>
#include <KNotification>

#include "auxiliary/kspaths.h"

#include "internalguider.h"
#include "gmath.h"

#include "ekos/auxiliary/QProgressIndicator.h"

#include "Options.h"

#define MAX_DITHER_RETIRES  20

namespace Ekos
{

InternalGuider::InternalGuider()
{
    // Create math object
    pmath = new cgmath();

    //connect(pmath, SIGNAL(newAxisDelta(double,double)), this, SIGNAL(newAxisDelta(double,double)));
    //connect(pmath, SIGNAL(newAxisDelta(double,double)), this, SLOT(updateGuideDriver(double,double)));
    //connect(pmath, SIGNAL(newStarPosition(QVector3D,bool)), this, SLOT(setStarPosition(QVector3D,bool)));
    connect(pmath, SIGNAL(newStarPosition(QVector3D,bool)), this, SIGNAL(newStarPosition(QVector3D,bool)));

    // Calibration
    calibrationStage = CAL_IDLE;

    //is_started = false;
    axis = GUIDE_RA;
    auto_drift_time = 5;

    start_x1 = start_y1 = 0;
    end_x1 = end_y1 = 0;
    start_x2 = start_y2 = 0;
    end_x2 = end_y2 = 0;
}

InternalGuider::~InternalGuider()
{
    delete (pmath);
}

bool InternalGuider::guide()
{
    if (state == GUIDE_SUSPENDED)
        return true;

    if (state >= GUIDE_GUIDING)
    {
        return processGuiding();
    }

    guideFrame->disconnect(this);

    QString logFileName = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/guide_log.txt";
    logFile.setFileName(logFileName);
    logFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&logFile);

    out << "Guiding rate,x15 arcsec/sec: " << Options::guidingRate() << endl;
    out << "Focal,mm: " << mountFocalLength << endl;
    out << "Aperture,mm: " << mountAperture << endl;
    out << "F/D: " << mountFocalLength/mountAperture << endl;
    out << "Frame #, Time Elapsed (ms), RA Error (arcsec), RA Correction (ms), RA Correction Direction, DEC Error (arcsec), DEC Correction (ms), DEC Correction Direction"  << endl;

    pmath->setLogFile(&logFile);

    pmath->start();

    m_lostStarTries=0;

    // TODO re-enable rapid check later on
#if 0
    m_isStarted = true;
    m_useRapidGuide = ui.rapidGuideCheck->isChecked();
    if (m_useRapidGuide)
        guideModule->startRapidGuide();

    emit newStatus(Ekos::GUIDE_GUIDING);

    guideModule->setSuspended(false);

    first_frame = true;

    if (ui.subFrameCheck->isEnabled() && ui.subFrameCheck->isChecked() && m_isSubFramed == false)
        first_subframe = true;

    capture();

#endif

    isFirstFrame = true;

    state = GUIDE_GUIDING;
    emit newStatus(state);

    emit frameCaptureRequested();

    return true;
}

bool InternalGuider::abort()
{
    calibrationStage = CAL_IDLE;

    if (state == GUIDE_CALIBRATING || state == GUIDE_GUIDING || state == GUIDE_DITHERING)
        emit newStatus(GUIDE_ABORTED);
    else
        emit newStatus(GUIDE_IDLE);

    state = GUIDE_IDLE;

    return true;
}

bool InternalGuider::suspend()
{
    state = GUIDE_SUSPENDED;
    emit newStatus(state);

    pmath->suspend(true);

    return true;
}

bool InternalGuider::resume()
{
    state = GUIDE_GUIDING;
    emit newStatus(state);

    pmath->suspend(false);

    emit frameCaptureRequested();

    return true;
}

bool InternalGuider::dither(double pixels)
{
    static Vector target_pos;
    static unsigned int retries=0;

    //if (ui.ditherCheck->isChecked() == false)
        //return false;

    double cur_x, cur_y, ret_angle;
    pmath->getReticleParameters(&cur_x, &cur_y, &ret_angle);
    pmath->getStarScreenPosition( &cur_x, &cur_y );
    Matrix ROT_Z = pmath->getROTZ();

    //qDebug() << "Star Pos X " << cur_x << " Y " << cur_y;

    if (state != GUIDE_DITHERING)
    {
        retries =0;

        // JM 2016-05-8: CCD would abort if required.
        //targetChip->abortExposure();

        int polarity = (rand() %2 == 0) ? 1 : -1;
        double angle = ((double) rand() / RAND_MAX) * M_PI/2.0;
        double diff_x = pixels * cos(angle);
        double diff_y = pixels * sin(angle);

        if (pmath->declinationSwapEnabled())
            diff_y *= -1;

        if (polarity > 0)
            target_pos = Vector( cur_x, cur_y, 0 ) + Vector( diff_x, diff_y, 0 );
        else
            target_pos = Vector( cur_x, cur_y, 0 ) - Vector( diff_x, diff_y, 0 );

        if (Options::guideLogging())
            qDebug() << "Guide: Dithering process started.. Reticle Target Pos X " << target_pos.x << " Y " << target_pos.y;

        pmath->setReticleParameters(target_pos.x, target_pos.y, ret_angle);

        state = GUIDE_DITHERING;
        emit newStatus(state);

        processGuiding();

        return true;
    }

    Vector star_pos = Vector( cur_x, cur_y, 0 ) - Vector( target_pos.x, target_pos.y, 0 );
    star_pos.y = -star_pos.y;
    star_pos = star_pos * ROT_Z;

    if (Options::guideLogging())
        qDebug() << "Guide: Dithering in progress. Diff star X:" << star_pos.x << "Y:" << star_pos.y;

    if (fabs(star_pos.x) < 1 && fabs(star_pos.y) < 1)
    {
        pmath->setReticleParameters(cur_x, cur_y, ret_angle);        
        if (Options::guideLogging())
            qDebug() << "Guide: Dither complete.";

        //emit ditherComplete();
        emit newStatus(Ekos::GUIDE_DITHERING_SUCCESS);

        // Back to guiding
        state = GUIDE_GUIDING;
    }
    else
    {
        if (++retries > MAX_DITHER_RETIRES)
        {
            emit newStatus(Ekos::GUIDE_DITHERING_ERROR);
            abort();
            return false;
        }

        processGuiding();
    }

    return true;
}

bool InternalGuider::calibrate()
{
    bool ccdInfo=true, scopeInfo=true;
    QString errMsg;

    if (subW == 0 || subH == 0)
    {
        errMsg = "CCD";
        ccdInfo = false;
    }

    if (mountAperture == 0 || mountFocalLength == 0)
    {
        scopeInfo = false;
        if (ccdInfo == false)
            errMsg += " & Telescope";
        else
            errMsg += "Telescope";
    }

    if (ccdInfo == false || scopeInfo == false)
    {
        KMessageBox::error(NULL, i18n("%1 info are missing. Please set the values in INDI Control Panel.", errMsg), i18n("Missing Information"));
        return false;
    }

    if (state != GUIDE_CALIBRATING)
    {
        calibrationStage = CAL_IDLE;
        state = GUIDE_CALIBRATING;
        emit newStatus(GUIDE_CALIBRATING);
    }

    if (calibrationStage > CAL_START)
    {
        processCalibration();
        return true;
    }

    guideFrame->disconnect(this);

    // Must reset dec swap before we run any calibration procedure!
    emit DESwapChanged(false);
    pmath->setDeclinationSwapEnabled(false);
    pmath->setLostStar(false);

    calibrationStage = CAL_START;


    // automatic
    // If two axies (RA/DEC) are required
    if( Options::twoAxisEnabled() )
        calibrateRADECRecticle(false);
    else
        // Just RA
        calibrateRADECRecticle(true);

    return true;
}

void InternalGuider::processCalibration()
{    
    pmath->performProcessing();

    if (pmath->isStarLost())
    {
        emit newLog(i18n("Lost track of the guide star. Try increasing the square size or reducing pulse duration."));
        reset();

        calibrationStage = CAL_ERROR;
        emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);

        return;
    }

    switch (calibrationType)
    {
    case CAL_NONE:
        break;

    case CAL_RA_AUTO:
        calibrateRADECRecticle(true);
        break;

    case CAL_RA_DEC_AUTO:
        calibrateRADECRecticle(false);
        break;
    }
}

void InternalGuider::setGuideView(FITSView *guideView)
{
    guideFrame = guideView;

    pmath->setGuideView(guideFrame);
}

void InternalGuider::reset()
{
    state = GUIDE_IDLE;
    //calibrationStage = CAL_IDLE;
    connect(guideFrame, SIGNAL(trackingStarSelected(int,int)), this, SLOT(trackingStarSelected(int, int)), Qt::UniqueConnection);
}

void InternalGuider::calibrateRADECRecticle( bool ra_only )
{
    bool auto_term_ok = false;

    Q_ASSERT(pmath);

    int pulseDuration = Options::calibrationPulseDuration();
    int totalPulse    = pulseDuration * Options::autoModeIterations();

    if (ra_only)
        calibrationType = CAL_RA_AUTO;
    else
        calibrationType = CAL_RA_DEC_AUTO;

    switch(calibrationStage)
    {

    case CAL_START:
        //----- automatic mode -----
        auto_drift_time = Options::autoModeIterations();

        if (ra_only)
            turn_back_time = auto_drift_time*2 + auto_drift_time/2;
        else
            turn_back_time = auto_drift_time*6;
        iterations = 0;

        emit newLog(i18n("RA drifting forward..."));

        pmath->getReticleParameters(&start_x1, &start_y1, NULL);

        if (Options::guideLogging())
            qDebug() << "Guide: Start X1 " << start_x1 << " Start Y1 " << start_y1;

        emit newPulse( RA_INC_DIR, pulseDuration );

        if (Options::guideLogging())
            qDebug() << "Guide: Iteration " << iterations << " Direction: " << RA_INC_DIR << " Duration: " << pulseDuration << " ms.";

        iterations++;

        calibrationStage = CAL_RA_INC;

        break;

    case CAL_RA_INC:
        emit newPulse( RA_INC_DIR, pulseDuration );

        if (Options::guideLogging())
        {
            // Star position resulting from LAST guiding pulse to mount
            double cur_x, cur_y;
            pmath->getStarScreenPosition( &cur_x, &cur_y );
            qDebug() << "Guide: Iteration #" << iterations-1 << ": STAR " << cur_x << "," << cur_y;
            qDebug() << "Guide: Iteration " << iterations << " Direction: " << RA_INC_DIR << " Duration: " << pulseDuration << " ms.";
        }

        iterations++;

        if (iterations == auto_drift_time)
            calibrationStage = CAL_RA_DEC;

        break;

    case CAL_RA_DEC:
    {
        if (iterations == auto_drift_time)
        {
            pmath->getStarScreenPosition( &end_x1, &end_y1 );
            if (Options::guideLogging())
                qDebug() << "Guide: End X1 " << end_x1 << " End Y1 " << end_y1;

            phi = pmath->calculatePhi( start_x1, start_y1, end_x1, end_y1 );
            ROT_Z = RotateZ( -M_PI*phi/180.0 ); // derotates...

            emit newLog(i18n("RA drifting reverse..."));

        }

        //----- Z-check (new!) -----
        double cur_x, cur_y;
        pmath->getStarScreenPosition( &cur_x, &cur_y );

        Vector star_pos = Vector( cur_x, cur_y, 0 ) - Vector( start_x1, start_y1, 0 );
        star_pos.y = -star_pos.y;
        star_pos = star_pos * ROT_Z;

        if (Options::guideLogging())
            qDebug() << "Guide: Star x pos is " << star_pos.x << " from original point.";

        // start point reached... so exit
        if( star_pos.x < 1.5 )
        {
            pmath->performProcessing();
            auto_term_ok = true;
        }

        //----- Z-check end -----

        if( !auto_term_ok )
        {
            if (iterations < turn_back_time)
            {
                emit newPulse( RA_DEC_DIR, pulseDuration );

                if (Options::guideLogging())
                {
                    // Star position resulting from LAST guiding pulse to mount
                    double cur_x, cur_y;
                    pmath->getStarScreenPosition( &cur_x, &cur_y );
                    qDebug() << "Guide: Iteration #" << iterations-1 << ": STAR " << cur_x << "," << cur_y;
                    qDebug() << "Guide: Iteration " << iterations << " Direction: " << RA_INC_DIR << " Duration: " << pulseDuration << " ms.";
                }

                iterations++;
                break;
            }

            calibrationStage = CAL_ERROR;

            emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);

            emit newLog(i18np("Guide RA: Scope cannot reach the start point after %1 iteration. Possible mount or drive problems...", "GUIDE_RA: Scope cannot reach the start point after %1 iterations. Possible mount or drive problems...", turn_back_time));

            KNotification::event( QLatin1String( "CalibrationFailed" ) , i18n("Guiding calibration failed with errors"));
            reset();
            break;
        }

        if (ra_only == false)
        {
            calibrationStage = CAL_DEC_INC;
            start_x2 = cur_x;
            start_y2 = cur_y;

            if (Options::guideLogging())
                qDebug() << "Guide: Start X2 " << start_x2 << " start Y2 " << start_y2;

            emit newPulse( DEC_INC_DIR, pulseDuration );

            if (Options::guideLogging())
            {
                // Star position resulting from LAST guiding pulse to mount
                double cur_x, cur_y;
                pmath->getStarScreenPosition( &cur_x, &cur_y );
                qDebug() << "Guide: Iteration #" << iterations-1 << ": STAR " << cur_x << "," << cur_y;
                qDebug() << "Guide: Iteration " << iterations << " Direction: " << RA_INC_DIR << " Duration: " << pulseDuration << " ms.";
            }

            iterations++;
            dec_iterations = 1;
            emit newLog(i18n("DEC drifting forward..."));
            break;
        }
        // calc orientation
        if( pmath->calculateAndSetReticle1D( start_x1, start_y1, end_x1, end_y1, totalPulse) )
        {
            calibrationStage = CAL_IDLE;

            // FIXME what is this for?
            //fillInterface();

            emit newStatus(Ekos::GUIDE_CALIBRATION_SUCESS);

            KNotification::event( QLatin1String( "CalibrationSuccessful" ) , i18n("Guiding calibration completed successfully"));
            //if (ui.autoStarCheck->isChecked())
            //guideModule->selectAutoStar();
        }
        else
        {
            emit newLog(i18n("Calibration rejected. Star drift is too short."));

            calibrationStage = CAL_ERROR;

            emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);

            KNotification::event( QLatin1String( "CalibrationFailed" ) , i18n("Guiding calibration failed with errors"));
        }

        reset();
        break;
    }

    case CAL_DEC_INC:
        emit newPulse( DEC_INC_DIR, pulseDuration );

        if (Options::guideLogging())
        {
            // Star position resulting from LAST guiding pulse to mount
            double cur_x, cur_y;
            pmath->getStarScreenPosition( &cur_x, &cur_y );
            qDebug() << "Guide: Iteration #" << iterations-1 << ": STAR " << cur_x << "," << cur_y;
            qDebug() << "Guide: Iteration " << iterations << " Direction: " << RA_INC_DIR << " Duration: " << pulseDuration << " ms.";
        }

        iterations++;
        dec_iterations++;

        if (dec_iterations == auto_drift_time)
            calibrationStage = CAL_DEC_DEC;

        break;

    case CAL_DEC_DEC:
    {
        if (dec_iterations == auto_drift_time)
        {
            pmath->getStarScreenPosition( &end_x2, &end_y2 );
            if (Options::guideLogging())
                qDebug() << "Guide: End X2 " << end_x2 << " End Y2 " << end_y2;

            phi = pmath->calculatePhi( start_x2, start_y2, end_x2, end_y2 );
            ROT_Z = RotateZ( -M_PI*phi/180.0 ); // derotates...

            emit newLog(i18n("DEC drifting reverse..."));
        }

        //----- Z-check (new!) -----
        double cur_x, cur_y;
        pmath->getStarScreenPosition( &cur_x, &cur_y );

        //pmain_wnd->appendLogText(i18n("GUIDE_DEC running back...");

        if (Options::guideLogging())
            qDebug() << "Guide: Cur X2 " << cur_x << " Cur Y2 " << cur_y;

        Vector star_pos = Vector( cur_x, cur_y, 0 ) - Vector( start_x2, start_y2, 0 );
        star_pos.y = -star_pos.y;
        star_pos = star_pos * ROT_Z;

        if (Options::guideLogging())
            qDebug() << "Guide: start Pos X " << star_pos.x << " from original point.";

        // start point reached... so exit
        if( star_pos.x < 1.5 )
        {
            pmath->performProcessing();
            auto_term_ok = true;
        }

        //----- Z-check end -----

        if( !auto_term_ok )
        {
            if (iterations < turn_back_time)
            {
                emit newPulse(DEC_DEC_DIR, pulseDuration );

                if (Options::guideLogging())
                {
                    // Star position resulting from LAST guiding pulse to mount
                    double cur_x, cur_y;
                    pmath->getStarScreenPosition( &cur_x, &cur_y );
                    qDebug() << "Guide: Iteration #" << iterations-1 << ": STAR " << cur_x << "," << cur_y;
                    qDebug() << "Guide: Iteration " << iterations << " Direction: " << RA_INC_DIR << " Duration: " << pulseDuration << " ms.";
                }

                iterations++;
                dec_iterations++;
                break;
            }

            calibrationStage = CAL_ERROR;

            emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);

            emit newLog(i18np("Guide DEC: Scope cannot reach the start point after %1 iteration.\nPossible mount or drive problems...", "GUIDE_DEC: Scope cannot reach the start point after %1 iterations.\nPossible mount or drive problems...", turn_back_time));

            KNotification::event( QLatin1String( "CalibrationFailed" ) , i18n("Guiding calibration failed with errors"));
            reset();
            break;
        }

        bool swap_dec=false;
        // calc orientation
        if( pmath->calculateAndSetReticle2D( start_x1, start_y1, end_x1, end_y1, start_x2, start_y2, end_x2, end_y2, &swap_dec, totalPulse ) )
        {
            calibrationStage = CAL_IDLE;
            //fillInterface();
            if (swap_dec)
                emit newLog(i18n("DEC swap enabled."));
            else
                emit newLog(i18n("DEC swap disabled."));

            emit newStatus(Ekos::GUIDE_CALIBRATION_SUCESS);

            emit DESwapChanged(swap_dec);

            KNotification::event( QLatin1String( "CalibrationSuccessful" ) , i18n("Guiding calibration completed successfully"));

            //if (ui.autoStarCheck->isChecked())
            //guideModule->selectAutoStar();

        }
        else
        {
            emit newLog(i18n("Calibration rejected. Star drift is too short."));

            emit newStatus(Ekos::GUIDE_CALIBRATION_ERROR);

            //ui.startCalibrationLED->setColor(alertColor);
            calibrationStage = CAL_ERROR;
            KNotification::event( QLatin1String( "CalibrationFailed" ) , i18n("Guiding calibration failed with errors"));
        }

        reset();

        break;
    }


    default:
        break;

    }
}

void InternalGuider::setStarPosition(QVector3D starCenter)
{
    pmath->setReticleParameters(starCenter.x(), starCenter.y(), -1);
}

void InternalGuider::trackingStarSelected(int x, int y)
{
    if (calibrationStage == CAL_IDLE)
        return;

    //int square_size = guide_squares[pmath->getSquareIndex()].size;

    pmath->setReticleParameters(x, y, -1);
    //pmath->moveSquare(x-square_size/(2*pmath->getBinX()), y-square_size/(2*pmath->getBinY()));

    //update_reticle_pos(x, y);



    //ui.selectStarLED->setColor(okColor);

    calibrationStage = CAL_START;

    //ui.pushButton_StartCalibration->setEnabled(true);

    /*QVector3D starCenter;
    starCenter.setX(x);
    starCenter.setY(y);
    emit newStarPosition(starCenter, true);*/

    //if (ui.autoStarCheck->isChecked())
    //if (Options::autoStarEnabled())
    //calibrate();
}

void InternalGuider::setDECSwap(bool enable)
{
    pmath->setDeclinationSwapEnabled(enable);
}

void InternalGuider::setSquareAlgorithm(int index)
{
    pmath->setSquareAlgorithm(index);
}

void InternalGuider::setReticleParameters(double x, double y, double angle)
{
    pmath->setReticleParameters(x,y,angle);
}

bool InternalGuider::getReticleParameters(double *x, double *y, double *angle)
{
    return pmath->getReticleParameters(x,y,angle);
}

bool InternalGuider::setGuiderParams(double ccdPixelSizeX, double ccdPixelSizeY, double mountAperture, double mountFocalLength)
{
    this->ccdPixelSizeX     = ccdPixelSizeX;
    this->ccdPixelSizeY     = ccdPixelSizeY;
    this->mountAperture     = mountAperture;
    this->mountFocalLength  = mountFocalLength;
    return pmath->setGuiderParameters(ccdPixelSizeX, ccdPixelSizeY, mountAperture, mountFocalLength);
}

bool InternalGuider::setFrameParams(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t binX, uint16_t binY)
{
    if( w <= 0 || h <= 0 )
        return false;

    subX = x;
    subY = y;
    subW = w;
    subH = h;

    subBinX = binX;
    subBinY = binY;

    pmath->setVideoParameters(w, h, subBinX, subBinY);

    return true;
}

bool InternalGuider::processGuiding()
{
    static int maxPulseCounter=0;
    const cproc_out_params *out;
    uint32_t tick = 0;

    // On first frame, center the box (reticle) around the star so we do not start with an offset the results in
    // unnecessary guiding pulses.
    if (isFirstFrame)
    {
        if (state == GUIDE_GUIDING)
        {
            Vector star_pos = pmath->findLocalStarPosition();
            pmath->setReticleParameters(star_pos.x, star_pos.y, -1);
        }
        isFirstFrame=false;
    }

    // calc math. it tracks square
    pmath->performProcessing();

    if (pmath->isStarLost() && ++m_lostStarTries > 2)
    {
        emit newLog(i18n("Lost track of the guide star. Try increasing the square size and check the mount."));
        abort();
        return false;
    }
    else
        m_lostStarTries = 0;

    // do pulse
    out = pmath->getOutputParameters();

    // If within 90% of max pulse repeatedly, let's abort
    if (out->pulse_length[GUIDE_RA] >=  (0.9 * Options::rAMaximumPulse()) || out->pulse_length[GUIDE_DEC] >= (0.9 * Options::dECMaximumPulse()))
        maxPulseCounter++;
    else
        maxPulseCounter=0;

    if (maxPulseCounter >= 3)
    {
        emit newLog(i18n("Lost track of the guide star. Aborting guiding..."));
        abort();
        maxPulseCounter=0;
        return false;
    }

    emit newPulse( out->pulse_dir[GUIDE_RA], out->pulse_length[GUIDE_RA], out->pulse_dir[GUIDE_DEC], out->pulse_length[GUIDE_DEC] );

    emit frameCaptureRequested();

    if (state == GUIDE_DITHERING)
        return true;

    tick = pmath->getTicks();

    if( tick & 1 )
    {
        // draw some params in window
        emit newAxisDelta(out->delta[GUIDE_RA], out->delta[GUIDE_DEC]);

        emit newAxisPulse(out->pulse_length[GUIDE_RA], out->pulse_length[GUIDE_DEC]);

        emit newAxisSigma(out->sigma[GUIDE_RA], out->sigma[GUIDE_DEC]);
    }

    // skip half frames
    //if( half_refresh_rate && (tick & 1) )
    //return;

    //drift_graph->on_paint();
    //pDriftOut->update();

    return true;
}

}
