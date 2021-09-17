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

#include <QVector3D>
#include <cmath>
#include <set>

GuiderUtils::Vector cgmath::findLocalStarPosition(QSharedPointer<FITSData> &imageData,
        GuideView *guideView)
{
    if (usingSEPMultiStar())
    {
        QRect trackingBox = guideView->getTrackingBox();
        return guideStars.findGuideStar(imageData, trackingBox, guideView);
    }

    return GuideAlgorithms::findLocalStarPosition(
               imageData, algorithm, video_width, video_height,
               guideView->getTrackingBox());
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

    logFile.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("guide_log.txt"));
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

    out << "Guiding rate,x15 arcsec/sec: " << endl;
    out << "Focal,mm: " << calibration.getFocalLength() << endl;
    out << "Aperture,mm: " << aperture << endl;
    out << "F/D: " << calibration.getFocalLength() / aperture << endl;
    out << "Frame #, Time Elapsed (ms), RA Error (arcsec), RA Correction (ms), RA Correction Direction, DEC Error "
        "(arcsec), DEC Correction (ms), DEC Correction Direction"
        << endl;

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

void cgmath::calculatePulses(void)
{
    qCDebug(KSTARS_EKOS_GUIDE) << "Processing Axes";

    in_params.proportional_gain[0] = Options::rAProportionalGain();
    in_params.proportional_gain[1] = Options::dECProportionalGain();

    in_params.integral_gain[0] = Options::rAIntegralGain();
    in_params.integral_gain[1] = Options::dECIntegralGain();

    in_params.enabled[0] = Options::rAGuideEnabled();
    in_params.enabled[1] = Options::dECGuideEnabled();

    in_params.min_pulse_arcsec[0] = Options::rAMinimumPulseArcSec();
    in_params.min_pulse_arcsec[1] = Options::dECMinimumPulseArcSec();

    in_params.max_pulse_arcsec[0] = Options::rAMaximumPulseArcSec();
    in_params.max_pulse_arcsec[1] = Options::dECMaximumPulseArcSec();

    // RA W/E enable
    // East RA+ enabled?
    in_params.enabled_axis1[0] = Options::eastRAGuideEnabled();
    // West RA- enabled?
    in_params.enabled_axis2[0] = Options::westRAGuideEnabled();

    // DEC N/S enable
    // North DEC+ enabled?
    in_params.enabled_axis1[1] = Options::northDECGuideEnabled();
    // South DEC- enabled?
    in_params.enabled_axis2[1] = Options::southDECGuideEnabled();

    // process axes...
    for (int k = GUIDE_RA; k <= GUIDE_DEC; k++)
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

        // Compute the average drift in the recent past for the integral control term.
        drift_integral[k] = 0;
        for (int i = 0; i < CIRCULAR_BUFFER_SIZE; ++i)
            drift_integral[k] += drift[k][i];
        drift_integral[k] /= (double)CIRCULAR_BUFFER_SIZE;

        qCDebug(KSTARS_EKOS_GUIDE) << "drift[" << axisStr(k) << "] = " << arcsecDrift
                                   << " integral[" << axisStr(k) << "] = " << drift_integral[k];

        // GPG pulse computation
        bool useGPG = Options::gPGEnabled() && (k == GUIDE_RA) && in_params.enabled[k];
        if (useGPG && gpg->computePulse(arcsecDrift,
                                        usingSEPMultiStar() ? &guideStars : nullptr, &pulseLength, &dir, calibration))
        {
            pulseDirection = dir;
            pulseLength = std::min(pulseLength, static_cast<int>(maxPulseMilliseconds + 0.5));
        }
        else
        {
            // This is the main non-GPG guide-pulse computation.
            // Traditionally it was hardwired so that proportional_gain=133 was about a control gain of 1.0
            // This is now in the 0.0 - 1.0 range, and multiplies the calibrated mount performance.

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
                        pulseDirection = arcsecDrift > 0 ? DEC_INC_DIR : DEC_DEC_DIR; // GUIDE_DEC.
                }
                else
                    pulseDirection = NO_DIR;
            }

        }
        qCDebug(KSTARS_EKOS_GUIDE) << "pulse_length[" << axisStr(k) << "] = " << pulseLength
                                   << "ms, Direction = " << directionStr(pulseDirection);

        out_params.pulse_dir[k]  = pulseDirection;
        out_params.pulse_length[k] = pulseLength;
        out_params.delta[k] = arcsecDrift;
    }

    if (Options::guideLogging())
    {
        QTextStream out(&logFile);
        out << iterationCounter << "," << logTime.elapsed() << "," << out_params.delta[0] << "," << out_params.pulse_length[0] <<
            ","
            << directionStr(out_params.pulse_dir[0]) << "," << out_params.delta[1] << ","
            << out_params.pulse_length[1] << "," << directionStr(out_params.pulse_dir[1]) << endl;
    }
}

