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
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <algorithm>

namespace Ekos
{

AIGuideProtocol::AIGuideProtocol(Guide *guide) : QObject(guide), m_Guide(guide)
{
    connect(&m_ProtocolTimer, &QTimer::timeout, this, &AIGuideProtocol::processProtocol);
    setObjectName("AIGuideProtocol");
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
            equipment["pixel_scale_arcsec_per_px"] = (206.265 * m_Guide->pixelSizeX()) / m_Guide->focalLength();
            if (std::abs(m_Guide->focalLength() - Options::telescopeFocalLength()) < 1.0)
                equipment["guide_optics_type"] = "OAG";
            else
                equipment["guide_optics_type"] = "Guidescope";
        }
    }
    m_SysIdData["equipment"] = equipment;

    QJsonObject fingerprint;
    fingerprint["guide_exposure_s"] = m_Guide ? m_Guide->exposure() : 0.0;
    fingerprint["guide_binning"] = Options::guideBinning();
    fingerprint["ra_proportional_gain"] = Options::rAProportionalGain();
    fingerprint["dec_proportional_gain"] = Options::dECProportionalGain();
    fingerprint["ra_integral_gain"] = Options::rAIntegralGain();
    fingerprint["dec_integral_gain"] = Options::dECIntegralGain();
    fingerprint["ra_min_pulse_arcsec"] = Options::rAMinimumPulseArcSec();
    fingerprint["dec_min_pulse_arcsec"] = Options::dECMinimumPulseArcSec();
    fingerprint["ra_max_pulse_arcsec"] = static_cast<int>(Options::rAMaximumPulseArcSec());
    fingerprint["dec_max_pulse_arcsec"] = static_cast<int>(Options::dECMaximumPulseArcSec());
    fingerprint["ra_hysteresis"] = Options::rAHysteresis();
    fingerprint["dec_hysteresis"] = Options::dECHysteresis();
    fingerprint["ra_pulse_algorithm"] = 0;
    fingerprint["dec_pulse_algorithm"] = 0;
    fingerprint["all_directions_enabled"] = true;
    m_SysIdData["model_fingerprint"] = fingerprint;

    m_SysIdData["sessions"] = QJsonArray();

    enforceSettings();

    QString dirPath = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("ai_training_logs");
    QDir().mkpath(dirPath);
    m_LogFilename = dirPath + "/" + QString("sysid_data_%1.json").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

    if (mountStr == "Worm Gear")
    {
        m_Phases.append({65.0, -45.0, 480, false, false, "", "", 0, 0, 0});
        m_Phases.append({65.0, -45.0, 600, true, false, "", "", 0, 0, 0});
        m_Phases.append({40.0, -45.0, 480, false, false, "", "", 0, 0, 0});
        m_Phases.append({40.0, -45.0, 400, true, false, "", "", 0, 0, 0});
        m_Phases.append({65.0,  45.0, 480, false, false, "", "", 0, 0, 0});
        m_Phases.append({65.0,  45.0, 400, true, false, "", "", 0, 0, 0});
    }
    else if (mountStr == "Harmonic Drive")
    {
        m_Phases.append({65.0, -45.0, 480, false, false, {}, {}, 0, 15, 30});
        m_Phases.append({65.0, -45.0, 120, true, false, {}, {}, 0, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "RA", "EAST",  50, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "RA", "EAST", 100, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "RA", "EAST", 200, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "RA", "WEST",  50, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "RA", "WEST", 100, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "RA", "WEST", 200, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "NORTH",  50, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "NORTH", 100, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "NORTH", 200, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "SOUTH",  50, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "SOUTH", 100, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "SOUTH", 200, 15, 30});
        m_Phases.append({45.0, -45.0, 120, true, false, {}, {}, 0, 15, 30});
        m_Phases.append({45.0, -45.0, 300, false, false, {}, {}, 0, 15, 30});
    }
    else
    {
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
    m_State = STATE_DONE;
    emit protocolLog("Protocol Aborted by User");
    emit protocolStopped();
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

            if (m_Phases.first().pulseResponse)
            {
                m_State = STATE_PULSE_RESPONSE_INIT;
                break;
            }

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
                emit protocolLog("Settling complete. Starting phase data collection...");
                ProtocolPhase phase = m_Phases.first();
                m_CaptureTimer = phase.durationSeconds;
                m_AbortRetries = 0;
                m_FreeDriftOverflow = false;

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
                phaseRecord["duration_s"] = phase.durationSeconds;
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

                phaseRecord["frames"] = m_PhaseData;

                QJsonArray sessions = m_SysIdData["sessions"].toArray();
                sessions.append(phaseRecord);
                m_SysIdData["sessions"] = sessions;

                m_LogFile.setFileName(m_LogFilename);
                if (m_LogFile.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    QJsonDocument doc(m_SysIdData);
                    m_LogFile.write(doc.toJson());
                    m_LogFile.close();
                }

                m_Phases.removeFirst();
                m_State = STATE_PRECHECK;
            }
            else
            {
                emit protocolProgress(m_TotalPhases - m_Phases.size(), m_TotalPhases,
                                      QString("Capturing Data... %1s remaining").arg(m_CaptureTimer));
                m_CaptureTimer--;
            }
            break;
        }

        case STATE_PULSE_RESPONSE_INIT:
        {
            if (!m_Guide)
            {
                m_State = STATE_ERROR;
                break;
            }

            ProtocolPhase phase = m_Phases.first();
            emit protocolLog(QString("Pulse Response: %1 %2 %3ms — preparing...")
                             .arg(phase.pulseAxis, phase.pulseDirection).arg(phase.pulseMagnitudeMs));

            m_Guide->setAIFreeDrift(true);
            m_PulseFrameCount = 0;
            m_PulseResponseData = QJsonArray();

            connect(m_Guide, &Guide::guideStats, this, &AIGuideProtocol::onGuideStats, Qt::UniqueConnection);

            if (m_Guide->status() == GUIDE_IDLE || m_Guide->status() == GUIDE_ABORTED)
                m_Guide->guide();

            m_FrameTimer.start();
            m_PulseSettleTimer = 5;
            m_State = STATE_PULSE_SETTLING;
            break;
        }

        case STATE_PULSE_SENDING:
        {
            if (!m_Guide)
            {
                m_State = STATE_ERROR;
                break;
            }

            ProtocolPhase phase = m_Phases.first();

            m_Guide->setAIFreeDrift(false);
            if (phase.pulseAxis == "RA")
            {
                if (phase.pulseDirection == "EAST")
                    m_Guide->sendSinglePulse(RA_INC_DIR, phase.pulseMagnitudeMs, StartCaptureAfterPulses);
                else
                    m_Guide->sendSinglePulse(RA_DEC_DIR, phase.pulseMagnitudeMs, StartCaptureAfterPulses);
            }
            else
            {
                if (phase.pulseDirection == "NORTH")
                    m_Guide->sendSinglePulse(DEC_INC_DIR, phase.pulseMagnitudeMs, StartCaptureAfterPulses);
                else
                    m_Guide->sendSinglePulse(DEC_DEC_DIR, phase.pulseMagnitudeMs, StartCaptureAfterPulses);
            }
            m_Guide->setAIFreeDrift(true);

            emit protocolLog(QString("Sent %1ms %2 %3 pulse. Recording %4 response frames...")
                             .arg(phase.pulseMagnitudeMs).arg(phase.pulseAxis).arg(phase.pulseDirection)
                             .arg(phase.responseFrames));

            m_PulseFrameCount = 0;
            m_PulseResponseData = QJsonArray();
            m_FrameTimer.start();
            m_PulseWatchdog = phase.responseFrames * 6 + 30;
            m_State = STATE_PULSE_RECORDING;
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

                    pulseSession["response_frames"] = m_PulseResponseData;

                    QJsonArray sessions = m_SysIdData["sessions"].toArray();
                    sessions.append(pulseSession);
                    m_SysIdData["sessions"] = sessions;

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

    if (m_State == STATE_CAPTURING_DATA)
    {
        double dt = m_FrameTimer.isValid() ? (m_FrameTimer.restart() / 1000.0) : 0.0;

        if (m_Phases.first().freeDrift && !m_FreeDriftOverflow)
        {
            constexpr double FREE_DRIFT_LIMIT_ARCSEC = 25.0;
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
        frame["t"] = m_PulseFrameCount * dt;
        frame["ra_raw_px"] = dx;
        frame["dec_raw_px"] = dy;
        frame["snr"] = snr;
        frame["dt"] = dt;
        m_PulseResponseData.append(frame);
        m_PulseFrameCount++;
    }
}

}