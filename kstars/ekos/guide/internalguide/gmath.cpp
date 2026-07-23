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
#include "auxiliary/ksnotification.h"
#include "ekos_guide_debug.h"
#include "guidealgorithms.h"
#include "guidelog.h"
#include "../guideview.h"
#include "linearguider.h"
#include "hysteresisguider.h"
#include "ekos/guide/opsguide.h"
#include "mount_guider_factory.h"

#include <QVector3D>
#include <cmath>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>

// Qt version calming
#include <qtendl.h>

namespace
{

bool raAlgorithmIsAI()
{
    return Options::rAGuidePulseAlgorithm() == Ekos::OpsGuide::AI_ALGORITHM;
}
bool decAlgorithmIsAI()
{
    return Options::dECGuidePulseAlgorithm() == (Ekos::OpsGuide::AI_ALGORITHM - 1);
}
}  // namespace

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
                       imageData, m_StarDetectionAlgorithm, video_width, video_height,
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
    m_RALinearGuider.reset( new LinearGuider("RA"));
    m_DECLinearGuider.reset(new LinearGuider("DEC"));
    m_RAHysteresisGuider.reset( new HysteresisGuider("RA"));
    m_DECHysteresisGuider.reset(new HysteresisGuider("DEC"));
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

bool cgmath::setGuiderParameters(double guider_aperture)
{
    aperture = guider_aperture;
    guideStars.setCalibration(calibration);

    return true;
}

// This logging will be removed in favor of guidelog.h.
void cgmath::createGuideLog()
{
    logFile.close();
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qCWarning(KSTARS_EKOS_GUIDE) << "Failed to open guide log file:" << logFile.fileName() << logFile.errorString();
        return;
    }
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

    m_AILoggedActive = false;
    m_AILoggedFullConfidence = false;
    m_AILoggedWarmup = false;

    // Close previous debug file so a new session gets a fresh log file
    if (m_AIDebugFile)
    {
        delete m_AIDebugFile;
        m_AIDebugFile = nullptr;
    }
    m_AIDebugHeaderWritten = false;

    memset(drift[GUIDE_RA], 0, sizeof(double) * CIRCULAR_BUFFER_SIZE);
    memset(drift[GUIDE_DEC], 0, sizeof(double) * CIRCULAR_BUFFER_SIZE);

    return true;
}

void cgmath::setStarDetectionAlgorithmIndex(int algorithmIndex)
{
    if (algorithmIndex < 0 || algorithmIndex > SEP_MULTISTAR)
        return;

    m_StarDetectionAlgorithm = algorithmIndex;
}