void cgmath::performProcessing(Ekos::GuideState state, QSharedPointer<FITSData> &imageData,
                               GuideView *guideView, GuideLog *logger)
{
    if (suspended)
    {
        if (Options::gPGEnabled())
        {
            GuiderUtils::Vector guideStarPosition = findLocalStarPosition(imageData, guideView);
            if (guideStarPosition.x != -1 && !std::isnan(guideStarPosition.x))
            {
                gpg->suspended(guideStarPosition, targetPosition,
                               usingSEPMultiStar() ? &guideStars : nullptr, calibration);
            }
        }
        // do nothing if suspended
        return;
    }

    GuiderUtils::Vector starPositionArcSec, targetPositionArcSec;

    // find guiding star location in the image
    starPosition = findLocalStarPosition(imageData, guideView);

    // If no star found, mark as lost star.
    if (starPosition.x == -1 || std::isnan(starPosition.x))
    {
        lost_star = true;
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
        lost_star = false;

    // Emit the detected star center
    QVector3D starCenter(starPosition.x, starPosition.y, 0);
    emit newStarPosition(starCenter, true);

    // If we're only calibrating, then we're done.
    if (state == Ekos::GUIDE_CALIBRATING)
        return;

    qCDebug(KSTARS_EKOS_GUIDE) << "################## BEGIN PROCESSING ##################";

    // translate star coords into sky coord. system

    // convert from pixels into arcsecs
    starPositionArcSec    = calibration.convertToArcseconds(starPosition);
    targetPositionArcSec = calibration.convertToArcseconds(targetPosition);

    qCDebug(KSTARS_EKOS_GUIDE) << "Star    X : " << starPosition.x << " Y  : " << starPosition.y;
    qCDebug(KSTARS_EKOS_GUIDE) << "Reticle X : " << targetPosition.x << " Y  :" << targetPosition.y;
    qCDebug(KSTARS_EKOS_GUIDE) << "Star    RA: " << starPositionArcSec.x << " DEC: " << starPositionArcSec.y;
    qCDebug(KSTARS_EKOS_GUIDE) << "Reticle RA: " << targetPositionArcSec.x << " DEC: " << targetPositionArcSec.y;

    // Compute RA & DEC drift in arcseconds.
    GuiderUtils::Vector star_drift = starPositionArcSec - targetPositionArcSec;
    star_drift = calibration.rotateToRaDec(star_drift);

    // both coords are ready for math processing
    // put coord to drift list
    // Note: if we're not guiding, these will be overwritten,
    // as driftUpto is only incremented when guiding.
    drift[GUIDE_RA][driftUpto[GUIDE_RA]]   = star_drift.x;
    drift[GUIDE_DEC][driftUpto[GUIDE_DEC]] = star_drift.y;

    qCDebug(KSTARS_EKOS_GUIDE) << "-------> AFTER ROTATION  Diff RA: " << star_drift.x << " DEC: " << star_drift.y;
    qCDebug(KSTARS_EKOS_GUIDE) << "RA index: " << driftUpto[GUIDE_RA]
                               << " DEC index: " << driftUpto[GUIDE_DEC];

    if (state == Ekos::GUIDE_GUIDING && usingSEPMultiStar())
    {
        double multiStarRADrift, multiStarDECDrift;
        if (guideStars.getDrift(sqrt(star_drift.x * star_drift.x + star_drift.y * star_drift.y),
                                targetPosition.x, targetPosition.y,
                                &multiStarRADrift, &multiStarDECDrift))
        {
            qCDebug(KSTARS_EKOS_GUIDE) << "-------> MultiStar:      Diff RA: " << multiStarRADrift << " DEC: " << multiStarDECDrift;
            drift[GUIDE_RA][driftUpto[GUIDE_RA]]   = multiStarRADrift;
            drift[GUIDE_DEC][driftUpto[GUIDE_DEC]] = multiStarDECDrift;
        }
        else
        {
            qCDebug(KSTARS_EKOS_GUIDE) << "-------> MultiStar: failed, fell back to guide star";
        }
    }

    // driftUpto will change when the circular buffer is updated,
    // so save the values for logging.
    const double raDrift = drift[GUIDE_RA][driftUpto[GUIDE_RA]];
    const double decDrift = drift[GUIDE_DEC][driftUpto[GUIDE_DEC]];

    // make decision by axes
    calculatePulses();

    if (state == Ekos::GUIDE_GUIDING)
    {
        calculateRmsError();
        emitStats();
        updateCircularBuffers();
    }

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
                                     0 : (out_params.pulse_dir[GUIDE_RA] == RA_DEC_DIR ? -1.0 : 1.0);
        const double decGuideFactor = out_params.pulse_dir[GUIDE_DEC] == NO_DIR ?
                                      0 : (out_params.pulse_dir[GUIDE_DEC] == DEC_INC_DIR ? -1.0 : 1.0);

        data.raGuideDistance = raGuideFactor * out_params.pulse_length[GUIDE_RA] /
                               calibration.raPulseMillisecondsPerPixel();
        data.decGuideDistance = decGuideFactor * out_params.pulse_length[GUIDE_DEC] /
                                calibration.decPulseMillisecondsPerPixel();

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
    qCDebug(KSTARS_EKOS_GUIDE) << "################## FINISH PROCESSING ##################";
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

