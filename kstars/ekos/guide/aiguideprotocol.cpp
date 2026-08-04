/*
    SPDX-FileCopyrightText: 2026 Pavan <pk6122004@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "aiguideprotocol.h"
#include "guide.h"
#include "kstarsdata.h"
#include "kspaths.h"
#include "skycomponents/skymapcomposite.h"
#include "skycomponents/artificialhorizoncomponent.h"
#include "indi/indimount.h"
#include "Options.h"
#include "internalguide/internalguider.h"
#include "internalguide/calibration.h"
#include "internalguide/mount_guider_factory.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QMap>
#include <QSet>
#include <QVector>
#include <algorithm>
#include <cmath>

namespace Ekos
{

// Free drift ends a segment when the star reaches this offset; the guider then re-centers
// and the drift resumes, so the star never wanders further than this.
static constexpr double FREE_DRIFT_LIMIT_ARCSEC = 25.0;
static constexpr double RECENTER_DONE_ARCSEC = 3.0;
static constexpr int RECENTER_TIMEOUT_S = 90;
static constexpr int MAX_RECENTER_ATTEMPTS = 6;
static constexpr int MIN_SEGMENT_FRAMES = 30;

AIGuideProtocol::AIGuideProtocol(Guide *guide) : QObject(guide), m_Guide(guide)
{
    connect(&m_ProtocolTimer, &QTimer::timeout, this, &AIGuideProtocol::processProtocol);
    setObjectName("AIGuideProtocol");
}

QString AIGuideProtocol::detectMountType() const
{
    if (!m_Guide || !m_Guide->mount())
        return "NOT_FOUND";

    return MountGuiderFactory::detectMountType(m_Guide->mount()->getDeviceName());
}

QJsonObject AIGuideProtocol::buildFingerprint() const
{
    QJsonObject fingerprint;
    fingerprint["guide_exposure_s"] = m_Guide ? m_Guide->exposure() : 0.0;
    fingerprint["guide_binning"] = Options::guideBinning();
    fingerprint["ra_proportional_gain"] = Options::rAProportionalGain();
    fingerprint["dec_proportional_gain"] = Options::dECProportionalGain();
    fingerprint["ra_integral_gain"] = Options::rAIntegralGain();
    fingerprint["dec_integral_gain"] = Options::dECIntegralGain();
    fingerprint["ra_min_pulse_arcsec"] = Options::rAMinimumPulseArcSec();
    fingerprint["dec_min_pulse_arcsec"] = Options::dECMinimumPulseArcSec();
    fingerprint["ra_max_pulse_arcsec"] = static_cast<double>(Options::rAMaximumPulseArcSec());
    fingerprint["dec_max_pulse_arcsec"] = static_cast<double>(Options::dECMaximumPulseArcSec());
    fingerprint["ra_hysteresis"] = Options::rAHysteresis();
    fingerprint["dec_hysteresis"] = Options::dECHysteresis();
    fingerprint["ra_pulse_algorithm"] = 0;
    fingerprint["dec_pulse_algorithm"] = 0;
    fingerprint["all_directions_enabled"] = true;
    return fingerprint;
}

// Settings can change mid-protocol; the recorded fingerprint must describe the data
// as collected, so it is rebuilt at every save and divergence is reported once.
void AIGuideProtocol::refreshFingerprint()
{
    const QJsonObject current = buildFingerprint();
    const QJsonObject recorded = m_SysIdData["model_fingerprint"].toObject();

    QStringList changed;
    for (auto it = current.begin(); it != current.end(); ++it)
    {
        const QJsonValue old = recorded[it.key()];
        if (old != it.value())
            changed << QString("%1: %2 -> %3").arg(it.key(), old.toVariant().toString(),
                                                   it.value().toVariant().toString());
    }
    if (!changed.isEmpty())
    {
        if (!m_SettingsChangedWarned)
        {
            m_SettingsChangedWarned = true;
            emit protocolLog(QString("WARNING: guide settings changed during the protocol (%1). "
                                     "Recording the current values — guide with these same settings "
                                     "or the weights will be rejected.").arg(changed.join(", ")));
        }
        m_SysIdData["model_fingerprint"] = current;

        QJsonObject equipment = m_SysIdData["equipment"].toObject();
        equipment["guide_exposure_ms"] = m_Guide ? static_cast<int>(m_Guide->exposure() * 1000.0) : 0;
        if (m_Guide && m_Guide->focalLength() > 0)
        {
            const int binning = std::max(1, Options::guideBinning().left(1).toInt());
            equipment["pixel_scale_arcsec_per_px"] = (206.265 * m_Guide->pixelSizeX() * binning) / m_Guide->focalLength();
        }
        m_SysIdData["equipment"] = equipment;
    }
}

void AIGuideProtocol::enforceSettings()
{
    if (!m_SettingsEnforced)
    {
        m_OrigRAAlgorithm = Options::rAGuidePulseAlgorithm();
        m_OrigDECAlgorithm = Options::dECGuidePulseAlgorithm();
        m_OrigRAEnabled = Options::rAGuideEnabled();
        m_OrigDECEnabled = Options::dECGuideEnabled();
        m_OrigEastEnabled = Options::eastRAGuideEnabled();
        m_OrigWestEnabled = Options::westRAGuideEnabled();
        m_OrigNorthEnabled = Options::northDECGuideEnabled();
        m_OrigSouthEnabled = Options::southDECGuideEnabled();
        m_OrigMaxDeltaRMS = Options::guideMaxDeltaRMS();
        m_SettingsEnforced = true;
    }

    Options::setRAGuidePulseAlgorithm(0);
    Options::setDECGuidePulseAlgorithm(0);
    Options::setRAGuideEnabled(true);
    Options::setDECGuideEnabled(true);
    Options::setEastRAGuideEnabled(true);
    Options::setWestRAGuideEnabled(true);
    Options::setNorthDECGuideEnabled(true);
    Options::setSouthDECGuideEnabled(true);
    Options::setGuideMaxDeltaRMS(100.0);
}

void AIGuideProtocol::restoreSettings()
{
    if (!m_SettingsEnforced)
        return;
    m_SettingsEnforced = false;

    Options::setRAGuidePulseAlgorithm(m_OrigRAAlgorithm);
    Options::setDECGuidePulseAlgorithm(m_OrigDECAlgorithm);
    Options::setRAGuideEnabled(m_OrigRAEnabled);
    Options::setDECGuideEnabled(m_OrigDECEnabled);
    Options::setEastRAGuideEnabled(m_OrigEastEnabled);
    Options::setWestRAGuideEnabled(m_OrigWestEnabled);
    Options::setNorthDECGuideEnabled(m_OrigNorthEnabled);
    Options::setSouthDECGuideEnabled(m_OrigSouthEnabled);
    Options::setGuideMaxDeltaRMS(m_OrigMaxDeltaRMS);
}

void AIGuideProtocol::start(const QString &mountType)
{
    m_State = STATE_IDLE;
    m_Phases.clear();

    emit protocolLog("Starting AI Guiding Assistant Protocol...");

    QString mountStr = mountType;
    QString mountTypeEnum;
    if (mountStr == "Worm Gear") mountTypeEnum = "WORM_GEAR";
    else if (mountStr == "Harmonic Drive") mountTypeEnum = "HARMONIC_DRIVE";
    else mountTypeEnum = "DIRECT_DRIVE";

    // Initialize the root JSON object for SysId Data
    m_SysIdData = QJsonObject();
    m_SysIdData["format_version"] = "1.0";
    QJsonObject equipment;
    equipment["mount_type"] = mountTypeEnum;
    equipment["mount_name"] = "Unknown";
    equipment["camera"] = "Unknown";
    equipment["focal_length_mm"] = 0;
    equipment["pixel_size_um"] = 0.0;
    equipment["pixel_scale_arcsec_per_px"] = 0.0;
    equipment["guide_exposure_ms"] = 0;
    equipment["guide_optics_type"] = "Unknown";

    if (m_Guide)
    {
        equipment["camera"] = m_Guide->camera();
        equipment["focal_length_mm"] = m_Guide->focalLength();
        equipment["pixel_size_um"] = m_Guide->pixelSizeX();
        equipment["guide_exposure_ms"] = static_cast<int>(m_Guide->exposure() * 1000.0);
        if (m_Guide->mount())
            equipment["mount_name"] = m_Guide->mount()->getDeviceName();
        if (m_Guide->focalLength() > 0)
        {
            const int binning = std::max(1, Options::guideBinning().left(1).toInt());
            equipment["pixel_scale_arcsec_per_px"] = (206.265 * m_Guide->pixelSizeX() * binning) / m_Guide->focalLength();
            equipment["pixel_scale_includes_binning"] = true;
            if (std::abs(m_Guide->focalLength() - Options::telescopeFocalLength()) < 1.0)
                equipment["guide_optics_type"] = "OAG";
            else
                equipment["guide_optics_type"] = "Guidescope";
        }
    }
    m_SysIdData["equipment"] = equipment;

    m_SettingsChangedWarned = false;
    m_BestDriftNoiseFrames = 0;
    m_GainLocked = false;
    m_SysIdData["model_fingerprint"] = buildFingerprint();

    m_SysIdData["sessions"] = QJsonArray();

    enforceSettings();

    QString dirPath = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("ai_training_logs");
    QDir().mkpath(dirPath);
    m_LogFilename = dirPath + "/" + QString("sysid_data_%1.json").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

    if (mountStr == "Worm Gear")
    {
        // PID Auto-Tune: step-response system-ID used to recommend a base RA/DEC
        // guiding gain on any mount class (pid_autotune_plan.md §7-8) -- worm-gear
        // backlash on DEC direction reversal should show up directly as dead time (L)
        // in the fitted model. Runs FIRST, at Position 1's sky location, before any
        // other phase: applyPIDAutoTuneGainLock() (called from STATE_PRECHECK once
        // these pulses are exhausted) computes K/L/tau from them and locks
        // Options::rA/dECProportionalGain() (+ integral gain) before the long
        // standard-guiding phase runs under it -- see pid_autotune_plan.md §7 for why
        // this must happen before, not after, the rest of the protocol. On by default.
        if (Options::aIPIDAutoTune())
        {
            for (int rep = 0; rep < 3; rep++)
            {
                m_Phases.append({65.0, -45.0, 0, false, true, "RA", "EAST",   500, 12, 10});
                m_Phases.append({65.0, -45.0, 0, false, true, "RA", "WEST",   500, 12, 10});
                m_Phases.append({65.0, -45.0, 0, false, true, "RA", "EAST",  1000, 12, 10});
                m_Phases.append({65.0, -45.0, 0, false, true, "RA", "WEST",  1000, 12, 10});
                m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "NORTH",  500, 12, 10});
                m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "SOUTH",  500, 12, 10});
                m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "NORTH", 1000, 12, 10});
                m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "SOUTH", 1000, 12, 10});
            }
        }

        m_Phases.append({65.0, -45.0, 480, false, false, "", "", 0, 0, 0});
        m_Phases.append({65.0, -45.0, 600, true, false, "", "", 0, 0, 0});
        m_Phases.append({40.0, -45.0, 480, false, false, "", "", 0, 0, 0});
        m_Phases.append({40.0, -45.0, 400, true, false, "", "", 0, 0, 0});
        m_Phases.append({65.0,  45.0, 480, false, false, "", "", 0, 0, 0});
        m_Phases.append({65.0,  45.0, 400, true, false, "", "", 0, 0, 0});
    }
    else if (mountStr == "Harmonic Drive")
    {
        // PID Auto-Tune: large pulses, alternating direction so drift/PE cancel in
        // pairing. Runs FIRST, before the long standard-guiding phase below --
        // applyPIDAutoTuneGainLock() (called from STATE_PRECHECK once these pulses
        // are exhausted) computes K/L/tau from them and locks
        // Options::rA/dECProportionalGain() (+ integral gain) before anything else
        // runs under it. This matters for two reasons (pid_autotune_plan.md §7):
        // (1) fingerprint locking -- changing the gain mid- or post-protocol would
        // invalidate all the sysid data already collected under the old gain; (2)
        // data quality -- the long PE-detection phase's reconstruction of the
        // "uncorrected" trajectory assumes a well-behaved, non-oscillating
        // correction, so it needs to run under an already-good gain, not whatever
        // was last manually set. The harmonic-drive elastic/spring (kappa/tau) fit
        // that used to also consume this same pulse data is commented out in
        // train_harmonic.py -- it has never resolved above the noise floor on any
        // rig tested so far (pid_autotune_plan.md §9.1); flagged there for future
        // exploration rather than run unconditionally every time. On by default;
        // no new slew (runs at Position 1's sky location).
        if (Options::aIPIDAutoTune())
        {
            for (int rep = 0; rep < 3; rep++)
            {
                m_Phases.append({65.0, -45.0, 0, false, true, "RA", "EAST",   500, 12, 10});
                m_Phases.append({65.0, -45.0, 0, false, true, "RA", "WEST",   500, 12, 10});
                m_Phases.append({65.0, -45.0, 0, false, true, "RA", "EAST",  1000, 12, 10});
                m_Phases.append({65.0, -45.0, 0, false, true, "RA", "WEST",  1000, 12, 10});
                m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "NORTH",  500, 12, 10});
                m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "SOUTH",  500, 12, 10});
                m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "NORTH", 1000, 12, 10});
                m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "SOUTH", 1000, 12, 10});
            }
        }

        // Position 1: 1800s standard guiding (resolves PE to 900s — strain-wave
        // fundamentals live at sidereal/ratio, 288-865s on rigs measured so far) and 480 free drift.
        // Now runs under the gain locked above, not whatever was last manually set.
        m_Phases.append({65.0, -45.0, 1800, false, false, {}, {}, 0, 15, 20});
        m_Phases.append({65.0, -45.0, 480, true,  false, {}, {}, 0, 15, 20});

        // Position 2 east of the meridian: parallactic spread for the DEC refraction fit
        m_Phases.append({45.0, 45.0, 120, true,  false, {}, {}, 0, 15, 20});
        m_Phases.append({45.0, 45.0, 300, false, false, {}, {}, 0, 15, 20});
    }
    else
    {
        // PID Auto-Tune: step-response system-ID used to recommend a base RA/DEC
        // guiding gain on any mount class (pid_autotune_plan.md §7-8) -- direct-drive
        // motors are expected to show a small, near-negligible tau/dead-time; a
        // confidently-small result is itself a useful finding, not just a null one.
        // Runs FIRST, at Position 1's sky location, before any other phase --
        // applyPIDAutoTuneGainLock() (called from STATE_PRECHECK once these pulses
        // are exhausted) computes K/L/tau from them and locks
        // Options::rA/dECProportionalGain() (+ integral gain) before the rest of the
        // protocol runs under it. On by default.
        if (Options::aIPIDAutoTune())
        {
            for (int rep = 0; rep < 3; rep++)
            {
                m_Phases.append({70.0, 0.0, 0, false, true, "RA", "EAST",   500, 12, 10});
                m_Phases.append({70.0, 0.0, 0, false, true, "RA", "WEST",   500, 12, 10});
                m_Phases.append({70.0, 0.0, 0, false, true, "RA", "EAST",  1000, 12, 10});
                m_Phases.append({70.0, 0.0, 0, false, true, "RA", "WEST",  1000, 12, 10});
                m_Phases.append({70.0, 0.0, 0, false, true, "DEC", "NORTH",  500, 12, 10});
                m_Phases.append({70.0, 0.0, 0, false, true, "DEC", "SOUTH",  500, 12, 10});
                m_Phases.append({70.0, 0.0, 0, false, true, "DEC", "NORTH", 1000, 12, 10});
                m_Phases.append({70.0, 0.0, 0, false, true, "DEC", "SOUTH", 1000, 12, 10});
            }
        }

        m_Phases.append({70.0,   0.0, 120, false, false, "", "", 0, 0, 0});
        m_Phases.append({70.0,   0.0, 180, true, false, "", "", 0, 0, 0});
        m_Phases.append({50.0, -60.0, 120, false, false, "", "", 0, 0, 0});
        m_Phases.append({50.0, -60.0, 180, true, false, "", "", 0, 0, 0});
        m_Phases.append({35.0,  60.0, 120, false, false, "", "", 0, 0, 0});
        m_Phases.append({35.0,  60.0, 180, true, false, "", "", 0, 0, 0});
    }

    emit protocolLog(QString("Loaded %1 phases for %2").arg(m_Phases.size()).arg(mountStr));

    m_TotalPhases = m_Phases.size();
    m_State = STATE_PRECHECK;
    m_ProtocolTimer.start(1000);
}

void AIGuideProtocol::requestTraining()
{
    if (m_SysIdData["sessions"].toArray().isEmpty())
    {
        emit protocolLog(i18n("No data to train! Please run the protocol first."));
        emit trainingError(i18n("No data to train"));
        return;
    }

    m_State = STATE_TRAINING;
    emit trainingProgress(i18n("Starting AI training via EkosLive cloud..."));
    emit trainingRequested(m_SysIdData);
}

void AIGuideProtocol::loadAndComplete(const QString &jsonFilePath)
{
    QFile file(jsonFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        emit protocolLog(QString("ERROR: Failed to open training data: %1").arg(jsonFilePath));
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError)
    {
        emit protocolLog(QString("ERROR: JSON parse error: %1").arg(parseError.errorString()));
        return;
    }

    m_SysIdData = doc.object();
    m_State = STATE_DONE;
    emit protocolLog(QString("Loaded pre-recorded sysid data from %1 (%2 sessions). Ready for training.")
                     .arg(jsonFilePath).arg(m_SysIdData["sessions"].toArray().size()));
    emit protocolComplete();
}

void AIGuideProtocol::onTrainingResult(bool success, const QString &message)
{
    if (success)
    {
        m_State = STATE_TRAINING_DONE;
        emit trainingProgress(message);
        emit trainingComplete();
    }
    else
    {
        emit trainingError(message);
    }
}

void AIGuideProtocol::stop()
{
    m_ProtocolTimer.stop();
    if (m_Guide)
    {
        m_Guide->setAIFreeDrift(false);
        m_Guide->abort();
        disconnect(m_Guide, &Guide::guideStats, this, &AIGuideProtocol::onGuideStats);
    }
    restoreSettings();
    m_State = STATE_IDLE;
    emit protocolLog("Protocol Aborted by User");
    emit protocolStopped();
}

// Writes one captured segment as a session and flushes the log. recordedDuration is what
// the segment was meant to run for: the trainer discards drift sessions much shorter than it.
void AIGuideProtocol::flushPhaseSegment(const ProtocolPhase &phase, int recordedDuration)
{
    QJsonObject phaseRecord;
    phaseRecord["session_id"] = QString("phase_alt%1_%2").arg(phase.targetAlt).arg(
                                    QDateTime::currentDateTime().toString("HHmmss"));
    phaseRecord["type"] = phase.freeDrift ? "free_drift" : "standard_guiding";

    double meanAlt = m_TargetAlt;
    if (!m_PhaseData.isEmpty())
    {
        double sumAlt = 0.0;
        for (int i = 0; i < m_PhaseData.size(); ++i)
            sumAlt += m_PhaseData.at(i).toObject()["altitude_deg"].toDouble();
        meanAlt = sumAlt / m_PhaseData.size();
    }
    phaseRecord["altitude_deg"] = meanAlt;
    phaseRecord["azimuth_deg"] = m_TargetAz;
    if (m_Guide && m_Guide->mount())
    {
        auto pierSide = m_Guide->mount()->pierSide();
        phaseRecord["pier_side"] = (pierSide == ISD::Mount::PIER_EAST) ? "EAST" : "WEST";
    }
    phaseRecord["duration_s"] = recordedDuration;
    phaseRecord["aggressiveness_ra"] = phase.freeDrift ? 0.0 : Options::rAProportionalGain();
    phaseRecord["aggressiveness_dec"] = phase.freeDrift ? 0.0 : Options::dECProportionalGain();
    phaseRecord["min_pulse_ra_arcsec"] = phase.freeDrift ? 0.0 : Options::rAMinimumPulseArcSec();
    phaseRecord["min_pulse_dec_arcsec"] = phase.freeDrift ? 0.0 : Options::dECMinimumPulseArcSec();
    phaseRecord["max_pulse_ra_arcsec"] = phase.freeDrift ? 0.0 : Options::rAMaximumPulseArcSec();
    phaseRecord["max_pulse_dec_arcsec"] = phase.freeDrift ? 0.0 : Options::dECMaximumPulseArcSec();

    if (m_Guide)
    {
        auto *internalGuider = qobject_cast<Ekos::InternalGuider*>(m_Guide->getGuiderInstance());
        if (internalGuider)
        {
            const auto &cal = internalGuider->getCalibration();
            phaseRecord["ra_ms_per_arcsec"] = cal.raPulseMillisecondsPerArcsecond();
            phaseRecord["dec_ms_per_arcsec"] = cal.decPulseMillisecondsPerArcsecond();
        }
    }

    if (m_NoiseStats.count() > 30)
    {
        const double hf = m_NoiseStats.sigma();
        phaseRecord["hf_motion_arcsec"] = hf;
        // The clean value is the LONGEST free drift; guided phases only serve
        // as a fallback when no drift completed, and are never shown.
        if (phase.freeDrift && m_NoiseStats.count() > m_BestDriftNoiseFrames)
        {
            m_BestDriftNoiseFrames = m_NoiseStats.count();
            m_SysIdData["noise_floor_arcsec"] = hf;
            emit protocolLog(QString("Phase noise floor: %1\" HF star motion "
                                     "(unguided — clean measurement).").arg(hf, 0, 'f', 2));
        }
        else if (m_BestDriftNoiseFrames == 0 && !m_SysIdData.contains("noise_floor_arcsec"))
            m_SysIdData["noise_floor_arcsec"] = hf;
    }

    phaseRecord["frames"] = m_PhaseData;

    QJsonArray sessions = m_SysIdData["sessions"].toArray();
    sessions.append(phaseRecord);
    m_SysIdData["sessions"] = sessions;

    refreshFingerprint();
    m_LogFile.setFileName(m_LogFilename);
    if (m_LogFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QJsonDocument doc(m_SysIdData);
        m_LogFile.write(doc.toJson());
        m_LogFile.close();
    }

}

void AIGuideProtocol::processProtocol()
{
    switch (m_State)
    {
        case STATE_IDLE:
        case STATE_DONE:
            m_ProtocolTimer.stop();
            break;

        case STATE_ERROR:
            m_ProtocolTimer.stop();
            // Same cleanup as stop(): without it a free-drift flag or enforced settings
            // left behind would silently suppress all pulses in later guiding sessions.
            if (m_Guide)
            {
                m_Guide->setAIFreeDrift(false);
                disconnect(m_Guide, &Guide::guideStats, this, &AIGuideProtocol::onGuideStats);
            }
            restoreSettings();
            emit protocolStopped();
            break;

        case STATE_PRECHECK:
        {
            if (!m_Guide || !m_Guide->mount() || !m_Guide->mount()->isConnected())
            {
                emit protocolLog("ERROR: Mount is not connected!");
                m_State = STATE_ERROR;
                break;
            }
            if (m_Guide->mount()->status() == ISD::Mount::MOUNT_PARKED)
            {
                emit protocolLog("Mount is parked. Unparking before scan...");
                m_Guide->mount()->unpark();
                break;
            }
            if (m_Guide->mount()->status() != ISD::Mount::MOUNT_TRACKING && m_Guide->mount()->status() != ISD::Mount::MOUNT_IDLE)
                break;

            if (m_Phases.isEmpty())
            {
                emit protocolLog("All phases complete!");
                restoreSettings();
                m_State = STATE_DONE;
                emit protocolComplete();
                break;
            }

            // First non-pulse-response phase reached: the PID Auto-Tune pulses (if any
            // were collected -- see the mount-type branches in start()) are all in by
            // now. Lock the base gain from them, once, before anything else runs --
            // pid_autotune_plan.md §7. A no-op (leaves the current gain untouched) if
            // the option was off or the data wasn't usable.
            if (!m_GainLocked && !m_Phases.first().pulseResponse)
                applyPIDAutoTuneGainLock();

            // Pulse-response phases go through the same horizon-scan/slew/settle path as
            // every other phase (see STATE_HORIZON_SCAN, STATE_SETTLING below) -- they are
            // NOT special-cased into skipping the slew. That used to be safe when the pulse
            // test ran after Position 1's standard-guiding/free-drift phases had already
            // slewed there; now that it runs first (pid_autotune_plan.md §7), skipping the
            // slew would fire calibration pulses wherever the mount happened to be at
            // startup (parked / pointed at the pole), which is exactly wrong for this test.
            m_State = STATE_HORIZON_SCAN;
            break;
        }

        case STATE_HORIZON_SCAN:
        {
            double targetAlt = m_Phases.first().targetAlt;
            emit protocolLog(QString("Phase started: Scanning Artificial Horizon for safe slew to Alt %1°...").arg(targetAlt));

            double meridianAz = KStarsData::Instance()->geo()->lat()->Degrees() > 0 ? 180.0 : 0.0;
            double targetAzOffset = m_Phases.first().azOffset;
            double targetAz = fmod(meridianAz + targetAzOffset + 360.0, 360.0);

            auto* horizon = &KStarsData::Instance()->skyComposite()->artificialHorizon()->getHorizon();
            double clearAz = 0;
            double clearAlt = targetAlt;
            bool foundClear = false;

            for (double altOffset = 0; altOffset <= 10.0; altOffset += 5.0)
            {
                for (double azOffset = 0; azOffset <= 30.0; azOffset += 5.0)
                {
                    QString reason;
                    if (horizon->isAltitudeOK(targetAz + azOffset, targetAlt + altOffset, &reason))
                    {
                        clearAz = targetAz + azOffset;
                        clearAlt = targetAlt + altOffset;
                        foundClear = true;
                        break;
                    }
                    if (horizon->isAltitudeOK(targetAz - azOffset, targetAlt + altOffset, &reason))
                    {
                        clearAz = targetAz - azOffset;
                        clearAlt = targetAlt + altOffset;
                        foundClear = true;
                        break;
                    }
                }
                if (foundClear) break;
            }

            if (!foundClear)
            {
                emit protocolLog("ERROR: Entire meridian is blocked by Artificial Horizon! Cannot proceed.");
                m_State = STATE_ERROR;
            }
            else
            {
                if (std::abs(m_TargetAz - clearAz) < 0.1 && std::abs(m_TargetAlt - clearAlt) < 0.1)
                {
                    emit protocolLog("Continuing data collection at current position...");
                    m_State = STATE_SETTLING;
                    m_SettlingTimer = 2;
                }
                else
                {
                    m_TargetAz = clearAz;
                    m_TargetAlt = clearAlt;
                    emit protocolLog(QString("Found safe sky patch: Az %1°, Alt %2°").arg(m_TargetAz).arg(m_TargetAlt));

                    if (!m_Guide || !m_Guide->mount())
                    {
                        emit protocolLog("ERROR: Mount unavailable, cannot slew.");
                        m_State = STATE_ERROR;
                        break;
                    }

                    SkyPoint targetPoint;
                    targetPoint.setAz(m_TargetAz);
                    targetPoint.setAlt(m_TargetAlt);
                    targetPoint.HorizontalToEquatorialNow();

                    emit protocolLog("Issuing Slew command to mount...");
                    m_Guide->mount()->Slew(&targetPoint);
                    m_State = STATE_SLEWING;
                }
            }
            break;
        }

        case STATE_SLEWING:
        {
            if (!m_Guide || !m_Guide->mount())
            {
                emit protocolLog("ERROR: Mount became unavailable during slew!");
                m_State = STATE_ERROR;
                break;
            }
            if (m_Guide->mount()->status() == ISD::Mount::MOUNT_TRACKING)
            {
                emit protocolLog("Slew complete! Mount is tracking.");
                m_State = STATE_SETTLING;
                m_SettlingTimer = 10;
            }
            else if (m_Guide->mount()->status() == ISD::Mount::MOUNT_ERROR)
            {
                emit protocolLog("ERROR: Mount failed to slew!");
                m_State = STATE_ERROR;
            }
            break;
        }

        case STATE_SETTLING:
            if (m_SettlingTimer > 0)
            {
                emit protocolProgress(m_TotalPhases - m_Phases.size(), m_TotalPhases,
                                      QString("Settling... %1s").arg(m_SettlingTimer));
                m_SettlingTimer--;
            }
            else
            {
                ProtocolPhase phase = m_Phases.first();

                if (phase.pulseResponse)
                {
                    emit protocolLog("Settling complete. Starting PID Auto-Tune pulse-response test...");
                    m_State = STATE_PULSE_RESPONSE_INIT;
                    break;
                }

                emit protocolLog("Settling complete. Starting phase data collection...");
                m_CaptureTimer = phase.durationSeconds;
                m_AbortRetries = 0;
                m_FreeDriftOverflow = false;
                m_SegmentSeconds = 0;
                m_PhaseAborted = false;
                m_RecenterAttempts = 0;

                m_NoiseHPF.configure(1.0, m_Guide ? m_Guide->exposure() : 1.0);
                m_NoiseStats.reset();
                m_NoiseFrameCount = 0;

                m_Guide->setAIFreeDrift(phase.freeDrift);
                m_PhaseData = QJsonArray();

                connect(m_Guide, &Guide::guideStats, this, &AIGuideProtocol::onGuideStats, Qt::UniqueConnection);
                m_FrameTimer.start();

                if (m_Guide->status() == GUIDE_IDLE || m_Guide->status() == GUIDE_ABORTED)
                    m_Guide->guide();

                m_State = STATE_CAPTURING_DATA;
            }
            break;

        case STATE_CAPTURING_DATA:
        {
            // Recovery from unexpected abort
            if (!m_Guide || m_Guide->status() == GUIDE_ABORTED)
            {
                constexpr int MAX_RETRIES = 3;
                if (m_AbortRetries < MAX_RETRIES)
                {
                    m_AbortRetries++;
                    emit protocolLog(QString("Guider aborted (retry %1/%2). Attempting recovery...")
                                     .arg(m_AbortRetries).arg(MAX_RETRIES));
                    if (m_Guide)
                    {
                        m_Guide->setAIFreeDrift(m_Phases.first().freeDrift);
                        m_Guide->guide();
                        m_FrameTimer.start();
                    }
                    break;
                }
                emit protocolLog(QString("Phase ended early (guide star lost after %1 retries). Saving %2 frames.")
                                 .arg(MAX_RETRIES).arg(m_PhaseData.size()));
                m_PhaseAborted = true;
                if (m_Guide)
                {
                    m_Guide->setAIFreeDrift(false);
                    disconnect(m_Guide, &Guide::guideStats, this, &AIGuideProtocol::onGuideStats);
                }
                m_CaptureTimer = 0;
            }

            if (m_Guide && m_Guide->status() != GUIDE_GUIDING && m_Guide->status() != GUIDE_ABORTED)
            {
                emit protocolProgress(m_TotalPhases - m_Phases.size(), m_TotalPhases,
                                      "Waiting for Guider to calibrate/start...");
                m_FrameTimer.start();
                break;
            }

            const bool phaseTimedOut = (m_CaptureTimer <= 0);
            const bool freeDriftOverflowed = m_FreeDriftOverflow;

            // The star reached the safety limit but the phase still has time: bank this
            // segment, let the guider pull the star back, then resume the drift.
            if (freeDriftOverflowed && !phaseTimedOut && m_Phases.first().freeDrift
                    && m_RecenterAttempts < MAX_RECENTER_ATTEMPTS)
            {
                m_RecenterAttempts++;
                emit protocolLog(QString("Free drift reached the %1\" limit after %2s. Re-centering the star "
                                         "to continue (%3s of drift remaining).")
                                 .arg(FREE_DRIFT_LIMIT_ARCSEC, 0, 'f', 0).arg(m_SegmentSeconds).arg(m_CaptureTimer));
                // Too few frames to fit anything — drop rather than record a junk session
                if (m_PhaseData.size() >= MIN_SEGMENT_FRAMES)
                    flushPhaseSegment(m_Phases.first(), m_SegmentSeconds);
                else
                    emit protocolLog(QString("Segment too short (%1 frames) — discarded.").arg(m_PhaseData.size()));
                if (m_Guide)
                    m_Guide->setAIFreeDrift(false);   // pulses resume: the loop re-centers
                m_RecenterTimer = RECENTER_TIMEOUT_S;
                m_State = STATE_DRIFT_RECENTER;
                break;
            }

            if (phaseTimedOut || freeDriftOverflowed)
            {
                if (freeDriftOverflowed)
                    emit protocolLog(QString("Free drift ended early to protect guide star (%1 frames saved).")
                                     .arg(m_PhaseData.size()));
                else
                    emit protocolLog("Phase complete. Stopping capture...");

                if (m_Guide)
                {
                    m_Guide->abort();
                    m_Guide->setAIFreeDrift(false);
                    disconnect(m_Guide, &Guide::guideStats, this, &AIGuideProtocol::onGuideStats);
                }

                ProtocolPhase phase = m_Phases.first();
                // Segments cut short by star loss keep the full requested duration so the
                // trainer's truncation guard can still discard them.
                const int recordedDuration = (phase.freeDrift && !m_PhaseAborted)
                                             ? m_SegmentSeconds : phase.durationSeconds;
                flushPhaseSegment(phase, recordedDuration);

                m_Phases.removeFirst();
                m_State = STATE_PRECHECK;
            }
            else
            {
                emit protocolProgress(m_TotalPhases - m_Phases.size(), m_TotalPhases,
                                      QString("Capturing Data... %1s remaining").arg(m_CaptureTimer));
                m_CaptureTimer--;
                m_SegmentSeconds++;
            }
            break;
        }

        case STATE_DRIFT_RECENTER:
        {
            const bool centered = std::abs(m_LastRAErrArcsec) < RECENTER_DONE_ARCSEC
                                  && std::abs(m_LastDECErrArcsec) < RECENTER_DONE_ARCSEC;
            const bool starLost = (!m_Guide || m_Guide->status() == GUIDE_ABORTED);

            if (starLost)
            {
                emit protocolLog("Lost the star while re-centering. Ending the drift phase.");
                if (m_Guide)
                {
                    m_Guide->abort();
                    m_Guide->setAIFreeDrift(false);
                    disconnect(m_Guide, &Guide::guideStats, this, &AIGuideProtocol::onGuideStats);
                }
                m_PhaseData = QJsonArray();
                m_Phases.removeFirst();
                m_State = STATE_PRECHECK;
                break;
            }

            const bool insideFence = std::abs(m_LastRAErrArcsec) < FREE_DRIFT_LIMIT_ARCSEC
                                     && std::abs(m_LastDECErrArcsec) < FREE_DRIFT_LIMIT_ARCSEC;

            // Timed out and still at the fence: the loop cannot recover the star, so
            // resuming would just trip the limit again. End the phase with what we have.
            if (m_RecenterTimer <= 0 && !insideFence)
            {
                emit protocolLog(QString("Re-centering failed (star still %1\" off). Ending the drift phase.")
                                 .arg(std::max(std::abs(m_LastRAErrArcsec), std::abs(m_LastDECErrArcsec)), 0, 'f', 1));
                if (m_Guide)
                {
                    m_Guide->abort();
                    m_Guide->setAIFreeDrift(false);
                    disconnect(m_Guide, &Guide::guideStats, this, &AIGuideProtocol::onGuideStats);
                }
                m_PhaseData = QJsonArray();
                m_Phases.removeFirst();
                m_State = STATE_PRECHECK;
                break;
            }

            if (centered || m_RecenterTimer <= 0)
            {
                if (!centered)
                    emit protocolLog("Re-centering timed out; resuming drift from the current position.");
                emit protocolLog(QString("Star re-centered. Resuming free drift (%1s remaining).").arg(m_CaptureTimer));

                m_PhaseData = QJsonArray();
                m_NoiseStats.reset();
                m_NoiseFrameCount = 0;
                m_FreeDriftOverflow = false;
                m_SegmentSeconds = 0;
                if (m_Guide)
                    m_Guide->setAIFreeDrift(true);
                m_FrameTimer.start();
                m_State = STATE_CAPTURING_DATA;
                break;
            }

            emit protocolProgress(m_TotalPhases - m_Phases.size(), m_TotalPhases,
                                  QString("Re-centering star... (RA %1\" DEC %2\")")
                                  .arg(m_LastRAErrArcsec, 0, 'f', 1).arg(m_LastDECErrArcsec, 0, 'f', 1));
            m_RecenterTimer--;
            break;
        }

        case STATE_PULSE_RESPONSE_INIT:
        {
            if (!m_Guide)
            {
                m_State = STATE_ERROR;
                break;
            }

            // Pulse-response can now run before any other guiding phase (it's first in
            // the protocol -- pid_autotune_plan.md §7), so unlike before, the guider may
            // not be calibrated/locked yet. RA/DEC error isn't decomposable without a
            // completed calibration, so firing test pulses before GUIDE_GUIDING is
            // reached records nothing but zeros (confirmed in a real run: every
            // ra_raw_px/dec_raw_px came back exactly 0.0). Wait here for calibration to
            // actually finish instead of a fixed short settle.
            if (m_Guide->status() != GUIDE_GUIDING)
            {
                if (m_Guide->status() == GUIDE_IDLE || m_Guide->status() == GUIDE_ABORTED)
                {
                    if (m_AbortRetries >= 3)
                    {
                        emit protocolLog("Pulse Response: guide calibration failed repeatedly -- skipping this pulse.");
                        m_Phases.removeFirst();
                        m_AbortRetries = 0;
                        m_State = STATE_PRECHECK;
                        break;
                    }
                    m_AbortRetries++;
                    emit protocolLog("Pulse Response: starting guide calibration/lock before firing test pulses...");
                    m_Guide->guide();
                    m_PulseWatchdog = 300; // ~5 minutes at 1Hz for calibration to complete
                }
                else if (m_PulseWatchdog > 0)
                {
                    m_PulseWatchdog--;
                    if (m_PulseWatchdog % 30 == 0)
                        emit protocolLog(QString("Pulse Response: still waiting for guide calibration/lock (status=%1)...")
                                         .arg(m_Guide->status()));
                }
                else
                {
                    emit protocolLog("Pulse Response: timed out waiting for guide calibration/lock -- skipping this pulse.");
                    m_Phases.removeFirst();
                    m_State = STATE_PRECHECK;
                }
                break; // stay in STATE_PULSE_RESPONSE_INIT, re-check next tick
            }

            m_AbortRetries = 0;
            ProtocolPhase phase = m_Phases.first();
            emit protocolLog(QString("Pulse Response: %1 %2 %3ms — preparing...")
                             .arg(phase.pulseAxis, phase.pulseDirection).arg(phase.pulseMagnitudeMs));

            m_Guide->setAIFreeDrift(true);
            m_PulseFrameCount = 0;
            m_PulseResponseData = QJsonArray();
            m_PulseBaselineData = QJsonArray();

            connect(m_Guide, &Guide::guideStats, this, &AIGuideProtocol::onGuideStats, Qt::UniqueConnection);

            m_FrameTimer.start();
            m_PulseSettleTimer = 3;
            m_State = STATE_PULSE_SETTLING;
            break;
        }

        case STATE_PULSE_SENDING:
        {
            // Armed and waiting for a clean frame boundary: the pulse itself is fired from
            // onGuideStats() (see firePulseResponsePulse()), the instant the next guide
            // frame completes, not from this 1Hz tick. Firing here unconditionally was the
            // bug — it could land the pulse mid-exposure (the free-drift capture loop keeps
            // running all through STATE_PULSE_SETTLING), which is what made the recorded
            // response shape inconsistent frame-to-frame. This watchdog only guards against
            // no frame ever arriving at all (e.g. star lost) while armed.
            if (!m_Guide || m_Guide->status() == GUIDE_ABORTED)
            {
                emit protocolLog("Pulse response: guider aborted while waiting to fire pulse.");
                m_Phases.removeFirst();
                m_State = STATE_PRECHECK;
                break;
            }
            if (m_PulseWatchdog > 0) m_PulseWatchdog--;
            if (m_PulseWatchdog <= 0)
            {
                emit protocolLog("Pulse response: timed out waiting for a clean frame boundary to fire the pulse (no frames arriving?). Skipping this test.");
                m_Phases.removeFirst();
                m_State = STATE_PRECHECK;
            }
            break;
        }

        case STATE_PULSE_RECORDING:
        {
            ProtocolPhase phase = m_Phases.first();

            const bool guiderAborted = (!m_Guide || m_Guide->status() == GUIDE_ABORTED);
            if (m_PulseWatchdog > 0) m_PulseWatchdog--;
            const bool completed   = (m_PulseFrameCount >= phase.responseFrames);
            const bool interrupted = (!completed && (guiderAborted || m_PulseWatchdog <= 0));

            emit protocolProgress(m_TotalPhases - m_Phases.size(), m_TotalPhases,
                                  QString("Recording pulse response: %1/%2 frames")
                                  .arg(m_PulseFrameCount).arg(phase.responseFrames));

            if (completed || interrupted)
            {
                if (interrupted)
                    emit protocolLog(QString("Pulse response interrupted (%1) after %2/%3 frames.")
                                     .arg(guiderAborted ? "guider aborted — star likely pushed out of frame" : "timed out")
                                     .arg(m_PulseFrameCount).arg(phase.responseFrames));
                else
                    emit protocolLog(QString("Pulse response recorded: %1 frames").arg(m_PulseFrameCount));

                if (m_PulseFrameCount > 0)
                {
                    ProtocolPhase p = m_Phases.first();
                    QJsonObject pulseSession;
                    pulseSession["session_id"] = QString("pulse_response_%1_%2_%3ms_%4")
                                                 .arg(p.pulseAxis.toLower(), p.pulseDirection.toLower())
                                                 .arg(p.pulseMagnitudeMs)
                                                 .arg(QDateTime::currentDateTime().toString("HHmmss"));
                    pulseSession["type"] = "pulse_response";
                    pulseSession["pulse_axis"] = p.pulseAxis;
                    pulseSession["pulse_direction"] = p.pulseDirection;
                    pulseSession["pulse_magnitude_ms"] = p.pulseMagnitudeMs;
                    pulseSession["altitude_deg"] = m_TargetAlt;
                    pulseSession["azimuth_deg"] = m_TargetAz;

                    if (m_Guide && m_Guide->mount())
                    {
                        auto pierSide = m_Guide->mount()->pierSide();
                        pulseSession["pier_side"] = (pierSide == ISD::Mount::PIER_EAST) ? "EAST" : "WEST";
                    }

                    pulseSession["baseline_frames"] = m_PulseBaselineData;
                    pulseSession["pulse_sent_at_ms"] = m_PulseSentAtMs;
                    pulseSession["response_frames"] = m_PulseResponseData;

                    QJsonArray sessions = m_SysIdData["sessions"].toArray();
                    sessions.append(pulseSession);
                    m_SysIdData["sessions"] = sessions;

                    refreshFingerprint();
                    m_LogFile.setFileName(m_LogFilename);
                    if (m_LogFile.open(QIODevice::WriteOnly | QIODevice::Text))
                    {
                        QJsonDocument doc(m_SysIdData);
                        m_LogFile.write(doc.toJson());
                        m_LogFile.close();
                    }
                }

                if (interrupted)
                {
                    if (m_Guide)
                    {
                        m_Guide->setAIFreeDrift(false);
                        disconnect(m_Guide, &Guide::guideStats, this, &AIGuideProtocol::onGuideStats);
                    }
                    m_Phases.removeFirst();
                    m_State = STATE_PRECHECK;
                }
                else
                {
                    m_PulseSettleTimer = phase.settleSeconds;
                    m_State = STATE_PULSE_SETTLING;
                }
            }
            break;
        }

        case STATE_PULSE_SETTLING:
        {
            if (m_PulseSettleTimer > 0)
            {
                emit protocolProgress(m_TotalPhases - m_Phases.size(), m_TotalPhases,
                                      QString("Pulse settling... %1s").arg(m_PulseSettleTimer));
                m_PulseSettleTimer--;
            }
            else
            {
                if (m_PulseFrameCount == 0 && m_PulseResponseData.isEmpty())
                {
                    // Arm the pulse; it fires from onGuideStats() at the next clean frame
                    // boundary, not from this timer tick — see firePulseResponsePulse().
                    // The watchdog here only guards against no frames arriving at all
                    // (e.g. star lost) while armed; it is unrelated to the recording-phase
                    // watchdog below.
                    m_PulseWatchdog = 30;
                    m_State = STATE_PULSE_SENDING;
                }
                else
                {
                    if (m_Guide)
                    {
                        m_Guide->setAIFreeDrift(false);
                        disconnect(m_Guide, &Guide::guideStats, this, &AIGuideProtocol::onGuideStats);
                    }
                    m_Phases.removeFirst();
                    m_State = STATE_PRECHECK;
                }
            }
            break;
        }

        default:
            break;
    }
}

void AIGuideProtocol::onGuideStats(double raErr, double decErr, int raPulse, int decPulse,
                                   double snr, double skyBg, int numStars)
{
    Q_UNUSED(skyBg)
    Q_UNUSED(numStars)

    m_LastRAErrArcsec = raErr;
    m_LastDECErrArcsec = decErr;

    // Armed by STATE_PULSE_SETTLING once the settle countdown elapses (see
    // processProtocol()). This guideStats call marks a clean frame-completion boundary —
    // firing here, rather than from the 1Hz protocol timer, guarantees the pulse is never
    // sent while a capture is mid-exposure, for both streaming and single-capture guiding.
    // This frame itself is the last pre-pulse sample, not a response frame, so return
    // immediately after firing rather than falling through.
    if (m_State == STATE_PULSE_SENDING)
    {
        firePulseResponsePulse();
        return;
    }

    if (m_State == STATE_CAPTURING_DATA)
    {
        double dt = m_FrameTimer.isValid() ? (m_FrameTimer.restart() / 1000.0) : 0.0;

        if (m_Phases.first().freeDrift && !m_FreeDriftOverflow)
        {
            if (std::abs(raErr) > FREE_DRIFT_LIMIT_ARCSEC || std::abs(decErr) > FREE_DRIFT_LIMIT_ARCSEC)
            {
                emit protocolLog(QString("Free drift limit reached (RA=%1\" DEC=%2\"). Ending phase early to protect star.")
                                 .arg(raErr, 0, 'f', 1).arg(decErr, 0, 'f', 1));
                m_FreeDriftOverflow = true;
                return;
            }
        }

        double dx = raErr;
        double dy = decErr;
        if (m_Guide && m_Guide->pixelSizeX() > 0 && m_Guide->focalLength() > 0)
        {
            double binning = std::max(1, Options::guideBinning().left(1).toInt());
            double scale = (206.265 * m_Guide->pixelSizeX() * binning) / m_Guide->focalLength();
            dx = -raErr / scale;
            dy = decErr / scale;
        }

        double alt_deg = 45.0, az_deg = 180.0, dec_deg = 0.0;
        if (m_Guide && m_Guide->mount())
        {
            SkyPoint currentPos = m_Guide->mount()->currentCoordinates();
            az_deg = currentPos.az().Degrees();
            alt_deg = currentPos.alt().Degrees();
            dec_deg = currentPos.dec().Degrees();
        }

        double lat_deg = 45.0;
        if (KStarsData::Instance() && KStarsData::Instance()->geo() && KStarsData::Instance()->geo()->lat())
            lat_deg = KStarsData::Instance()->geo()->lat()->Degrees();

        double parallactic_angle_deg = 0.0;
        double sin_az = std::sin(az_deg * M_PI / 180.0);
        double cos_lat = std::cos(lat_deg * M_PI / 180.0);
        double cos_dec = std::cos(dec_deg * M_PI / 180.0);

        if (std::abs(cos_dec) > 1e-6)
        {
            double sin_q = (sin_az * cos_lat) / cos_dec;
            sin_q = std::clamp(sin_q, -1.0, 1.0);
            parallactic_angle_deg = std::asin(sin_q) * 180.0 / M_PI;
        }

        QJsonObject frame;
        frame["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
        frame["altitude_deg"] = alt_deg;
        frame["azimuth_deg"] = az_deg;
        frame["parallactic_angle_deg"] = parallactic_angle_deg;
        frame["ra_raw_px"] = dx;
        frame["dec_raw_px"] = dy;
        frame["snr"] = snr;
        frame["dt"] = dt;
        frame["error_code"] = (snr == 0.0) ? FRAME_STAR_LOST : FRAME_OK;
        frame["ra_pulse_ms"] = raPulse;
        frame["dec_pulse_ms"] = decPulse;
        m_PhaseData.append(frame);

        // Live noise floor: HPF removes drift/PE, sigma of the remainder is the
        // unguidable high-frequency star motion (seeing + centroid error). Only shown
        // during free drift: the guide loop injects HF motion, so guided values read high.
        if (snr > 0.0)
        {
            const double hf = m_NoiseHPF.addValue(raErr);
            m_NoiseFrameCount++;
            if (m_NoiseFrameCount > 5)
                m_NoiseStats.add(m_NoiseFrameCount, hf);
            const bool freeDrift = !m_Phases.isEmpty() && m_Phases.first().freeDrift;
            if (freeDrift && m_NoiseFrameCount % 60 == 0 && m_NoiseStats.count() > 30)
                emit protocolLog(QString("Measured noise floor (HF star motion): %1\" — "
                                         "guiding cannot correct below this.")
                                 .arg(m_NoiseStats.sigma(), 0, 'f', 2));
        }
    }

    // Pre-pulse settle frames become the fit's baseline
    if (m_State == STATE_PULSE_SETTLING && m_PulseFrameCount == 0 && m_PulseResponseData.isEmpty()
            && m_PulseBaselineData.size() < 6)
    {
        double dx = raErr;
        double dy = decErr;
        if (m_Guide && m_Guide->pixelSizeX() > 0 && m_Guide->focalLength() > 0)
        {
            double binning = std::max(1, Options::guideBinning().left(1).toInt());
            double scale = (206.265 * m_Guide->pixelSizeX() * binning) / m_Guide->focalLength();
            dx = -raErr / scale;
            dy = decErr / scale;
        }

        QJsonObject frame;
        frame["ra_raw_px"] = dx;
        frame["dec_raw_px"] = dy;
        frame["snr"] = snr;
        m_PulseBaselineData.append(frame);
    }

    if (m_State == STATE_PULSE_RECORDING)
    {
        double dt = m_FrameTimer.isValid() ? (m_FrameTimer.restart() / 1000.0) : 0.0;

        double dx = raErr;
        double dy = decErr;
        if (m_Guide && m_Guide->pixelSizeX() > 0 && m_Guide->focalLength() > 0)
        {
            double binning = std::max(1, Options::guideBinning().left(1).toInt());
            double scale = (206.265 * m_Guide->pixelSizeX() * binning) / m_Guide->focalLength();
            dx = -raErr / scale;
            dy = decErr / scale;
        }

        QJsonObject frame;
        // True seconds since the pulse was sent
        frame["t"] = (QDateTime::currentMSecsSinceEpoch() - m_PulseSentAtMs) / 1000.0;
        frame["ra_raw_px"] = dx;
        frame["dec_raw_px"] = dy;
        frame["snr"] = snr;
        frame["dt"] = dt;
        m_PulseResponseData.append(frame);
        m_PulseFrameCount++;
    }
}

void AIGuideProtocol::firePulseResponsePulse()
{
    if (!m_Guide)
    {
        m_State = STATE_ERROR;
        return;
    }

    ProtocolPhase phase = m_Phases.first();

    // Streaming guiding delivers the next frame automatically once the pulse-in-flight
    // guard (m_streamingPulseGuard) expires; single-capture guiding needs an explicit new
    // exposure requested after the pulse (m_PulseTimer -> capture()). Passing the wrong
    // one here previously meant an extra, unneeded capture() request could be issued even
    // in streaming mode — matching the convention InternalGuider already uses for regular
    // guiding pulses (see internalguider.cpp).
    const CaptureAfterPulses captureMode = m_Guide->isStreamingGuide() ? DontCaptureAfterPulses : StartCaptureAfterPulses;

    m_Guide->setAIFreeDrift(false);
    if (phase.pulseAxis == "RA")
    {
        if (phase.pulseDirection == "EAST")
            m_Guide->sendSinglePulse(RA_INC_DIR, phase.pulseMagnitudeMs, captureMode);
        else
            m_Guide->sendSinglePulse(RA_DEC_DIR, phase.pulseMagnitudeMs, captureMode);
    }
    else
    {
        if (phase.pulseDirection == "NORTH")
            m_Guide->sendSinglePulse(DEC_INC_DIR, phase.pulseMagnitudeMs, captureMode);
        else
            m_Guide->sendSinglePulse(DEC_DEC_DIR, phase.pulseMagnitudeMs, captureMode);
    }
    m_Guide->setAIFreeDrift(true);

    emit protocolLog(QString("Sent %1ms %2 %3 pulse. Recording %4 response frames...")
                     .arg(phase.pulseMagnitudeMs).arg(phase.pulseAxis).arg(phase.pulseDirection)
                     .arg(phase.responseFrames));

    m_PulseFrameCount = 0;
    m_PulseResponseData = QJsonArray();
    m_PulseSentAtMs = QDateTime::currentMSecsSinceEpoch();
    m_FrameTimer.start();
    m_PulseWatchdog = phase.responseFrames * 6 + 30;
    m_State = STATE_PULSE_RECORDING;
}

namespace
{

// One pulse-response session's response curve: seconds since the pulse, and
// baseline-subtracted raw pixel displacement along the relevant axis.
struct PulseCurve
{
    QVector<double> t;
    QVector<double> pos;
};

PulseCurve extractPulseCurve(const QJsonObject &session, const QString &axisKey)
{
    PulseCurve curve;
    const QJsonArray baseline = session.value("baseline_frames").toArray();
    double baselineMean = 0.0;
    if (!baseline.isEmpty())
    {
        double sum = 0.0;
        for (const auto &bf : baseline)
            sum += bf.toObject().value(axisKey).toDouble();
        baselineMean = sum / baseline.size();
    }
    const QJsonArray frames = session.value("response_frames").toArray();
    curve.t.reserve(frames.size());
    curve.pos.reserve(frames.size());
    for (const auto &f : frames)
    {
        const QJsonObject fo = f.toObject();
        curve.t.append(fo.value("t").toDouble());
        curve.pos.append(fo.value(axisKey).toDouble() - baselineMean);
    }
    return curve;
}

// Linear interpolation of (t, v) at queryT; clamps to the curve's endpoints outside its range.
double interpAt(const QVector<double> &t, const QVector<double> &v, double queryT)
{
    if (t.isEmpty())
        return 0.0;
    if (queryT <= t.first())
        return v.first();
    if (queryT >= t.last())
        return v.last();
    for (int i = 1; i < t.size(); ++i)
    {
        if (t[i] >= queryT)
        {
            const double frac = (queryT - t[i - 1]) / std::max(t[i] - t[i - 1], 1e-9);
            return v[i - 1] + frac * (v[i] - v[i - 1]);
        }
    }
    return v.last();
}

double medianOf(QVector<double> values)
{
    if (values.isEmpty())
        return 0.0;
    std::sort(values.begin(), values.end());
    const int n = values.size();
    return (n % 2 == 0) ? (values[n / 2 - 1] + values[n / 2]) / 2.0 : values[n / 2];
}

// Below this many usable step-response fits, don't trust the result enough to apply
// it automatically -- offline_trainer/pid_autotune.py uses the same style of gate
// (MIN_FITS_FOR_MEDIUM_CONFIDENCE) for its advisory recommendation; applied here as a
// hard floor since this result is applied live, not just surfaced for review.
constexpr int MIN_FITS_TO_APPLY = 4;
// Same default as offline_trainer/pid_autotune.py's SIMC_LAMBDA_L_FACTOR.
constexpr double SIMC_LAMBDA_L_FACTOR = 3.0;
constexpr double INTEGRAL_GAIN_CONSERVATIVE_FRACTION = 0.25;

} // namespace

// Live C++ port of offline_trainer/pid_autotune.py's per-axis SIMC-style calculation,
// simplified to avoid a nonlinear curve fit: the plateau amplitude is estimated as the
// mean of the last third of a paired pulse's response curve rather than fit, since on
// every rig tested so far the step response is effectively flat well before the
// response-frame window ends and the fitted tau has never resolved below the sampling
// floor anyway (pid_autotune_plan.md §9.1) -- this is a reasonable same-night stand-in,
// not a replacement for the offline trainer's fuller fit.
bool AIGuideProtocol::computeAndApplyAxisGain(const QString &axis, double msPerArcsec)
{
    if (msPerArcsec <= 0.0)
    {
        emit protocolLog(QString("PID Auto-Tune [%1]: no calibrated ms/arcsec on record -- keeping current gain.").arg(axis));
        return false;
    }

    const QString axisKey = (axis == "RA") ? "ra_raw_px" : "dec_raw_px";
    const QString posDir  = (axis == "RA") ? "EAST" : "NORTH";
    const QString negDir  = (axis == "RA") ? "WEST" : "SOUTH";
    const double pixelScale = m_SysIdData.value("equipment").toObject()
                              .value("pixel_scale_arcsec_per_px").toDouble(1.0);

    QMap<double, QList<QJsonObject>> posByMag, negByMag;
    const QJsonArray sessions = m_SysIdData.value("sessions").toArray();
    for (const auto &s : sessions)
    {
        const QJsonObject so = s.toObject();
        if (so.value("type").toString() != "pulse_response" || so.value("pulse_axis").toString() != axis)
            continue;
        const double mag = so.value("pulse_magnitude_ms").toDouble();
        const QString dir = so.value("pulse_direction").toString();
        if (dir == posDir)
            posByMag[mag].append(so);
        else if (dir == negDir)
            negByMag[mag].append(so);
    }

    QVector<double> kSamples, lSamples, tFirstSamples;
    QSet<int> signSet;

    for (auto it = posByMag.constBegin(); it != posByMag.constEnd(); ++it)
    {
        const double mag = it.key();
        const QList<QJsonObject> &posList = it.value();
        const QList<QJsonObject> &negList = negByMag.value(mag);
        const int pairs = std::min(posList.size(), negList.size());
        for (int i = 0; i < pairs; ++i)
        {
            const PulseCurve cp = extractPulseCurve(posList.at(i), axisKey);
            const PulseCurve cn = extractPulseCurve(negList.at(i), axisKey);
            if (cp.t.size() < 5 || cn.t.size() < 5)
                continue;

            QVector<double> tArr, diff;
            for (int j = 0; j < cp.t.size(); ++j)
            {
                if (cp.t[j] < cn.t.first() || cp.t[j] > cn.t.last())
                    continue;
                tArr.append(cp.t[j]);
                diff.append(cp.pos[j] - interpAt(cn.t, cn.pos, cp.t[j]));
            }
            if (tArr.size() < 5)
                continue;

            // Plateau amplitude/noise estimate from the last third of samples.
            const int tailCount = std::max(1, static_cast<int>(tArr.size()) / 3);
            double tailSum = 0.0;
            for (int j = tArr.size() - tailCount; j < tArr.size(); ++j)
                tailSum += diff[j];
            const double pFit = tailSum / tailCount;

            double varSum = 0.0;
            for (int j = tArr.size() - tailCount; j < tArr.size(); ++j)
                varSum += (diff[j] - pFit) * (diff[j] - pFit);
            const double residualStd = std::sqrt(varSum / tailCount);

            if (std::abs(pFit) < 2.0 * std::max(residualStd, 1e-6))
                continue; // noise-dominated, skip -- same gate as pulse_response_fit.py

            double lSample = tArr.last();
            const double threshold = std::max(2.5 * residualStd, 1e-6);
            for (int j = 0; j < tArr.size(); ++j)
            {
                if (std::abs(diff[j]) > threshold)
                {
                    lSample = tArr[j];
                    break;
                }
            }

            kSamples.append(std::abs(pFit) * pixelScale / mag);
            lSamples.append(lSample);
            tFirstSamples.append(tArr.first());
            signSet.insert(pFit > 0 ? 1 : -1);
        }
    }

    if (kSamples.size() < MIN_FITS_TO_APPLY)
    {
        emit protocolLog(QString("PID Auto-Tune [%1]: only %2 usable pulse-response fit(s) (need >= %3) -- keeping current gain.")
                         .arg(axis).arg(kSamples.size()).arg(MIN_FITS_TO_APPLY));
        return false;
    }
    if (signSet.size() > 1)
    {
        emit protocolLog(QString("PID Auto-Tune [%1]: pulse-response signs inconsistent across pulses -- "
                                 "data looks like noise, keeping current gain.").arg(axis));
        return false;
    }

    const double K   = medianOf(kSamples);
    const double L   = medianOf(lSamples);
    const double tau = std::max(medianOf(tFirstSamples), L);

    if (K <= 0.0)
    {
        emit protocolLog(QString("PID Auto-Tune [%1]: computed process gain is zero -- keeping current gain.").arg(axis));
        return false;
    }

    const double lambda = std::max(tau, SIMC_LAMBDA_L_FACTOR * L);
    const double Kc = (1.0 / K) * tau / (lambda + L);
    const double proportionalGain = std::max(0.0, std::min(1.0, Kc / msPerArcsec));
    const double integralGain = std::max(0.0, std::min(1.0, INTEGRAL_GAIN_CONSERVATIVE_FRACTION * proportionalGain));

    const double oldGain = (axis == "RA") ? Options::rAProportionalGain() : Options::dECProportionalGain();
    if (axis == "RA")
    {
        Options::setRAProportionalGain(proportionalGain);
        Options::setRAIntegralGain(integralGain);
    }
    else
    {
        Options::setDECProportionalGain(proportionalGain);
        Options::setDECIntegralGain(integralGain);
    }

    emit protocolLog(QString("PID Auto-Tune [%1]: K=%2\"/ms  L=%3s  tau=%4s  (n=%5 fits) "
                             "-- gain %6 -> %7 (locked for the rest of this session)")
                     .arg(axis).arg(K, 0, 'f', 5).arg(L, 0, 'f', 2).arg(tau, 0, 'f', 2)
                     .arg(kSamples.size()).arg(oldGain, 0, 'f', 3).arg(proportionalGain, 0, 'f', 3));
    return true;
}

void AIGuideProtocol::applyPIDAutoTuneGainLock()
{
    m_GainLocked = true;

    if (!Options::aIPIDAutoTune())
        return;

    auto *internalGuider = m_Guide ? qobject_cast<InternalGuider *>(m_Guide->getGuiderInstance()) : nullptr;
    if (!internalGuider)
    {
        emit protocolLog("PID Auto-Tune: Internal Guider required to read calibration -- skipping gain lock.");
        return;
    }
    const auto &cal = internalGuider->getCalibration();

    const bool raApplied  = computeAndApplyAxisGain("RA",  cal.raPulseMillisecondsPerArcsecond());
    const bool decApplied = computeAndApplyAxisGain("DEC", cal.decPulseMillisecondsPerArcsecond());

    if (raApplied || decApplied)
        refreshFingerprint();
}

}