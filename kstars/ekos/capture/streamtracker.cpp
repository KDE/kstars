/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "streamtracker.h"

#include "Options.h"

#include <KLocalizedString>

#include <ekos_capture_debug.h>

#include <algorithm>
#include <cmath>

namespace Ekos
{

namespace
{
// Calibration keeps pulsing an axis until the target moves this far (pixels)...
constexpr double CALIBRATION_MIN_MOVE = 10;
// ...but gives up if it moved less than this after the longest allowed total pulse
constexpr double CALIBRATION_NO_MOVE = 3;
constexpr int CALIBRATION_MAX_STEPS = 10;
// Reports in a row without a valid target before the target is declared lost
constexpr int LOST_REPORTS = 5;
// Pulses shorter than this are not worth sending (ms)
constexpr int MIN_PULSE = 10;

double length(const QPointF &point)
{
    return std::hypot(point.x(), point.y());
}
}

StreamTracker::StreamTracker(QObject *parent) : QObject(parent)
{
    m_Clock.start();
}

void StreamTracker::setDevices(ISD::Camera *camera, ISD::Mount *mount, ISD::Guider *guider)
{
    if (camera != m_Camera || guider != m_Guider)
    {
        stop();
        clearCalibration();
    }

    if (m_Camera && m_Camera != camera)
        m_Camera->disconnect(this);
    if (m_Mount && m_Mount != mount)
        m_Mount->disconnect(this);

    m_Guider = guider;

    if (mount && mount != m_Mount)
    {
        connect(mount, &ISD::Mount::pierSideChanged, this, [this](ISD::Mount::PierSide side)
        {
            if (m_Calibrated && side != m_CalibrationPierSide)
            {
                Q_EMIT newLog(i18n("Pier side changed, stream tracking calibration is cleared."));
                clearCalibration();
                if (m_State == TRACKER_TRACKING || m_State == TRACKER_LOST)
                    setState(TRACKER_WAITING);
            }
        });
    }
    m_Mount = mount;

    if (camera && camera != m_Camera)
    {
        connect(camera, &ISD::Camera::propertyDefined, this, &StreamTracker::processProperty);
        connect(camera, &ISD::Camera::propertyUpdated, this, &StreamTracker::processProperty);
        connect(camera, &ISD::Camera::propertyDeleted, this, [this](INDI::Property prop)
        {
            if (prop.isNameMatch("STREAM_TRACKING"))
                stop();
        });
    }
    m_Camera = camera;

    // Tracking may already be on, e.g. when switching optical trains
    if (m_Camera)
    {
        auto tracking = m_Camera->getSwitch("STREAM_TRACKING");
        if (tracking)
            processProperty(tracking);
    }
}

void StreamTracker::clearCalibration()
{
    m_Calibrated = false;
    m_RAVector = m_DECVector = QPointF();
}

void StreamTracker::processProperty(INDI::Property prop)
{
    if (prop.isNameMatch("STREAM_TRACKING"))
    {
        INDI::PropertySwitch tracking(prop);
        auto on = tracking.findWidgetByName("TRACKING_ON");
        if (on && on->getState() == ISS_ON)
            start();
        else
            stop();
    }
    else if (prop.isNameMatch("STREAM_TRACKING_OFFSET") && m_State != TRACKER_IDLE)
        processReport(INDI::PropertyNumber(prop));
}

void StreamTracker::start()
{
    if (m_State != TRACKER_IDLE)
        return;

    if (!Options::streamTrackingCorrectMount())
    {
        Q_EMIT newLog(i18n("Stream tracking is on in the camera, but mount correction is disabled in the options."));
        return;
    }

    if (!m_Guider)
    {
        Q_EMIT newLog(i18n("Stream tracking cannot correct the mount: the optical train has no mount or guider "
                           "that accepts guide pulses."));
        setState(TRACKER_ERROR);
        return;
    }

    if (m_Calibrated && currentBinning() != m_CalibrationBinning)
    {
        Q_EMIT newLog(i18n("Camera binning changed, stream tracking will calibrate again."));
        clearCalibration();
    }

    // Only reports published from now on count
    m_LastSequence = -1;
    if (m_Camera)
    {
        auto offset = m_Camera->getNumber("STREAM_TRACKING_OFFSET");
        auto sequence = offset ? offset.findWidgetByName("SEQUENCE") : nullptr;
        if (sequence)
            m_LastSequence = sequence->getValue();
    }
    m_LostReports = 0;
    m_GateUntil = 0;
    m_SkipReports = 0;

    Q_EMIT newLog(m_Calibrated ? i18n("Stream tracking started.") :
                  i18n("Stream tracking started. Calibrating once the target is locked..."));
    setState(TRACKER_WAITING);
}

void StreamTracker::stop()
{
    if (m_State == TRACKER_IDLE)
        return;

    if (m_State == TRACKER_CALIBRATING)
        Q_EMIT newLog(i18n("Stream tracking stopped during calibration."));
    else
        Q_EMIT newLog(i18n("Stream tracking stopped."));
    setState(TRACKER_IDLE);
}

QString StreamTracker::stateName(TrackerState state)
{
    switch (state)
    {
        case TRACKER_IDLE:
            return QStringLiteral("Idle");
        case TRACKER_WAITING:
            return QStringLiteral("Waiting");
        case TRACKER_CALIBRATING:
            return QStringLiteral("Calibrating");
        case TRACKER_TRACKING:
            return QStringLiteral("Tracking");
        case TRACKER_LOST:
            return QStringLiteral("Lost");
        case TRACKER_PAUSED:
            return QStringLiteral("Paused");
        case TRACKER_ERROR:
            return QStringLiteral("Error");
    }
    return QString();
}

void StreamTracker::setState(TrackerState state)
{
    if (state == m_State)
        return;

    m_State = state;
    Q_EMIT newState(state);
}

void StreamTracker::processReport(INDI::PropertyNumber offset)
{
    auto sequence = offset.findWidgetByName("SEQUENCE");
    if (sequence == nullptr || sequence->getValue() == m_LastSequence)
        return;
    m_LastSequence = sequence->getValue();

    if (m_State == TRACKER_ERROR)
        return;

    if (m_IsGuidingActive && m_IsGuidingActive())
    {
        if (m_State != TRACKER_PAUSED)
        {
            Q_EMIT newLog(i18n("Ekos guiding is active, stream tracking is paused."));
            setState(TRACKER_PAUSED);
        }
        return;
    }
    if (m_State == TRACKER_PAUSED)
        setState(TRACKER_WAITING);

    const double quality = offset.findWidgetByName("QUALITY")->getValue();
    if (offset.getState() != IPS_OK || quality < Options::streamTrackingMinQuality())
    {
        // Calibration just waits for the target; tracking reports it lost after a while
        if (m_State == TRACKER_TRACKING && ++m_LostReports == LOST_REPORTS)
        {
            Q_EMIT newLog(i18n("Stream tracking lost the target (quality %1). Waiting for it to return...",
                               QString::number(quality, 'f', 2)));
            setState(TRACKER_LOST);
        }
        return;
    }

    m_LostReports = 0;
    if (m_State == TRACKER_LOST)
    {
        Q_EMIT newLog(i18n("Stream tracking found the target again."));
        setState(TRACKER_TRACKING);
    }

    if (!reportUsable())
        return;

    const QPointF position(offset.findWidgetByName("CENTROID_X")->getValue(), offset.findWidgetByName("CENTROID_Y")->getValue());
    const QPointF error(offset.findWidgetByName("OFFSET_X")->getValue(), offset.findWidgetByName("OFFSET_Y")->getValue());

    if (m_State == TRACKER_WAITING)
    {
        if (m_Calibrated)
            setState(TRACKER_TRACKING);
        else
        {
            m_Stage = CAL_BASELINE;
            setState(TRACKER_CALIBRATING);
        }
    }

    if (m_State == TRACKER_CALIBRATING)
        processCalibration(position);
    else if (m_State == TRACKER_TRACKING)
        processTracking(error);
}

void StreamTracker::processCalibration(const QPointF &position)
{
    const int step = Options::streamTrackingCalibrationPulse();

    switch (m_Stage)
    {
        case CAL_BASELINE:
            m_Baseline = position;
            m_StagePulseTotal = step;
            m_Stage = CAL_RA_OUT;
            Q_EMIT newLog(i18n("Stream tracking calibration: moving West..."));
            pulse(step, 0);
            break;

        case CAL_RA_OUT:
        case CAL_DEC_OUT:
        {
            const bool ra = (m_Stage == CAL_RA_OUT);
            const QPointF moved = position - m_Baseline;

            if (length(moved) < CALIBRATION_MIN_MOVE && m_StagePulseTotal < step * CALIBRATION_MAX_STEPS)
            {
                m_StagePulseTotal += step;
                ra ? pulse(step, 0) : pulse(0, step);
                break;
            }

            if (length(moved) < CALIBRATION_NO_MOVE)
            {
                failCalibration(ra ? i18n("the target did not move in RA after %1 ms of West pulses.", m_StagePulseTotal) :
                                i18n("the target did not move in DEC after %1 ms of North pulses.", m_StagePulseTotal));
                break;
            }

            if (ra)
            {
                m_RAVector = moved / m_StagePulseTotal;
                m_RATotal = m_StagePulseTotal;
                m_Stage = CAL_RA_BACK;
                pulse(-m_RATotal, 0);
            }
            else
            {
                m_DECVector = moved / m_StagePulseTotal;
                m_DECTotal = m_StagePulseTotal;
                m_Stage = CAL_DEC_BACK;
                pulse(0, -m_DECTotal);
            }
            break;
        }

        case CAL_RA_BACK:
            m_Baseline = position;
            m_StagePulseTotal = step;
            m_Stage = CAL_DEC_OUT;
            Q_EMIT newLog(i18n("Stream tracking calibration: moving North..."));
            pulse(0, step);
            break;

        case CAL_DEC_BACK:
            if (finishCalibration())
                setState(TRACKER_TRACKING);
            break;
    }
}

bool StreamTracker::finishCalibration()
{
    const double determinant = m_RAVector.x() * m_DECVector.y() - m_RAVector.y() * m_DECVector.x();
    const double raRate = length(m_RAVector), decRate = length(m_DECVector);

    // sin of the angle between the axes; near 0 means RA and DEC moved the target the same way
    if (std::fabs(determinant) < 0.2 * raRate * decRate)
    {
        failCalibration(i18n("RA and DEC pulses moved the target in the same direction."));
        return false;
    }

    const double angle = std::acos(QPointF::dotProduct(m_RAVector, m_DECVector) / (raRate * decRate)) * 180.0 / M_PI;

    m_Calibrated = true;
    m_CalibrationBinning = currentBinning();
    m_CalibrationPierSide = m_Mount ? m_Mount->pierSide() : ISD::Mount::PIER_UNKNOWN;

    Q_EMIT newLog(i18n("Stream tracking calibration complete: RA %1 px/s, DEC %2 px/s, axes %3° apart. Tracking...",
                       QString::number(raRate * 1000, 'f', 1), QString::number(decRate * 1000, 'f', 1),
                       QString::number(angle, 'f', 0)));
    if (std::fabs(angle - 90) > 30)
        Q_EMIT newLog(i18n("Warning: stream tracking axes are far from perpendicular. Check for backlash or a "
                           "stalled axis if tracking is poor."));
    return true;
}

void StreamTracker::failCalibration(const QString &reason)
{
    Q_EMIT newLog(i18n("Stream tracking calibration failed: %1", reason));
    clearCalibration();
    setState(TRACKER_ERROR);
}

void StreamTracker::processTracking(const QPointF &offset)
{
    if (length(offset) < Options::streamTrackingDeadband())
        return;

    // Solve offset = raMs * RAVector + decMs * DECVector, then correct a fraction of it
    const double determinant = m_RAVector.x() * m_DECVector.y() - m_RAVector.y() * m_DECVector.x();
    const double raMs  = (offset.x() * m_DECVector.y() - offset.y() * m_DECVector.x()) / determinant;
    const double decMs = (m_RAVector.x() * offset.y() - m_RAVector.y() * offset.x()) / determinant;

    const double aggressiveness = Options::streamTrackingAggressiveness();
    const int maxPulse = Options::streamTrackingMaxPulse();
    int raPulse  = static_cast<int>(std::lround(std::clamp(-raMs * aggressiveness, -1.0 * maxPulse, 1.0 * maxPulse)));
    int decPulse = static_cast<int>(std::lround(std::clamp(-decMs * aggressiveness, -1.0 * maxPulse, 1.0 * maxPulse)));
    if (std::abs(raPulse) < MIN_PULSE)
        raPulse = 0;
    if (std::abs(decPulse) < MIN_PULSE)
        decPulse = 0;

    if (raPulse == 0 && decPulse == 0)
        return;

    qCDebug(KSTARS_EKOS_CAPTURE) << "Stream tracking offset" << offset << "RA pulse" << raPulse << "DEC pulse" << decPulse;
    pulse(raPulse, decPulse);
    Q_EMIT newCorrection(offset.x(), offset.y(), raPulse, decPulse);
}

void StreamTracker::pulse(int raMs, int decMs)
{
    if (!m_Guider)
        return;

    if (raMs != 0)
        m_Guider->doPulse(raMs > 0 ? RA_DEC_DIR : RA_INC_DIR, std::abs(raMs));
    if (decMs != 0)
        m_Guider->doPulse(decMs > 0 ? DEC_INC_DIR : DEC_DEC_DIR, std::abs(decMs));

    // Some mounts run the two axes one after the other, so wait for both, then let the mount settle.
    // The first report after that still averages frames from before, so skip it as well.
    m_GateUntil = m_Clock.elapsed() + std::abs(raMs) + std::abs(decMs) +
                  static_cast<qint64>(Options::streamTrackingSettle() * 1000);
    m_SkipReports = 1;
}

bool StreamTracker::reportUsable()
{
    if (m_Clock.elapsed() < m_GateUntil)
        return false;

    if (m_SkipReports > 0)
    {
        m_SkipReports--;
        return false;
    }

    return true;
}

int StreamTracker::currentBinning() const
{
    if (!m_Camera)
        return 1;

    auto binning = m_Camera->getNumber("CCD_BINNING");
    return binning ? static_cast<int>(binning[0].getValue()) : 1;
}

}