bool cgmath::usingSEPMultiStar() const
{
    return m_StarDetectionAlgorithm == SEP_MULTISTAR;
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

    m_accumulated_pulse_ra = 0.0;
    m_accumulated_pulse_dec = 0.0;

    memset(drift[GUIDE_RA], 0, sizeof(double) * CIRCULAR_BUFFER_SIZE);
    memset(drift[GUIDE_DEC], 0, sizeof(double) * CIRCULAR_BUFFER_SIZE);

    if (calibration.getFocalLength() > 0 && aperture > 0)
        createGuideLog();

    gpg->reset();
    m_RALinearGuider->reset();
    m_DECLinearGuider->reset();
    m_RAHysteresisGuider->reset();
    m_DECHysteresisGuider->reset();

    m_sessionStartTime = 0.0;
    m_AILoggedActive = false;
    m_AILoggedFullConfidence = false;
    m_AILoggedWarmup = false;
    setAIState(AIGuideState::DISABLED);

    const bool useAIAlgorithm = (raAlgorithmIsAI() || decAlgorithmIsAI());
    const bool shadowRequested = Options::aIShadowMode();

    if (useAIAlgorithm || shadowRequested)
    {
        m_AIGuider.reset();
        // AI guiding requires weights explicitly loaded from the UI (AI button -> Load Weights,
        // or the AI Assistant). No silent auto-locate of a default file.
        const QString weightsPath = Options::aIGuiderWeightsFile().toLocalFile();

        if (!weightsPath.isEmpty() && QFile::exists(weightsPath))
        {
            m_AIGuider = MountGuiderFactory::createFromWeights(weightsPath);
            if (m_AIGuider && m_AIGuider->loadWeights(weightsPath))
            {
                if (useAIAlgorithm)
                {
                    qCWarning(KSTARS_EKOS_GUIDE) << "=======================================================";
                    qCWarning(KSTARS_EKOS_GUIDE) << ">>> AI GUIDER IS ACTIVE! Weights loaded:" << weightsPath;
                    qCWarning(KSTARS_EKOS_GUIDE) << "=======================================================";
                    setAIState(AIGuideState::WARMUP);
                }
                else
                {
                    qCInfo(KSTARS_EKOS_GUIDE) << "[AI GUIDER] SHADOW mode: predictions logged, NOT applied. Weights:" << weightsPath;
                    emit newLog("[AI GUIDER] Shadow mode active: AI running silently alongside standard guiding.");
                    setAIState(AIGuideState::SHADOW);
                }
                m_AIGuider->resetSession();
            }
            else
            {
                qCWarning(KSTARS_EKOS_GUIDE) << ">>> AI GUIDER FAILED TO LOAD WEIGHTS OR FINGERPRINT MISMATCH:" << weightsPath;
                if (useAIAlgorithm)
                {
                    emit newLog(i18n("AI Guider failed to load weights or fingerprint mismatched. Guiding aborted."));
                    KSNotification::error(
                        i18n("AI Guider failed to load weights or Fingerprint mismatched!\n\nRe-run the Guide AI Assistant to generate new weights for your current settings, or switch the Guide Algorithm back to a standard mode. Guiding has been aborted."),
                        i18n("AI Guider Error"));
                }
                m_AIGuider.reset();
                setAIState(AIGuideState::DISABLED);
            }
        }
        else
        {
            qCWarning(KSTARS_EKOS_GUIDE) << ">>> AI GUIDER: no weights file configured.";
            if (useAIAlgorithm)
            {
                emit newLog(i18n("AI Guiding is selected but no trained weights are loaded. Load them via the AI "
                                 "button > 'Load Weights...', or run the Guide AI Assistant. Guiding aborted."));
                KSNotification::error(
                    i18n("AI Guiding is selected but no trained weights are loaded.\n\n"
                     "Load a weights file (AI button > Load Weights...) or run the Guide AI Assistant to generate "
                     "weights for your current settings. Guiding has been aborted."),
                    i18n("AI Guider: No Weights"));
            }
            setAIState(AIGuideState::DISABLED);
        }
    }
    else
    {
        m_AIGuider.reset();
        setAIState(AIGuideState::DISABLED);
    }

    // AI was selected as the guide algorithm but the AI guider could not be loaded (no weights,
    // missing file, or fingerprint mismatch). InternalGuider::guide() checks this and aborts the
    // session instead of silently guiding with the standard algorithm.
    m_aiRequiredButUnavailable = useAIAlgorithm && !(m_AIGuider && m_AIGuider->isLoaded());
}

void cgmath::abort()
{
    guideStars.reset();
    m_RALinearGuider->reset();
    m_DECLinearGuider->reset();
    m_RAHysteresisGuider->reset();
    m_DECHysteresisGuider->reset();
}

void cgmath::setAIState(AIGuideState s)
{
    m_aiState = s;
    const double conf = (m_AIGuider && m_AIGuider->isLoaded()) ? m_AIGuider->confidence() : 0.0;
    emit newAIState(static_cast<int>(s), conf);
}

