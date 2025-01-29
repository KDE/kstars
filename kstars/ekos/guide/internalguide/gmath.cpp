/*
    SPDX-FileCopyrightText: 2012 Andrew Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars:
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "gmath.h"

#include "Options.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsview.h"
#include "auxiliary/kspaths.h"
#include "ekos_guide_debug.h"
#include "ekos/auxiliary/stellarsolverprofileeditor.h"
#include "guidealgorithms.h"
#include "guidelog.h"
#include "../guideview.h"

#include <QVector3D>
#include <cmath>
#include <set>

// Qt version calming
#include <qtendl.h>

#include <QRandomGenerator>
namespace
{
QRandomGenerator generator;
double getNoise(double maxAbsVal)
{
    return (maxAbsVal - generator.bounded(maxAbsVal * 2));
}
}
GuiderUtils::Vector cgmath::findLocalStarPosition(QSharedPointer<FITSData> &imageData,
        QSharedPointer<GuideView> &guideView, bool firstFrame)
{
    GuiderUtils::Vector position;
    if (usingSEPMultiStar())
    {
        QRect trackingBox = guideView->getTrackingBox();
        position = guideStars.findGuideStar(imageData, trackingBox, guideView, firstFrame);

    }
    else
        position = GuideAlgorithms::findLocalStarPosition(
                       imageData, algorithm, video_width, video_height,
                       guideView->getTrackingBox());

    if (position.x == -1 || position.y == -1)
        setLostStar(true);
    return position;
}


cgmath::cgmath() : QObject()
{
    // sky coord. system vars.
    starPosition = GuiderUtils::Vector(0);
    targetPosition = GuiderUtils::Vector(0);

    // processing
    in_params.reset();
    out_params.reset();
    driftUpto[GUIDE_RA] = driftUpto[GUIDE_DEC] = 0;
    drift[GUIDE_RA]                                = new double[CIRCULAR_BUFFER_SIZE];
    drift[GUIDE_DEC]                               = new double[CIRCULAR_BUFFER_SIZE];
    memset(drift[GUIDE_RA], 0, sizeof(double) * CIRCULAR_BUFFER_SIZE);
    memset(drift[GUIDE_DEC], 0, sizeof(double) * CIRCULAR_BUFFER_SIZE);
    drift_integral[GUIDE_RA] = drift_integral[GUIDE_DEC] = 0;

    logFile.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("guide_log.txt"));
    gpg.reset(new GPG());
}

cgmath::~cgmath()
{
    delete[] drift[GUIDE_RA];
    delete[] drift[GUIDE_DEC];
}

bool cgmath::setVideoParameters(int vid_wd, int vid_ht, int binX, int binY)
{
    if (vid_wd <= 0 || vid_ht <= 0)
        return false;

    video_width  = vid_wd / binX;
    video_height = vid_ht / binY;

    calibration.setBinningUsed(binX, binY);
    guideStars.setCalibration(calibration);

    return true;
}

bool cgmath::setGuiderParameters(double ccd_pix_wd, double ccd_pix_ht, double guider_aperture, double guider_focal)
{
    if (ccd_pix_wd < 0)
        ccd_pix_wd = 0;
    if (ccd_pix_ht < 0)
        ccd_pix_ht = 0;
    if (guider_focal <= 0)
        guider_focal = 1;

    aperture = guider_aperture;
    guideStars.setCalibration(calibration);

    return true;
}

// This logging will be removed in favor of guidelog.h.
void cgmath::createGuideLog()
{
    logFile.close();
    logFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&logFile);

    out << "Guiding rate,x15 arcsec/sec: " << Qt::endl;
    out << "Focal,mm: " << calibration.getFocalLength() << Qt::endl;
    out << "Aperture,mm: " << aperture << Qt::endl;
    out << "F/D: " << calibration.getFocalLength() / aperture << Qt::endl;
    out << "Frame #, Time Elapsed (ms), RA Error (arcsec), RA Correction (ms), RA Correction Direction, DEC Error "
        "(arcsec), DEC Correction (ms), DEC Correction Direction"
        << Qt::endl;

    logTime.restart();
}

bool cgmath::setTargetPosition(double x, double y)
{
    // check frame ranges
    if (x < 0)
        x = 0;
    if (y < 0)
        y = 0;
    if (x >= (double)video_width - 1)
        x = (double)video_width - 1;
    if (y >= (double)video_height - 1)
        y = (double)video_height - 1;

    targetPosition = GuiderUtils::Vector(x, y, 0);

    guideStars.setCalibration(calibration);

    return true;
}

bool cgmath::getTargetPosition(double *x, double *y) const
{
    *x = targetPosition.x;
    *y = targetPosition.y;
    return true;
}

int cgmath::getAlgorithmIndex(void) const
{
    return algorithm;
}

void cgmath::getStarScreenPosition(double *dx, double *dy) const
{
    *dx = starPosition.x;
    *dy = starPosition.y;
}

bool cgmath::reset()
{
    iterationCounter = 0;
    driftUpto[GUIDE_RA] = driftUpto[GUIDE_DEC] = 0;
    drift_integral[GUIDE_RA] = drift_integral[GUIDE_DEC] = 0;
    out_params.reset();

    memset(drift[GUIDE_RA], 0, sizeof(double) * CIRCULAR_BUFFER_SIZE);
    memset(drift[GUIDE_DEC], 0, sizeof(double) * CIRCULAR_BUFFER_SIZE);

    return true;
}

void cgmath::setAlgorithmIndex(int algorithmIndex)
{
    if (algorithmIndex < 0 || algorithmIndex > SEP_MULTISTAR)
        return;

    algorithm = algorithmIndex;
}

bool cgmath::usingSEPMultiStar() const
{
    return algorithm == SEP_MULTISTAR;
}

void cgmath::updateCircularBuffers(void)
{
    iterationCounter++;

    driftUpto[GUIDE_RA]++;
    driftUpto[GUIDE_DEC]++;
    if (driftUpto[GUIDE_RA] >= CIRCULAR_BUFFER_SIZE)
        driftUpto[GUIDE_RA] = 0;
    if (driftUpto[GUIDE_DEC] >= CIRCULAR_BUFFER_SIZE)
        driftUpto[GUIDE_DEC] = 0;
}

//-------------------- Processing ---------------------------
void cgmath::start()
{
    iterationCounter                   = 0;
    driftUpto[GUIDE_RA] = driftUpto[GUIDE_DEC] = 0;
    drift_integral[GUIDE_RA] = drift_integral[GUIDE_DEC] = 0;
    out_params.reset();

    memset(drift[GUIDE_RA], 0, sizeof(double) * CIRCULAR_BUFFER_SIZE);
    memset(drift[GUIDE_DEC], 0, sizeof(double) * CIRCULAR_BUFFER_SIZE);

    if (calibration.getFocalLength() > 0 && aperture > 0)
        createGuideLog();

    gpg->reset();
}

void cgmath::abort()
{
    guideStars.reset();
}

void cgmath::suspend(bool mode)
{
    suspended = mode;
}

bool cgmath::isSuspended() const
{
    return suspended;
}

bool cgmath::isStarLost() const
{
    return lost_star;
}

void cgmath::setLostStar(bool is_lost)
{
    lost_star = is_lost;
}

namespace
{
QString axisStr(int raDEC)
{
    if (raDEC == GUIDE_RA)
        return "RA";
    else if (raDEC == GUIDE_DEC)
        return "DEC";
    else
        return "???";
}

const QString directionStr(GuideDirection dir)
{
    switch (dir)
    {
        case RA_DEC_DIR:
            return "Decrease RA";
        case RA_INC_DIR:
            return "Increase RA";
        case DEC_DEC_DIR:
            return "Decrease DEC";
        case DEC_INC_DIR:
            return "Increase DEC";
        default:
            return "NO DIR";
    }
}
}  // namespace

bool cgmath::configureInParams(Ekos::GuideState state)
{
    const bool dithering = state == Ekos::GuideState::GUIDE_DITHERING;

    if (!dithering)
    {
        in_params.proportional_gain[0] = Options::rAProportionalGain();
        in_params.proportional_gain[1] = Options::dECProportionalGain();

        in_params.integral_gain[0] = Options::rAIntegralGain();
        in_params.integral_gain[1] = Options::dECIntegralGain();

        // Always pulse if we're dithering.
        in_params.enabled[0] = Options::rAGuideEnabled();
        in_params.enabled[1] = Options::dECGuideEnabled();

        in_params.min_pulse_arcsec[0] = Options::rAMinimumPulseArcSec();
        in_params.min_pulse_arcsec[1] = Options::dECMinimumPulseArcSec();

        in_params.max_pulse_arcsec[0] = Options::rAMaximumPulseArcSec();
        in_params.max_pulse_arcsec[1] = Options::dECMaximumPulseArcSec();

        // RA W/E enable (but always pulse if dithering).
        // East RA+ enabled?
        in_params.enabled_axis1[0] = Options::eastRAGuideEnabled();
        // West RA- enabled?
        in_params.enabled_axis2[0] = Options::westRAGuideEnabled();

        // DEC N/S enable (but always pulse if dithering).
        // North DEC+ enabled?
        in_params.enabled_axis1[1] = Options::northDECGuideEnabled();
        // South DEC- enabled?
        in_params.enabled_axis2[1] = Options::southDECGuideEnabled();
    }
    else
    {
        // If we're dithering, enable all axes and use full pulses.
        in_params.proportional_gain[0] = 1.0;
        in_params.proportional_gain[1] = 1.0;
        in_params.integral_gain[0] = 0.0;
        in_params.integral_gain[1] = 0.0;
        in_params.min_pulse_arcsec[0] = 0.0;
        in_params.min_pulse_arcsec[1] = 0.0;
        in_params.max_pulse_arcsec[0] = Options::rAMaximumPulseArcSec();
        in_params.max_pulse_arcsec[1] = Options::dECMaximumPulseArcSec();
        in_params.enabled[0] = true;
        in_params.enabled[1] = true;
        in_params.enabled_axis1[0] = true;
        in_params.enabled_axis2[0] = true;
        in_params.enabled_axis1[1] = true;
        in_params.enabled_axis2[1] = true;
    }

    return dithering;
}

void cgmath::updateOutParams(int k, const double arcsecDrift, int pulseLength, GuideDirection pulseDirection)
{
    out_params.pulse_dir[k]  = pulseDirection;
    out_params.pulse_length[k] = pulseLength;
    out_params.delta[k] = arcsecDrift;
}

void cgmath::outputGuideLog()
{
    if (Options::guideLogging())
    {
        QTextStream out(&logFile);
        out << iterationCounter << "," << logTime.elapsed() << "," << out_params.delta[0] << "," << out_params.pulse_length[0] <<
            ","
            << directionStr(out_params.pulse_dir[0]) << "," << out_params.delta[1] << ","
            << out_params.pulse_length[1] << "," << directionStr(out_params.pulse_dir[1]) << Qt::endl;
    }
}

void cgmath::processAxis(const int k, const bool dithering, const bool darkGuide, const Seconds &timeStep,
                         const QString &label)
{
    // zero all out commands
    GuideDirection pulseDirection = NO_DIR;
    int pulseLength = 0;  // milliseconds
    GuideDirection dir;

    // Get the drift for this axis
    const int idx = driftUpto[k];
    const double arcsecDrift = drift[k][idx];

    const double pulseConverter = (k == GUIDE_RA) ?
                                  calibration.raPulseMillisecondsPerArcsecond() :
                                  calibration.decPulseMillisecondsPerArcsecond();
    const double maxPulseMilliseconds = in_params.max_pulse_arcsec[k] * pulseConverter;

    // GPG pulse computation
    bool useGPG = !dithering && Options::gPGEnabled() && (k == GUIDE_RA) && in_params.enabled[k];
    if (useGPG && darkGuide)
    {
        gpg->darkGuiding(&pulseLength, &dir, calibration, timeStep);
        pulseDirection = dir;
    }
    else if (darkGuide)
    {
        // We should not be dark guiding without GPG
        qCDebug(KSTARS_EKOS_GUIDE) << "Warning: dark guiding without GPG or while dithering.";
        return;
    }
    else if (useGPG && gpg->computePulse(arcsecDrift,
                                         usingSEPMultiStar() ? &guideStars : nullptr, &pulseLength, &dir, calibration, timeStep))
    {
        pulseDirection = dir;
        pulseLength = std::min(pulseLength, static_cast<int>(maxPulseMilliseconds + 0.5));
    }
    else
    {
        // This is the main non-GPG guide-pulse computation.
        // Traditionally it was hardwired so that proportional_gain=133 was about a control gain of 1.0
        // This is now in the 0.0 - 1.0 range, and multiplies the calibrated mount performance.

        // Compute the average drift in the recent past for the integral control term.
        drift_integral[k] = 0;
        for (int i = 0; i < CIRCULAR_BUFFER_SIZE; ++i)
            drift_integral[k] += drift[k][i];
        drift_integral[k] /= (double)CIRCULAR_BUFFER_SIZE;

        if (in_params.integral_gain[k] > 0)
            qCDebug(KSTARS_EKOS_GUIDE) << label << "drift[" << axisStr(k) << "] = " << arcsecDrift
                                       << " integral[" << axisStr(k) << "] = " << drift_integral[k];

        const double arcsecPerMsPulse = k == GUIDE_RA ? calibration.raPulseMillisecondsPerArcsecond() :
                                        calibration.decPulseMillisecondsPerArcsecond();
        const double proportionalResponse = arcsecDrift * in_params.proportional_gain[k] * arcsecPerMsPulse;
        const double integralResponse = drift_integral[k] * in_params.integral_gain[k] * arcsecPerMsPulse;
        pulseLength = std::min(fabs(proportionalResponse + integralResponse), maxPulseMilliseconds);

        // calc direction
        // We do not send pulse if direction is disabled completely, or if direction in a specific axis (e.g. N or S) is disabled
        if (!in_params.enabled[k] || // This axis not enabled
                // Positive direction of this axis not enabled.
                (arcsecDrift > 0 && !in_params.enabled_axis1[k]) ||
                // Negative direction of this axis not enabled.
                (arcsecDrift < 0 && !in_params.enabled_axis2[k]))
        {
            pulseDirection = NO_DIR;
            pulseLength = 0;
        }
        else
        {
            // Check the min pulse value, and assign the direction.
            const double pulseArcSec = pulseConverter > 0 ? pulseLength / pulseConverter : 0;
            if (pulseArcSec >= in_params.min_pulse_arcsec[k])
            {
                if (k == GUIDE_RA)
                    pulseDirection = arcsecDrift > 0 ? RA_DEC_DIR : RA_INC_DIR;
                else
                    pulseDirection = arcsecDrift > 0 ? DEC_DEC_DIR : DEC_INC_DIR;
            }
            else
                pulseDirection = NO_DIR;
        }

    }
    updateOutParams(k, arcsecDrift, pulseLength, pulseDirection);
}

void cgmath::calculatePulses(Ekos::GuideState state, const std::pair<Seconds, Seconds> &timeStep)
{
    const bool dithering = configureInParams(state);

    processAxis(GUIDE_RA, dithering, false, timeStep.first, "Guiding:");
    processAxis(GUIDE_DEC, dithering, false, timeStep.second, "Guiding:");

    qCDebug(KSTARS_EKOS_GUIDE)
            << QString("Guiding pulses: RA: %1ms %2  DEC: %3ms %4")
            .arg(out_params.pulse_length[GUIDE_RA]).arg(directionStr(out_params.pulse_dir[GUIDE_RA]))
            .arg(out_params.pulse_length[GUIDE_DEC]).arg(directionStr(out_params.pulse_dir[GUIDE_DEC]));

    outputGuideLog();
}

void cgmath::performProcessing(Ekos::GuideState state, QSharedPointer<FITSData> &imageData,
                               QSharedPointer<GuideView> &guideView,
                               const std::pair<Seconds, Seconds> &timeStep, GuideLog * logger)
{
    if (suspended)
    {
        if (Options::gPGEnabled())
        {
            GuiderUtils::Vector guideStarPosition = findLocalStarPosition(imageData, guideView, false);
            if (guideStarPosition.x != -1 && !std::isnan(guideStarPosition.x))
            {
                gpg->suspended(guideStarPosition, targetPosition,
                               usingSEPMultiStar() ? &guideStars : nullptr, calibration);
            }
        }
        // do nothing if suspended
        return;
    }

    QElapsedTimer timer;
    timer.start();
    GuiderUtils::Vector starPositionArcSec, targetPositionArcSec;

    // find guiding star location in the image
    starPosition = findLocalStarPosition(imageData, guideView, false);

    // If no star found, mark as lost star.
    if (starPosition.x == -1 || std::isnan(starPosition.x))
    {
        setLostStar(true);
        if (logger != nullptr && state == Ekos::GUIDE_GUIDING)
        {
            GuideLog::GuideData data;
            data.code = GuideLog::GuideData::NO_STAR_FOUND;
            data.type = GuideLog::GuideData::DROP;
            logger->addGuideData(data);
        }
        return;
    }
    else
        setLostStar(false);

    // Emit the detected star center
    QVector3D starCenter(starPosition.x, starPosition.y, 0);
    emit newStarPosition(starCenter, true);

    // If we're only calibrating, then we're done.
    if (state == Ekos::GUIDE_CALIBRATING)
        return;

    if (state == Ekos::GUIDE_GUIDING && (targetPosition.x <= 0.0 || targetPosition.y <= 0.0))
    {
        qCDebug(KSTARS_EKOS_GUIDE) << "Guiding with target 0.0 -- something's wrong!!!!!!!!!!!";
        for (int k = GUIDE_RA; k <= GUIDE_DEC; k++)
        {
            out_params.pulse_dir[k]  = NO_DIR;
            out_params.pulse_length[k] = 0;
            out_params.delta[k] = 0;
            setLostStar(true);
        }
        return;
    }
    // translate star coords into sky coord. system

    // convert from pixels into arcsecs
    starPositionArcSec    = calibration.convertToArcseconds(starPosition);
    targetPositionArcSec = calibration.convertToArcseconds(targetPosition);

    // Compute RA & DEC drift in arcseconds.
    const GuiderUtils::Vector star_xy_arcsec_drift = starPositionArcSec - targetPositionArcSec;
    const GuiderUtils::Vector star_drift = calibration.rotateToRaDec(star_xy_arcsec_drift);

    // both coords are ready for math processing
    // put coord to drift list
    // Note: if we're not guiding, these will be overwritten,
    // as driftUpto is only incremented when guiding.
    drift[GUIDE_RA][driftUpto[GUIDE_RA]]   = star_drift.x;
    drift[GUIDE_DEC][driftUpto[GUIDE_DEC]] = star_drift.y;

    qCDebug(KSTARS_EKOS_GUIDE)
            << QString("Star %1 %2 a-s %3 %4 Target %5 %6 a-s %7 %8 Drift: RA %9 DEC %10")
            .arg(starPosition.x, 0, 'f', 1)        .arg(starPosition.y, 0, 'f', 1)
            .arg(starPositionArcSec.x, 0, 'f', 1)  .arg(starPositionArcSec.y, 0, 'f', 1)
            .arg(targetPosition.x, 0, 'f', 1)      .arg(targetPosition.y, 0, 'f', 1)
            .arg(targetPositionArcSec.x, 0, 'f', 1).arg(targetPositionArcSec.y, 0, 'f', 1)
            .arg(star_drift.x, 0, 'f', 2)          .arg(star_drift.y, 0, 'f', 2);

    if (state == Ekos::GUIDE_GUIDING && usingSEPMultiStar())
    {
        double multiStarRADrift, multiStarDECDrift;
        if (guideStars.getDrift(sqrt(star_drift.x * star_drift.x + star_drift.y * star_drift.y),
                                targetPosition.x, targetPosition.y,
                                &multiStarRADrift, &multiStarDECDrift))
        {
#ifdef DEBUG_ADD_NOISE
            const double raNoise = getNoise(.75);
            const double decNoise = getNoise(.75);
            multiStarRADrift += raNoise;
            multiStarDECDrift += decNoise;
            qCDebug(KSTARS_EKOS_GUIDE)
                    << "Added guide noise arcsecs" << "RA" << raNoise << "DEC" << decNoise;
            fprintf(stderr, "Add guide noise arcsecs %.1f %.1f\n", raNoise, decNoise);
#endif
            qCDebug(KSTARS_EKOS_GUIDE) << QString("MultiStar drift: RA %1 DEC %2")
                                       .arg(multiStarRADrift, 0, 'f', 2)
                                       .arg(multiStarDECDrift, 0, 'f', 2);
            drift[GUIDE_RA][driftUpto[GUIDE_RA]]   = multiStarRADrift;
            drift[GUIDE_DEC][driftUpto[GUIDE_DEC]] = multiStarDECDrift;
        }
        else
        {
            qCDebug(KSTARS_EKOS_GUIDE) << "MultiStar: failed, fell back to guide star";
        }
    }

    // driftUpto will change when the circular buffer is updated,
    // so save the values for logging.
    const double raDrift = drift[GUIDE_RA][driftUpto[GUIDE_RA]];
    const double decDrift = drift[GUIDE_DEC][driftUpto[GUIDE_DEC]];

    // make decision by axes
    calculatePulses(state, timeStep);

    if (state == Ekos::GUIDE_GUIDING)
    {
        calculateRmsError();
        emitStats();
        updateCircularBuffers();
    }
    qCDebug(KSTARS_EKOS_GUIDE) << QString("performProcessing took %1s").arg(timer.elapsed() / 1000.0, 0, 'f', 3);

    if (logger != nullptr)
    {
        GuideLog::GuideData data;
        data.type = GuideLog::GuideData::MOUNT;
        // These are distances in pixels.
        // Note--these don't include the multistar algorithm, but the below ra/dec ones do.
        data.dx = starPosition.x - targetPosition.x;
        data.dy = starPosition.y - targetPosition.y;
        // Above computes position - reticle. Should the reticle-position, so negate.
        calibration.convertToPixels(-raDrift, -decDrift, &data.raDistance, &data.decDistance);

        const double raGuideFactor = out_params.pulse_dir[GUIDE_RA] == NO_DIR ?
                                     0 : (out_params.pulse_dir[GUIDE_RA] == RA_INC_DIR ? 1.0 : -1.0);
        const double decGuideFactor = out_params.pulse_dir[GUIDE_DEC] == NO_DIR ?
                                      0 : (out_params.pulse_dir[GUIDE_DEC] == DEC_INC_DIR ? 1.0 : -1.0);

        // Phd2LogViewer wants these in pixels instead of arcseconds, so normalizing them, but
        // that will be wrong for non-square pixels. They should really accept arcsecond units.
        data.raGuideDistance = calibration.xPixelsPerArcsecond() * raGuideFactor * out_params.pulse_length[GUIDE_RA] /
                               calibration.raPulseMillisecondsPerArcsecond();
        data.decGuideDistance = calibration.yPixelsPerArcsecond() * decGuideFactor * out_params.pulse_length[GUIDE_DEC] /
                                calibration.decPulseMillisecondsPerArcsecond();

        data.raDuration = out_params.pulse_dir[GUIDE_RA] == NO_DIR ? 0 : out_params.pulse_length[GUIDE_RA];
        data.raDirection = out_params.pulse_dir[GUIDE_RA];
        data.decDuration = out_params.pulse_dir[GUIDE_DEC] == NO_DIR ? 0 : out_params.pulse_length[GUIDE_DEC];
        data.decDirection = out_params.pulse_dir[GUIDE_DEC];
        data.code = GuideLog::GuideData::NO_ERRORS;
        data.snr = guideStars.getGuideStarSNR();
        data.mass = guideStars.getGuideStarMass();
        // Add SNR and MASS from SEP stars.
        logger->addGuideData(data);
    }
}

void cgmath::performDarkGuiding(Ekos::GuideState state, const std::pair<Seconds, Seconds> &timeStep)
{

    const bool dithering = configureInParams(state);
    //out_params.sigma[GUIDE_RA] = 0;

    processAxis(GUIDE_RA, dithering, true, timeStep.first, "Dark Guiding:");
    qCDebug(KSTARS_EKOS_GUIDE)
            << QString("Dark Guiding pulses: RA: %1ms %2")
            .arg(out_params.pulse_length[GUIDE_RA]).arg(directionStr(out_params.pulse_dir[GUIDE_RA]));


    // Don't guide in DEC when dark guiding
    updateOutParams(GUIDE_DEC, 0, 0, NO_DIR);

    outputGuideLog();
}

void cgmath::emitStats()
{
    double pulseRA = 0;
    if (out_params.pulse_dir[GUIDE_RA] == RA_DEC_DIR)
        pulseRA = out_params.pulse_length[GUIDE_RA];
    else if (out_params.pulse_dir[GUIDE_RA] == RA_INC_DIR)
        pulseRA = -out_params.pulse_length[GUIDE_RA];
    double pulseDEC = 0;
    if (out_params.pulse_dir[GUIDE_DEC] == DEC_DEC_DIR)
        pulseDEC = -out_params.pulse_length[GUIDE_DEC];
    else if (out_params.pulse_dir[GUIDE_DEC] == DEC_INC_DIR)
        pulseDEC = out_params.pulse_length[GUIDE_DEC];

    const bool hasGuidestars = usingSEPMultiStar();
    const double snr = hasGuidestars ? guideStars.getGuideStarSNR() : 0;
    const double skyBG = hasGuidestars ? guideStars.skybackground().mean : 0;
    const int numStars = hasGuidestars ? guideStars.skybackground().starsDetected : 0;  // wait for rob's release

    emit guideStats(-out_params.delta[GUIDE_RA], -out_params.delta[GUIDE_DEC],
                    pulseRA, pulseDEC, snr, skyBG, numStars);
}

void cgmath::calculateRmsError(void)
{
    if (!do_statistics)
        return;

    if (iterationCounter == 0)
        return;

    int count = std::min(iterationCounter, static_cast<unsigned int>(CIRCULAR_BUFFER_SIZE));
    for (int k = GUIDE_RA; k <= GUIDE_DEC; k++)
    {
        double sqr_avg = 0;
        for (int i = 0; i < count; ++i)
            sqr_avg += drift[k][i] * drift[k][i];

        out_params.sigma[k] = sqrt(sqr_avg / (double)count);
    }
}


QVector3D cgmath::selectGuideStar(const QSharedPointer<FITSData> &imageData)
{
    return guideStars.selectGuideStar(imageData);
}

double cgmath::getGuideStarSNR()
{
    return guideStars.getGuideStarSNR();
}

//---------------------------------------------------------------------------------------
cproc_in_params::cproc_in_params()
{
    reset();
}

void cproc_in_params::reset(void)
{
    average           = true;

    for (int k = GUIDE_RA; k <= GUIDE_DEC; k++)
    {
        enabled[k]          = true;
        integral_gain[k]    = 0;
        max_pulse_arcsec[k] = 5000;
        min_pulse_arcsec[k] = 0;
    }
}

cproc_out_params::cproc_out_params()
{
    reset();
}

void cproc_out_params::reset(void)
{
    for (int k = GUIDE_RA; k <= GUIDE_DEC; k++)
    {
        delta[k]        = 0;
        pulse_dir[k]    = NO_DIR;
        pulse_length[k] = 0;
        sigma[k]        = 0;
    }
}