void cgmath::suspend(bool mode)
{
    suspended = mode;
    m_RALinearGuider->reset();
    m_DECLinearGuider->reset();
    m_RAHysteresisGuider->reset();
    m_DECHysteresisGuider->reset();
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

void cgmath::updateOutParams(int k, const double arcsecDrift, int pulseLength, GuideDirection pulseDirection,
                             bool accumulate)
{
    out_params.pulse_dir[k]  = pulseDirection;
    out_params.pulse_length[k] = pulseLength;
    out_params.delta[k] = arcsecDrift;

    if (!accumulate)
        return;

    // Accumulate the signed pulse length
    double signed_pulse = pulseLength;
    if (k == GUIDE_RA && pulseDirection == RA_DEC_DIR)
    {
        signed_pulse = -signed_pulse;
    }
    else if (k == GUIDE_DEC && pulseDirection == DEC_DEC_DIR)
    {
        signed_pulse = -signed_pulse;
    }

    if (k == GUIDE_RA)
    {
        m_accumulated_pulse_ra += signed_pulse;
    }
    else
    {
        m_accumulated_pulse_dec += signed_pulse;
    }
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

    if (dithering)
    {
        m_RALinearGuider->reset();
        m_DECLinearGuider->reset();
        m_RAHysteresisGuider->reset();
        m_DECHysteresisGuider->reset();
    }
    LinearGuider *lGuider = nullptr;
    HysteresisGuider *hGuider = nullptr;
    bool useGPG = false;
    bool useAI = false;
    if (!dithering)
    {
        if ((Options::rAGuidePulseAlgorithm() == Ekos::OpsGuide::HYSTERESIS_ALGORITHM) && k == GUIDE_RA)
            hGuider = m_RAHysteresisGuider.get();
        else if ((Options::dECGuidePulseAlgorithm() == Ekos::OpsGuide::HYSTERESIS_ALGORITHM) && k == GUIDE_DEC)
            hGuider = m_DECHysteresisGuider.get();
        else if ((Options::rAGuidePulseAlgorithm() == Ekos::OpsGuide::LINEAR_ALGORITHM) && k == GUIDE_RA)
            lGuider = m_RALinearGuider.get();
        else if ((Options::dECGuidePulseAlgorithm() == Ekos::OpsGuide::LINEAR_ALGORITHM) && k == GUIDE_DEC)
            lGuider = m_DECLinearGuider.get();
        else if ((Options::rAGuidePulseAlgorithm() == Ekos::OpsGuide::GPG_ALGORITHM) && (k == GUIDE_RA)
                 && in_params.enabled[k])
            useGPG = true;
        else if (raAlgorithmIsAI() && (k == GUIDE_RA) && in_params.enabled[k])
            useAI = true;
        else if (decAlgorithmIsAI() && (k == GUIDE_DEC) && in_params.enabled[k])
            useAI = true;
    }

    if (useGPG && darkGuide)
    {
        gpg->darkGuiding(&pulseLength, &dir, calibration, timeStep);
        pulseDirection = dir;
    }
    else if (useAI && darkGuide && m_AIGuider && m_AIGuider->isLoaded())
    {
        double dt_sec = timeStep.count();
        GuideOutput ai_out = m_AIGuider->darkPredict(dt_sec);
        if (ai_out.valid)
        {
            double ai_pulse_arcsec = (k == GUIDE_RA) ? ai_out.ra_correction_arcsec : ai_out.dec_correction_arcsec;
            const double aiGain = Options::aIPredictionGain();
            const double aiResponse = ai_pulse_arcsec * pulseConverter;
            double total = aiGain * ai_out.confidence * aiResponse;

            qCDebug(KSTARS_EKOS_GUIDE) << QString("[AI GUIDER] Dark Guiding [%1] | dt=%2s, conf=%3, AIResponse=%4ms -> Total=%5ms")
                                       .arg(k == GUIDE_RA ? "RA" : "DEC").arg(dt_sec, 0, 'f', 1).arg(ai_out.confidence, 0, 'f', 2)
                                       .arg(aiResponse, 0, 'f', 1).arg(total, 0, 'f', 1);

            pulseLength = std::min(std::abs(total), maxPulseMilliseconds);
            pulseDirection = (k == GUIDE_RA) ?
                             (total > 0 ? RA_DEC_DIR : RA_INC_DIR) :
                             (total > 0 ? DEC_DEC_DIR : DEC_INC_DIR);

            // Only suppress if the axis is disabled entirely.
            // Note: min_pulse_arcsec is NOT checked here for dark guiding,
            // matching GPG's behavior. Dark guiding pulses are intentionally
            // small and should not be filtered by the normal minimum pulse threshold.
            if (!in_params.enabled[k])
            {
                pulseDirection = NO_DIR;
                pulseLength = 0;
            }
        }
    }
    else if (darkGuide)
    {
        // We should not be dark guiding without GPG or AI
        qCDebug(KSTARS_EKOS_GUIDE) << "Warning: dark guiding without GPG or AI, or while dithering.";
        return;
    }
    else if (useGPG && gpg->computePulse(arcsecDrift,
                                         usingSEPMultiStar() ? &guideStars : nullptr, &pulseLength, &dir, calibration, timeStep))
    {
        pulseDirection = dir;
        pulseLength = std::min(pulseLength, static_cast<int>(maxPulseMilliseconds + 0.5));
    }
    else if (lGuider != nullptr)
    {
        // If we haven't been here in 2 exposures, should probably reset the linear guider.
        // Similarly if we've dithered.
        // Search for all gpg resets
        // Also when it gets started or restarted...
        //
        // Also when I enable/disable the checkboxes, I should reset things appropriately

        double pulse = 0;
        if (k == GUIDE_RA)
        {
            lGuider->setGain(Options::rAProportionalGain());
            lGuider->setMinMove(Options::rAMinimumPulseArcSec());
            pulse = lGuider->guide(arcsecDrift) * calibration.raPulseMillisecondsPerArcsecond();
            pulseDirection = pulse > 0 ? RA_DEC_DIR : RA_INC_DIR;
        }
        else
        {
            lGuider->setGain(Options::dECProportionalGain());
            lGuider->setMinMove(Options::dECMinimumPulseArcSec());
            pulse = lGuider->guide(arcsecDrift) * calibration.decPulseMillisecondsPerArcsecond();
            pulseDirection = pulse > 0 ? DEC_DEC_DIR : DEC_INC_DIR;
        }
        pulseLength = std::min(std::abs(pulse), maxPulseMilliseconds);
    }
    else if (hGuider != nullptr)
    {
        double pulse = 0;
        if (k == GUIDE_RA)
        {
            hGuider->setGain(Options::rAProportionalGain());
            hGuider->setMinMove(Options::rAMinimumPulseArcSec());
            hGuider->setHysteresis(Options::rAHysteresis());
            pulse = hGuider->guide(arcsecDrift) * calibration.raPulseMillisecondsPerArcsecond();
            pulseDirection = pulse > 0 ? RA_DEC_DIR : RA_INC_DIR;
        }
        else
        {
            hGuider->setGain(Options::dECProportionalGain());
            hGuider->setMinMove(Options::dECMinimumPulseArcSec());
            hGuider->setHysteresis(Options::dECHysteresis());
            pulse = hGuider->guide(arcsecDrift) * calibration.decPulseMillisecondsPerArcsecond();
            pulseDirection = pulse > 0 ? DEC_DEC_DIR : DEC_INC_DIR;
        }
        pulseLength = std::min(std::abs(pulse), maxPulseMilliseconds);
    }
    else if (useAI && m_AIGuider && m_AIGuider->isLoaded() && m_lastAIPrediction.valid)
    {
        double ai_pulse_arcsec = (k == GUIDE_RA) ?
                                 m_lastAIPrediction.ra_correction_arcsec :
                                 m_lastAIPrediction.dec_correction_arcsec;
        const double conf = m_lastAIPrediction.confidence;
        const double aiGain = Options::aIPredictionGain();

        // Compute the average drift in the recent past for the integral control term.
        drift_integral[k] = 0;
        for (int i = 0; i < CIRCULAR_BUFFER_SIZE; ++i)
            drift_integral[k] += drift[k][i];
        drift_integral[k] /= (double)CIRCULAR_BUFFER_SIZE;

        const double proportionalResponse = arcsecDrift * in_params.proportional_gain[k] * pulseConverter;
        const double integralResponse = drift_integral[k] * in_params.integral_gain[k] * pulseConverter;
        const double aiResponse = ai_pulse_arcsec * pulseConverter;

        // Scale down proportional response when AI is confident (if enabled)
        double activePropGain = 1.0;
        if (Options::aIProportionalBackoff())
        {
            activePropGain -= (aiGain * conf * 0.5); // Reduce P-gain by up to 50%
        }

        // The total response restores the PID integral term to fight steady-state errors
        double total = (proportionalResponse * activePropGain) + integralResponse + (aiGain * conf * aiResponse);

        qCDebug(KSTARS_EKOS_GUIDE) <<
                                   QString("[AI GUIDER] Blending [%1] | PropResponse=%2ms (ActiveGain=%3), IntResponse=%4ms, AIResponse=%5ms, Conf=%6 Gain=%7 -> Total=%8ms")
                                   .arg(k == GUIDE_RA ? "RA" : "DEC").arg(proportionalResponse, 0, 'f', 1).arg(activePropGain, 0, 'f', 2)
                                   .arg(integralResponse, 0, 'f', 1).arg(aiResponse, 0, 'f', 1).arg(conf, 0, 'f', 2).arg(aiGain, 0, 'f', 2).arg(total, 0, 'f',
                                           1);

        pulseLength = std::min(std::abs(total), maxPulseMilliseconds);
        pulseDirection = (k == GUIDE_RA) ?
                         (total > 0 ? RA_DEC_DIR : RA_INC_DIR) :
                         (total > 0 ? DEC_DEC_DIR : DEC_INC_DIR);

        if (!in_params.enabled[k] ||
                (arcsecDrift > 0 && !in_params.enabled_axis1[k]) ||
                (arcsecDrift < 0 && !in_params.enabled_axis2[k]))
        {
            pulseDirection = NO_DIR;
            pulseLength = 0;
        }
        else
        {
            const double pulseArcSec = pulseConverter > 0 ? pulseLength / pulseConverter : 0;
            if (pulseArcSec < in_params.min_pulse_arcsec[k])
            {
                pulseDirection = NO_DIR;
                pulseLength = 0;
            }
        }
    }
    else
    {
        if (useAI && m_AIGuider && m_AIGuider->isLoaded() && !m_lastAIPrediction.valid)
        {
            qCDebug(KSTARS_EKOS_GUIDE) << QString("[AI GUIDER] Warmup active! Relying on Standard PID for %1").arg(
                                           k == GUIDE_RA ? "RA" : "DEC");
        }
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
        pulseLength = std::min(std::abs(proportionalResponse + integralResponse), maxPulseMilliseconds);

        // calculation of correcting mount pulse
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
                // Star drifts are based on pixel differences of the star position in the camera sensor coordinate
                // system: To correct a star drift > 0 the mount has to move in decreasing RA- & DEC-direction
                if (k == GUIDE_RA)
                    pulseDirection = arcsecDrift > 0 ? RA_DEC_DIR : RA_INC_DIR;
                else
                    pulseDirection = arcsecDrift > 0 ? DEC_DEC_DIR : DEC_INC_DIR;
            }
            else
                pulseDirection = NO_DIR;
        }

    }
    // Limit the pulse length in case of ridiculous values.
    constexpr int MAX_PULSE_MILLISECONDS = 5000;
    if (pulseLength > MAX_PULSE_MILLISECONDS)
    {
        qCDebug(KSTARS_EKOS_GUIDE) << i18n("Limited long pulse of %1ms to %2ms", pulseLength, MAX_PULSE_MILLISECONDS);
        pulseLength = MAX_PULSE_MILLISECONDS;
    }
    updateOutParams(k, arcsecDrift, pulseLength, pulseDirection, !darkGuide);
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
        m_RALinearGuider->reset();
        m_DECLinearGuider->reset();
        m_RAHysteresisGuider->reset();
        m_DECHysteresisGuider->reset();
        if (Options::rAGuidePulseAlgorithm() == Ekos::OpsGuide::GPG_ALGORITHM)
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
    Q_EMIT newStarPosition(starCenter, true);

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

    // --- AI Guider feed-forward prediction ---
    if (m_AIGuider && m_AIGuider->isLoaded() && state == Ekos::GUIDE_GUIDING)
    {
        GuideFrameData frameData;

        // Pixel scale is in arcseconds per pixel
        frameData.pixel_scale = calibration.xPixelsPerArcsecond();
        if (frameData.pixel_scale == 0) frameData.pixel_scale = 1.0;
        else frameData.pixel_scale = 1.0 / frameData.pixel_scale;

        // Convert the Ekos arcsecond drift back into raw camera pixels for the AI
        double ra_px = raDrift / frameData.pixel_scale;
        double dec_px = decDrift / frameData.pixel_scale;

        frameData.ra_raw_px    = ra_px;
        frameData.dec_raw_px   = dec_px;
        frameData.snr          = guideStars.getGuideStarSNR();
        // Use absolute epoch time to prevent phase-slip across aborts, dithers, or cloud passes
        static qint64 baseEpoch = QDateTime::currentMSecsSinceEpoch();
        double current_time_sec = (QDateTime::currentMSecsSinceEpoch() - baseEpoch) / 1000.0;
        if (m_sessionStartTime == 0.0)
        {
            frameData.dt = timeStep.first.count();
        }
        else
        {
            frameData.dt = current_time_sec - m_sessionStartTime;
            if (frameData.dt <= 0.001) frameData.dt = timeStep.first.count(); // Fallback
        }
        m_sessionStartTime = current_time_sec;
        frameData.t_session_sec = current_time_sec;

        // Fetch altitude from FITS header if available
        QVariant altVariant;
        if (imageData->getRecordValue("OBJCTALT", altVariant))
        {
            frameData.altitude_deg = altVariant.toDouble();
        }
        else
        {
            frameData.altitude_deg = 45.0;
        }

        // Fetch azimuth from FITS header if available
        QVariant azVariant;
        if (imageData->getRecordValue("OBJCTAZ", azVariant))
        {
            frameData.azimuth_deg = azVariant.toDouble();
        }
        else
        {
            frameData.azimuth_deg = 180.0;
        }

        // Fetch DEC for parallactic angle computation
        double dec_target = 0.0;
        QVariant decVariant;
        if (imageData->getRecordValue("OBJCTDEC", decVariant))
        {
            bool ok;
            double d = decVariant.toDouble(&ok);
            if (ok) dec_target = d;
        }

        // Fetch Latitude
        double lat_target = 45.0;
        QVariant latVariant;
        if (imageData->getRecordValue("SITELAT", latVariant))
        {
            bool ok;
            double l = latVariant.toDouble(&ok);
            if (ok) lat_target = l;
        }

        // Calculate Parallactic Angle (q)
        double sin_az = std::sin(frameData.azimuth_deg * M_PI / 180.0);
        double cos_lat = std::cos(lat_target * M_PI / 180.0);
        double cos_dec = std::cos(dec_target * M_PI / 180.0);

        if (std::abs(cos_dec) > 1e-6)
        {
            double sin_q = (sin_az * cos_lat) / cos_dec;
            sin_q = std::clamp(sin_q, -1.0, 1.0);
            frameData.parallactic_angle_deg = std::asin(sin_q) * 180.0 / M_PI;
        }
        else
        {
            frameData.parallactic_angle_deg = 0.0;
        }

        // Fetch PierSide from FITS header if available
        QVariant pierVariant;
        if (imageData->getRecordValue("PIERSIDE", pierVariant))
        {
            QString pierStr = pierVariant.toString().toUpper();
            frameData.pier_side_east = (pierStr == "EAST");
        }
        else
        {
            frameData.pier_side_east = false; // default
        }

        static bool last_pier_side_east = frameData.pier_side_east;
        // If this is the first time we're setting it up, just initialize it without resetting
        static bool pier_initialized = false;

        if (!pier_initialized)
        {
            last_pier_side_east = frameData.pier_side_east;
            pier_initialized = true;
        }
        else if (last_pier_side_east != frameData.pier_side_east)
        {
            qCWarning(KSTARS_EKOS_GUIDE) << "Meridian flip detected (PierSide changed from"
                                         << (last_pier_side_east ? "EAST" : "WEST") << "to"
                                         << (frameData.pier_side_east ? "EAST" : "WEST")
                                         << ")! Resetting AI Guider session and physics state.";
            m_AIGuider->resetSession(true); // Force reset RLS and warmup
            last_pier_side_east = frameData.pier_side_east;
        }

        // Signed sum of the pulses applied since the last frame (the accumulator stores
        // RA_DEC_DIR / DEC_DEC_DIR as negative). The AI backs these out to recover the raw
        // physical drift, so the sign must be preserved here.
        frameData.ra_pulse_ms = m_accumulated_pulse_ra;
        frameData.dec_pulse_ms = m_accumulated_pulse_dec;

        // Forward the real guide-rate calibration so a guider can convert pulse ms -> arcsec with
        // the SAME factor the uncorrected-drift computation below uses (0 if not yet calibrated).
        frameData.ra_ms_per_arcsec  = calibration.raPulseMillisecondsPerArcsecond();
        frameData.dec_ms_per_arcsec = calibration.decPulseMillisecondsPerArcsecond();

        // Reset the accumulators now that we have captured the sum of all pulses since the last real frame
        m_accumulated_pulse_ra = 0.0;
        m_accumulated_pulse_dec = 0.0;

        // Calculate Uncorrected Physical Drift for RLS Phase Estimation
        int prev_idx = (driftUpto[GUIDE_RA] - 1 + CIRCULAR_BUFFER_SIZE) % CIRCULAR_BUFFER_SIZE;
        double prev_ra_arcsec = drift[GUIDE_RA][prev_idx];
        double prev_dec_arcsec = drift[GUIDE_DEC][prev_idx];

        double applied_pulse_arcsec_ra = 0.0;
        if (calibration.raPulseMillisecondsPerArcsecond() > 0)
        {
            applied_pulse_arcsec_ra = std::abs(frameData.ra_pulse_ms) / calibration.raPulseMillisecondsPerArcsecond();
            // Apply sign
            if (frameData.ra_pulse_ms < 0) applied_pulse_arcsec_ra = -applied_pulse_arcsec_ra;
        }

        double applied_pulse_arcsec_dec = 0.0;
        if (calibration.decPulseMillisecondsPerArcsecond() > 0)
        {
            applied_pulse_arcsec_dec = std::abs(frameData.dec_pulse_ms) / calibration.decPulseMillisecondsPerArcsecond();
            if (frameData.dec_pulse_ms < 0) applied_pulse_arcsec_dec = -applied_pulse_arcsec_dec;
        }

        double uncorrected_drift_ra_arcsec = raDrift - prev_ra_arcsec - applied_pulse_arcsec_ra;
        double uncorrected_drift_dec_arcsec = decDrift - prev_dec_arcsec - applied_pulse_arcsec_dec;

        double uncorrected_drift_ra_px = uncorrected_drift_ra_arcsec / frameData.pixel_scale;
        double uncorrected_drift_dec_px = uncorrected_drift_dec_arcsec / frameData.pixel_scale;

        m_AIGuider->update(ra_px, dec_px, uncorrected_drift_ra_px, uncorrected_drift_dec_px, frameData.snr,
                           applied_pulse_arcsec_ra / frameData.pixel_scale,
                           applied_pulse_arcsec_dec / frameData.pixel_scale);
        m_lastAIPrediction = m_AIGuider->predict(frameData);

        qCDebug(KSTARS_EKOS_GUIDE) <<
                                   QString("[AI GUIDER] Feed-Forward | Inputs: RA Drift=%1px (%2\"), DEC Drift=%3px (%4\"), SNR=%5, Alt=%6°, Az=%7°, q=%8°, PulseRA=%9ms, PulseDEC=%10ms, dt=%11s, t_session=%12s")
                                   .arg(ra_px, 0, 'f', 3).arg(raDrift, 0, 'f', 2)
                                   .arg(dec_px, 0, 'f', 3).arg(decDrift, 0, 'f', 2)
                                   .arg(frameData.snr, 0, 'f', 1).arg(frameData.altitude_deg, 0, 'f', 1)
                                   .arg(frameData.azimuth_deg, 0, 'f', 1).arg(frameData.parallactic_angle_deg, 0, 'f', 1)
                                   .arg(frameData.ra_pulse_ms, 0, 'f', 1).arg(frameData.dec_pulse_ms, 0, 'f', 1)
                                   .arg(frameData.dt, 0, 'f', 3).arg(frameData.t_session_sec, 0, 'f', 1);
        qCDebug(KSTARS_EKOS_GUIDE) << QString("[AI GUIDER] Feed-Forward | Output: valid=%1, conf=%2, PredRA=%3\", PredDEC=%4\"")
                                   .arg(m_lastAIPrediction.valid).arg(m_lastAIPrediction.confidence, 0, 'f', 2)
                                   .arg(m_lastAIPrediction.ra_correction_arcsec, 0, 'f', 3).arg(m_lastAIPrediction.dec_correction_arcsec, 0, 'f', 3);

        if (!m_lastAIPrediction.valid)
        {
            // Still warming up
            if (m_aiState == AIGuideState::ACTIVE || m_aiState == AIGuideState::FALLBACK)
                setAIState(AIGuideState::FALLBACK);  // Was active but lost confidence
            else if (m_aiState != AIGuideState::SHADOW)
                setAIState(AIGuideState::WARMUP);

            m_AILoggedActive = false;
            m_AILoggedFullConfidence = false;

            if (!m_AILoggedWarmup)
            {
                if (m_aiState == AIGuideState::SHADOW)
                    emit newLog("[AI GUIDER] Shadow mode warming up...");
                else
                    emit newLog("[AI GUIDER] AI guider will take over shortly...");
                m_AILoggedWarmup = true;
            }
        }
        else
        {
            // Prediction is valid
            if (m_aiState == AIGuideState::WARMUP || m_aiState == AIGuideState::FALLBACK)
                setAIState(AIGuideState::ACTIVE);
            // SHADOW stays SHADOW — never transitions to ACTIVE automatically

            if (!m_AILoggedActive)
            {
                if (m_aiState == AIGuideState::SHADOW)
                    emit newLog("[AI GUIDER] Shadow mode active — logging predictions alongside standard guiding.");
                else
                    emit newLog("[AI GUIDER] AI guider guiding.");
                m_AILoggedActive = true;
            }
            if (!m_AILoggedFullConfidence && m_lastAIPrediction.confidence >= 0.99)
            {
                if (m_aiState != AIGuideState::SHADOW)
                    emit newLog("[AI GUIDER] Confidence reached 100%. Maximum predictive authority active!");
                m_AILoggedFullConfidence = true;
            }
        }

        // --- AI DEBUG FILE LOGGER ---
        if (!m_AIDebugFile)
        {
            QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                             + "/ai_debug_logs";
            QDir().mkpath(logDir);
            QString logPath = logDir + "/ai_guider_" +
                              QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".csv";
            m_AIDebugFile = new QFile(logPath);
            // Open once and keep the handle open for the whole session; we flush per
            // frame rather than re-opening/closing the file on every guide frame.
            if (m_AIDebugFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            {
                qCInfo(KSTARS_EKOS_GUIDE) << "[AI GUIDER] Debug log:" << logPath;
            }
            else
            {
                qCWarning(KSTARS_EKOS_GUIDE) << "[AI GUIDER] Could not open debug log:" << logPath;
                delete m_AIDebugFile;
                m_AIDebugFile = nullptr;
            }
        }
        if (m_AIDebugFile && m_AIDebugFile->isOpen())
        {
            QTextStream out(m_AIDebugFile);
            if (!m_AIDebugHeaderWritten)
            {
                out << "t_session,dt,altitude_deg,azimuth_deg,parallactic_angle_deg,ra_error_arcsec,uncorrected_ra_delta_px,dec_error_arcsec,uncorrected_dec_delta_px,conf,pred_ra_arcsec,physics_ra_arcsec,mlp_ra_arcsec,pred_dec_arcsec,physics_dec_arcsec,mlp_dec_arcsec,ai_state,pe_statestring\n";
                m_AIDebugHeaderWritten = true;
            }
            out << frameData.t_session_sec << ","
                << frameData.dt << ","
                << frameData.altitude_deg << ","
                << frameData.azimuth_deg << ","
                << frameData.parallactic_angle_deg << ","
                << raDrift << ","
                << uncorrected_drift_ra_px << ","
                << decDrift << ","
                << uncorrected_drift_dec_px << ","
                << m_lastAIPrediction.confidence << ","
                << m_lastAIPrediction.ra_correction_arcsec << ","
                << m_lastAIPrediction.physics_ra_arcsec << ","
                << m_lastAIPrediction.mlp_ra_arcsec << ","
                << m_lastAIPrediction.dec_correction_arcsec << ","
                << m_lastAIPrediction.physics_dec_arcsec << ","
                << m_lastAIPrediction.mlp_dec_arcsec << ","
                << aiGuideStateString(m_aiState) << ","
                << m_AIGuider->stateString() << "\n";
            out.flush();
        }
    }

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
        // Above computes position - reticle (star drift), but we want the mount drift, so negate.
        calibration.convertToPixels(-raDrift, -decDrift, &data.raDistance, &data.decDistance);

        const double raGuideFactor = out_params.pulse_dir[GUIDE_RA] == NO_DIR ?
                                     0 : (out_params.pulse_dir[GUIDE_RA] == RA_INC_DIR ? -1.0 : 1.0);
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

    // analyze.cpp uses only one RA/DEC-axis (up:+, down:-), hence RA is negated.
    Q_EMIT guideStats(-out_params.delta[GUIDE_RA], out_params.delta[GUIDE_DEC],
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
        // Calculate RMS using PHD2's formula: sqrt((n * sumYSq - sumY * sumY) / (n * n))
        double sumY = 0;
        double sumYSq = 0;

        for (int i = 0; i < count; ++i)
        {
            sumY += drift[k][i];
            sumYSq += drift[k][i] * drift[k][i];
        }

        // Use PHD2's formula for population standard deviation
        double variance = count < 2 ? 0 : (count * sumYSq - sumY * sumY) / (count * count);
        if (variance >= 0.0)
            out_params.sigma[k] = sqrt(variance);
        else
            out_params.sigma[k] = 0.0;
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
