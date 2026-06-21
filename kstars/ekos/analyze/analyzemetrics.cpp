/*
    SPDX-FileCopyrightText: 2026 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "analyzemetrics.h"

#include <QtGlobal>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace
{

QByteArray escapeLabel(const QString &value)
{
    QByteArray escaped = value.toUtf8();
    escaped.replace("\\", "\\\\");
    escaped.replace("\n", "\\n");
    escaped.replace("\"", "\\\"");
    return escaped;
}

QByteArray number(double value)
{
    return QByteArray::number(value, 'g', 16);
}

void descriptor(QByteArray &output, const QByteArray &name, const QByteArray &type,
                const QByteArray &help, const QByteArray &unit = QByteArray())
{
    output += "# TYPE " + name + ' ' + type + '\n';
    if (!unit.isEmpty())
        output += "# UNIT " + name + ' ' + unit + '\n';
    output += "# HELP " + name + ' ' + help + '\n';
}

void sample(QByteArray &output, const QByteArray &name, double value,
            const QByteArray &labels = QByteArray())
{
    output += name;
    if (!labels.isEmpty())
        output += '{' + labels + '}';
    output += ' ' + number(value) + '\n';
}

void sample(QByteArray &output, const QByteArray &name, quint64 value,
            const QByteArray &labels = QByteArray())
{
    output += name;
    if (!labels.isEmpty())
        output += '{' + labels + '}';
    output += ' ' + QByteArray::number(value) + '\n';
}

void optionalSample(QByteArray &output, const QByteArray &name, const std::optional<double> &value)
{
    if (value && qIsFinite(*value))
        sample(output, name, *value);
}

QByteArray label(const QByteArray &name, const QString &value)
{
    return name + "=\"" + escapeLabel(value) + '"';
}

void stateSamples(QByteArray &output, const QByteArray &name, const QStringList &states,
                  const QString &current)
{
    for (const auto &state : states)
        sample(output, name, state == current ? 1.0 : 0.0, label("state", state));
}

} // namespace

namespace Ekos
{

AnalyzeMetrics::AnalyzeMetrics(const QString &version) : m_version(version)
{
    clear();
}

double AnalyzeMetrics::RollingRms::add(double x, double y)
{
    m_x.push_back(x);
    m_y.push_back(y);
    m_xSum += x;
    m_ySum += y;
    m_xSquares += x * x;
    m_ySquares += y * y;

    if (m_x.size() > WINDOW_SIZE)
    {
        const double oldX = m_x.front();
        const double oldY = m_y.front();
        m_x.pop_front();
        m_y.pop_front();
        m_xSum -= oldX;
        m_ySum -= oldY;
        m_xSquares -= oldX * oldX;
        m_ySquares -= oldY * oldY;
    }

    const auto size = static_cast<double>(m_x.size());
    if (size < 2)
        return 0;
    const double xVariance = (m_xSquares - m_xSum * m_xSum / size) / (size - 1);
    const double yVariance = (m_ySquares - m_ySum * m_ySum / size) / (size - 1);
    return std::sqrt(std::max(0.0, xVariance + yVariance));
}

void AnalyzeMetrics::RollingRms::reset()
{
    m_x.clear();
    m_y.clear();
    m_xSum = m_ySum = m_xSquares = m_ySquares = 0;
}

void AnalyzeMetrics::clear()
{
    m_sessionStartMs.reset();
    reset(0);
    m_sessionStartMs.reset();
}

void AnalyzeMetrics::reset(qint64 startTimeMs)
{
    m_sessionStartMs = startTimeMs;
    m_captureActive = false;
    m_captureStartMs.reset();
    m_captureFilter.clear();
    m_captureExposureSeconds = 0;
    m_captureDurationSeconds.reset();
    m_captureHfr.reset();
    m_captureEccentricity.reset();
    m_captureStars.reset();
    m_captureMedian.reset();
    m_targetDistance.reset();
    m_captureTotals.clear();

    m_autofocusActive = false;
    m_autofocusStartMs.reset();
    m_autofocusDurationSeconds.reset();
    m_focusPosition.reset();
    m_adaptiveAdjustment.reset();
    m_temperature.reset();
    m_focusTotals.clear();
    m_adaptiveFocusTotals.clear();

    m_guideState = QStringLiteral("idle");
    m_guideTransitions.clear();
    m_lastGuideStatsMs.reset();
    m_guideRaError.reset();
    m_guideDecError.reset();
    m_guideDrift.reset();
    m_guideRms.reset();
    m_captureRms.reset();
    m_guideRaPulse.reset();
    m_guideDecPulse.reset();
    m_guideSnr.reset();
    m_guideSkyBackground.reset();
    m_guideStars.reset();
    m_guideRmsFilter.reset();
    m_captureRmsFilter.reset();

    m_alignState = QStringLiteral("idle");
    m_alignCompleted = m_alignFailed = m_alignAborted = 0;
    m_mountState = QStringLiteral("idle");
    m_mountRa.reset();
    m_mountDec.reset();
    m_mountAzimuth.reset();
    m_mountAltitude.reset();
    m_mountHourAngle.reset();
    m_pierSide = QStringLiteral("unknown");
    m_flipState = QStringLiteral("none");
    m_flipCompleted = m_flipFailed = 0;

    m_schedulerActive = false;
    m_schedulerJob.clear();
    m_schedulerStartMs.reset();
    m_schedulerDurationSeconds.reset();
    m_schedulerStarted = m_schedulerEnded = 0;
}

void AnalyzeMetrics::ensureSession(qint64 timeMs)
{
    if (!m_sessionStartMs)
        reset(timeMs);
}

QString AnalyzeMetrics::boundedFilter(const QString &filter)
{
    const QString value = filter.isEmpty() ? QStringLiteral("unknown") : filter.left(64);
    if (m_captureTotals.contains(value) || m_focusTotals.contains(value) || m_adaptiveFocusTotals.contains(value))
        return value;
    QSet<QString> filters;
    for (auto it = m_captureTotals.cbegin(); it != m_captureTotals.cend(); ++it)
        filters.insert(it.key());
    for (auto it = m_focusTotals.cbegin(); it != m_focusTotals.cend(); ++it)
        filters.insert(it.key());
    for (auto it = m_adaptiveFocusTotals.cbegin(); it != m_adaptiveFocusTotals.cend(); ++it)
        filters.insert(it.key());
    return filters.size() < MAX_FILTERS ? value : QStringLiteral("other");
}

QString AnalyzeMetrics::normalizedState(const QString &state)
{
    QString result = state.trimmed().toLower();
    for (auto &character : result)
    {
        if (!character.isLetterOrNumber())
            character = QLatin1Char('_');
    }
    while (result.contains(QStringLiteral("__")))
        result.replace(QStringLiteral("__"), QStringLiteral("_"));
    return result;
}

void AnalyzeMetrics::captureStarting(qint64 timeMs, double exposureSeconds, const QString &filter)
{
    ensureSession(timeMs);
    m_captureActive = true;
    m_captureStartMs = timeMs;
    m_captureFilter = boundedFilter(filter);
    m_captureExposureSeconds = exposureSeconds;
    m_captureRms.reset();
    m_captureRmsFilter.reset();
}

void AnalyzeMetrics::captureComplete(qint64 timeMs, double exposureSeconds, const QString &filter,
                                     double hfr, int stars, double median, double eccentricity)
{
    ensureSession(timeMs);
    if (!m_captureActive)
        return;
    const QString bounded = boundedFilter(filter);
    auto &totals = m_captureTotals[bounded];
    ++totals.completed;
    totals.exposureSeconds += std::max(0.0, exposureSeconds);
    if (m_captureStartMs)
        m_captureDurationSeconds = std::max(0.0, (timeMs - *m_captureStartMs) / 1000.0);
    m_captureFilter = bounded;
    m_captureHfr = hfr;
    m_captureStars = stars;
    m_captureMedian = median;
    m_captureEccentricity = eccentricity;
    m_captureActive = false;
    m_captureStartMs.reset();
}

void AnalyzeMetrics::captureAborted(qint64 timeMs, const QString &filter)
{
    ensureSession(timeMs);
    if (!m_captureActive)
        return;
    const QString bounded = boundedFilter(filter);
    ++m_captureTotals[bounded].aborted;
    if (m_captureStartMs)
        m_captureDurationSeconds = std::max(0.0, (timeMs - *m_captureStartMs) / 1000.0);
    m_captureFilter = bounded;
    m_captureActive = false;
    m_captureStartMs.reset();
}

void AnalyzeMetrics::autofocusStarting(qint64 timeMs, const QString &filter)
{
    Q_UNUSED(filter);
    ensureSession(timeMs);
    m_autofocusActive = true;
    m_autofocusStartMs = timeMs;
}

void AnalyzeMetrics::autofocusComplete(qint64 timeMs, const QString &filter, double position)
{
    ensureSession(timeMs);
    if (!m_autofocusActive)
        return;
    const QString bounded = boundedFilter(filter);
    ++m_focusTotals[bounded].completed;
    if (m_autofocusStartMs)
        m_autofocusDurationSeconds = std::max(0.0, (timeMs - *m_autofocusStartMs) / 1000.0);
    if (qIsFinite(position))
        m_focusPosition = position;
    m_autofocusActive = false;
    m_autofocusStartMs.reset();
}

void AnalyzeMetrics::autofocusAborted(qint64 timeMs, const QString &filter)
{
    ensureSession(timeMs);
    if (!m_autofocusActive)
        return;
    const QString bounded = boundedFilter(filter);
    ++m_focusTotals[bounded].aborted;
    if (m_autofocusStartMs)
        m_autofocusDurationSeconds = std::max(0.0, (timeMs - *m_autofocusStartMs) / 1000.0);
    m_autofocusActive = false;
    m_autofocusStartMs.reset();
}

void AnalyzeMetrics::adaptiveFocusComplete(qint64 timeMs, const QString &filter, int adjustment,
        int position, bool moved)
{
    ensureSession(timeMs);
    const QString bounded = boundedFilter(filter);
    auto &totals = m_adaptiveFocusTotals[bounded];
    if (moved)
        ++totals.first;
    else
        ++totals.second;
    m_adaptiveAdjustment = adjustment;
    m_focusPosition = position;
}

void AnalyzeMetrics::temperature(double value)
{
    if (value > -200 && qIsFinite(value))
        m_temperature = value;
}

void AnalyzeMetrics::guideState(const QString &state)
{
    const QString normalized = normalizedState(state);
    const QStringList states { "idle", "guiding", "calibrating", "suspended", "dithering" };
    if (states.contains(normalized) && normalized != m_guideState)
    {
        m_guideState = normalized;
        ++m_guideTransitions[normalized];
    }
}

void AnalyzeMetrics::guideStats(qint64 timeMs, double raError, double decError, int raPulse,
                                int decPulse, double snr, double skyBackground, int stars)
{
    ensureSession(timeMs);
    if (m_lastGuideStatsMs && timeMs - *m_lastGuideStatsMs > 30000)
    {
        m_guideRmsFilter.reset();
        m_captureRmsFilter.reset();
    }
    m_guideRaError = raError;
    m_guideDecError = decError;
    if (qIsFinite(raError) && qIsFinite(decError))
    {
        m_guideDrift = std::hypot(raError, decError);
        m_guideRms = m_guideRmsFilter.add(raError, decError);
        if (m_captureActive)
            m_captureRms = m_captureRmsFilter.add(raError, decError);
    }
    m_guideRaPulse = raPulse;
    m_guideDecPulse = decPulse;
    m_guideSnr = snr;
    m_guideSkyBackground = skyBackground;
    m_guideStars = stars;
    m_lastGuideStatsMs = timeMs;
}

void AnalyzeMetrics::alignState(const QString &state)
{
    m_alignState = normalizedState(state);
    if (m_alignState == QLatin1String("complete"))
        ++m_alignCompleted;
    else if (m_alignState == QLatin1String("failed"))
        ++m_alignFailed;
    else if (m_alignState == QLatin1String("aborted"))
        ++m_alignAborted;
}

void AnalyzeMetrics::mountState(const QString &state)
{
    m_mountState = normalizedState(state);
}

void AnalyzeMetrics::mountCoordinates(double ra, double dec, double azimuth, double altitude,
                                      int pierSide, double hourAngle)
{
    m_mountRa = ra;
    m_mountDec = dec;
    m_mountAzimuth = azimuth;
    m_mountAltitude = altitude;
    m_mountHourAngle = hourAngle;
    m_pierSide = pierSide == 0 ? QStringLiteral("west_to_east") :
                 pierSide == 1 ? QStringLiteral("east_to_west") : QStringLiteral("unknown");
}

void AnalyzeMetrics::meridianFlipState(const QString &state)
{
    QString normalized = normalizedState(state);
    normalized.remove(QStringLiteral("mount_flip_"));
    m_flipState = normalized;
    if (normalized == QLatin1String("completed"))
        ++m_flipCompleted;
    else if (normalized == QLatin1String("error"))
        ++m_flipFailed;
}

void AnalyzeMetrics::schedulerJobStarted(qint64 timeMs, const QString &jobName)
{
    ensureSession(timeMs);
    m_schedulerActive = true;
    m_schedulerJob = jobName.left(256);
    m_schedulerStartMs = timeMs;
    ++m_schedulerStarted;
}

void AnalyzeMetrics::schedulerJobEnded(qint64 timeMs)
{
    ensureSession(timeMs);
    if (!m_schedulerActive)
        return;
    if (m_schedulerStartMs)
        m_schedulerDurationSeconds = std::max(0.0, (timeMs - *m_schedulerStartMs) / 1000.0);
    m_schedulerActive = false;
    m_schedulerStartMs.reset();
    ++m_schedulerEnded;
}

void AnalyzeMetrics::targetDistance(double arcseconds)
{
    if (qIsFinite(arcseconds))
        m_targetDistance = arcseconds;
}

QByteArray AnalyzeMetrics::render(qint64 nowMs) const
{
    QByteArray output;
    output.reserve(12000);

    descriptor(output, "kstars_info", "gauge", "KStars build information.");
    sample(output, "kstars_info", 1.0, label("version", m_version));

    descriptor(output, "kstars_ekos_session_start_time_seconds", "gauge",
               "Unix timestamp when the current Analyze session started.", "seconds");
    descriptor(output, "kstars_ekos_session_elapsed_seconds", "gauge",
               "Elapsed time in the current Analyze session.", "seconds");
    if (m_sessionStartMs)
    {
        sample(output, "kstars_ekos_session_start_time_seconds", *m_sessionStartMs / 1000.0);
        sample(output, "kstars_ekos_session_elapsed_seconds", std::max(0.0, (nowMs - *m_sessionStartMs) / 1000.0));
    }

    descriptor(output, "kstars_ekos_capture_active", "gauge", "Whether an exposure is active.");
    sample(output, "kstars_ekos_capture_active", m_captureActive ? 1.0 : 0.0);
    descriptor(output, "kstars_ekos_capture_filter_info", "gauge", "Current or most recent capture filter.");
    if (!m_captureFilter.isEmpty())
        sample(output, "kstars_ekos_capture_filter_info", 1.0, label("filter", m_captureFilter));
    descriptor(output, "kstars_ekos_capture_configured_exposure_seconds", "gauge",
               "Configured duration of the current or most recent exposure.", "seconds");
    if (!m_captureFilter.isEmpty())
        sample(output, "kstars_ekos_capture_configured_exposure_seconds", m_captureExposureSeconds);
    descriptor(output, "kstars_ekos_capture_elapsed_seconds", "gauge", "Elapsed time in the active exposure.", "seconds");
    if (m_captureActive && m_captureStartMs)
        sample(output, "kstars_ekos_capture_elapsed_seconds", std::max(0.0, (nowMs - *m_captureStartMs) / 1000.0));
    descriptor(output, "kstars_ekos_capture_last_duration_seconds", "gauge",
               "Wall-clock duration of the most recent exposure attempt.", "seconds");
    optionalSample(output, "kstars_ekos_capture_last_duration_seconds", m_captureDurationSeconds);
    descriptor(output, "kstars_ekos_capture_results", "counter", "Capture attempts by filter and result.");
    descriptor(output, "kstars_ekos_capture_exposure_seconds", "counter",
               "Successfully completed configured exposure time by filter.", "seconds");
    for (auto it = m_captureTotals.cbegin(); it != m_captureTotals.cend(); ++it)
    {
        const auto filterLabel = label("filter", it.key());
        sample(output, "kstars_ekos_capture_results_total", it->completed,
               filterLabel + ",result=\"completed\"");
        sample(output, "kstars_ekos_capture_results_total", it->aborted,
               filterLabel + ",result=\"aborted\"");
        sample(output, "kstars_ekos_capture_exposure_seconds_total", it->exposureSeconds, filterLabel);
    }
    descriptor(output, "kstars_ekos_capture_hfr", "gauge", "HFR reported for the most recent completed exposure.");
    optionalSample(output, "kstars_ekos_capture_hfr", m_captureHfr);
    descriptor(output, "kstars_ekos_capture_eccentricity", "gauge",
               "Eccentricity reported for the most recent completed exposure.");
    optionalSample(output, "kstars_ekos_capture_eccentricity", m_captureEccentricity);
    descriptor(output, "kstars_ekos_capture_detected_stars", "gauge",
               "Stars detected in the most recent completed exposure.");
    optionalSample(output, "kstars_ekos_capture_detected_stars", m_captureStars);
    descriptor(output, "kstars_ekos_capture_median_adu", "gauge",
               "Median ADU of the most recent completed exposure.", "adu");
    optionalSample(output, "kstars_ekos_capture_median_adu", m_captureMedian);
    descriptor(output, "kstars_ekos_capture_target_distance_arcseconds", "gauge",
               "Distance from the scheduler target for the most recent exposure.", "arcseconds");
    optionalSample(output, "kstars_ekos_capture_target_distance_arcseconds", m_targetDistance);

    descriptor(output, "kstars_ekos_autofocus_active", "gauge", "Whether autofocus is active.");
    sample(output, "kstars_ekos_autofocus_active", m_autofocusActive ? 1.0 : 0.0);
    descriptor(output, "kstars_ekos_autofocus_last_duration_seconds", "gauge",
               "Duration of the most recent autofocus attempt.", "seconds");
    optionalSample(output, "kstars_ekos_autofocus_last_duration_seconds", m_autofocusDurationSeconds);
    descriptor(output, "kstars_ekos_autofocus_runs", "counter", "Autofocus attempts by filter and result.");
    for (auto it = m_focusTotals.cbegin(); it != m_focusTotals.cend(); ++it)
    {
        const auto filterLabel = label("filter", it.key());
        sample(output, "kstars_ekos_autofocus_runs_total", it->completed,
               filterLabel + ",result=\"completed\"");
        sample(output, "kstars_ekos_autofocus_runs_total", it->aborted,
               filterLabel + ",result=\"aborted\"");
    }
    descriptor(output, "kstars_ekos_focus_position_steps", "gauge", "Most recent focuser position.", "steps");
    optionalSample(output, "kstars_ekos_focus_position_steps", m_focusPosition);
    descriptor(output, "kstars_ekos_adaptive_focus_adjustment_steps", "gauge",
               "Most recent adaptive-focus adjustment.", "steps");
    optionalSample(output, "kstars_ekos_adaptive_focus_adjustment_steps", m_adaptiveAdjustment);
    descriptor(output, "kstars_ekos_adaptive_focus_runs", "counter", "Adaptive-focus runs by filter and result.");
    for (auto it = m_adaptiveFocusTotals.cbegin(); it != m_adaptiveFocusTotals.cend(); ++it)
    {
        const auto filterLabel = label("filter", it.key());
        sample(output, "kstars_ekos_adaptive_focus_runs_total", it->first,
               filterLabel + ",result=\"moved\"");
        sample(output, "kstars_ekos_adaptive_focus_runs_total", it->second,
               filterLabel + ",result=\"unchanged\"");
    }
    descriptor(output, "kstars_ekos_focuser_temperature_celsius", "gauge", "Focuser temperature.", "celsius");
    optionalSample(output, "kstars_ekos_focuser_temperature_celsius", m_temperature);

    descriptor(output, "kstars_ekos_guide_state", "gauge", "Current collapsed guider state.");
    const QStringList guideStates { "idle", "guiding", "calibrating", "suspended", "dithering" };
    stateSamples(output, "kstars_ekos_guide_state", guideStates, m_guideState);
    descriptor(output, "kstars_ekos_guide_state_transitions", "counter", "Guider state transitions.");
    for (const auto &state : guideStates)
        sample(output, "kstars_ekos_guide_state_transitions_total", m_guideTransitions.value(state), label("state", state));
    descriptor(output, "kstars_ekos_guide_ra_error_arcseconds", "gauge", "Current RA guide error.", "arcseconds");
    optionalSample(output, "kstars_ekos_guide_ra_error_arcseconds", m_guideRaError);
    descriptor(output, "kstars_ekos_guide_dec_error_arcseconds", "gauge", "Current DEC guide error.", "arcseconds");
    optionalSample(output, "kstars_ekos_guide_dec_error_arcseconds", m_guideDecError);
    descriptor(output, "kstars_ekos_guide_drift_arcseconds", "gauge", "Current total guide drift.", "arcseconds");
    optionalSample(output, "kstars_ekos_guide_drift_arcseconds", m_guideDrift);
    descriptor(output, "kstars_ekos_guide_rms_arcseconds", "gauge", "Rolling guider RMS error.", "arcseconds");
    optionalSample(output, "kstars_ekos_guide_rms_arcseconds", m_guideRms);
    descriptor(output, "kstars_ekos_guide_capture_rms_arcseconds", "gauge",
               "Rolling guider RMS during the active capture.", "arcseconds");
    optionalSample(output, "kstars_ekos_guide_capture_rms_arcseconds", m_captureRms);
    descriptor(output, "kstars_ekos_guide_ra_pulse_milliseconds", "gauge", "Current RA guide pulse.", "milliseconds");
    optionalSample(output, "kstars_ekos_guide_ra_pulse_milliseconds", m_guideRaPulse);
    descriptor(output, "kstars_ekos_guide_dec_pulse_milliseconds", "gauge", "Current DEC guide pulse.", "milliseconds");
    optionalSample(output, "kstars_ekos_guide_dec_pulse_milliseconds", m_guideDecPulse);
    descriptor(output, "kstars_ekos_guide_signal_to_noise_ratio", "gauge", "Current guide-star signal-to-noise ratio.");
    optionalSample(output, "kstars_ekos_guide_signal_to_noise_ratio", m_guideSnr);
    descriptor(output, "kstars_ekos_guide_detected_stars", "gauge", "Current guide-star count.");
    optionalSample(output, "kstars_ekos_guide_detected_stars", m_guideStars);
    descriptor(output, "kstars_ekos_guide_sky_background_adu", "gauge", "Current guide sky background.", "adu");
    optionalSample(output, "kstars_ekos_guide_sky_background_adu", m_guideSkyBackground);

    descriptor(output, "kstars_ekos_mount_state", "gauge", "Current mount state.");
    stateSamples(output, "kstars_ekos_mount_state",
    { "idle", "moving", "slewing", "tracking", "parking", "parked", "error" }, m_mountState);
    descriptor(output, "kstars_ekos_mount_right_ascension_degrees", "gauge", "Current mount right ascension.", "degrees");
    optionalSample(output, "kstars_ekos_mount_right_ascension_degrees", m_mountRa);
    descriptor(output, "kstars_ekos_mount_declination_degrees", "gauge", "Current mount declination.", "degrees");
    optionalSample(output, "kstars_ekos_mount_declination_degrees", m_mountDec);
    descriptor(output, "kstars_ekos_mount_hour_angle_degrees", "gauge", "Current mount hour angle.", "degrees");
    optionalSample(output, "kstars_ekos_mount_hour_angle_degrees", m_mountHourAngle);
    descriptor(output, "kstars_ekos_mount_azimuth_degrees", "gauge", "Current mount azimuth.", "degrees");
    optionalSample(output, "kstars_ekos_mount_azimuth_degrees", m_mountAzimuth);
    descriptor(output, "kstars_ekos_mount_altitude_degrees", "gauge", "Current mount altitude.", "degrees");
    optionalSample(output, "kstars_ekos_mount_altitude_degrees", m_mountAltitude);
    descriptor(output, "kstars_ekos_mount_pier_side", "gauge", "Current mount pier side.");
    for (const auto &side :
            {
                QStringLiteral("west_to_east"), QStringLiteral("east_to_west"), QStringLiteral("unknown")
            })
        sample(output, "kstars_ekos_mount_pier_side", side == m_pierSide ? 1.0 : 0.0, label("side", side));

    descriptor(output, "kstars_ekos_align_state", "gauge", "Current alignment state.");
    stateSamples(output, "kstars_ekos_align_state",
    { "idle", "complete", "failed", "aborted", "in_progress", "successful", "syncing", "slewing", "rotating", "suspended" },
    m_alignState);
    descriptor(output, "kstars_ekos_align_results", "counter", "Terminal alignment results.");
    sample(output, "kstars_ekos_align_results_total", m_alignCompleted, "result=\"completed\"");
    sample(output, "kstars_ekos_align_results_total", m_alignFailed, "result=\"failed\"");
    sample(output, "kstars_ekos_align_results_total", m_alignAborted, "result=\"aborted\"");
    descriptor(output, "kstars_ekos_meridian_flip_state", "gauge", "Current meridian-flip state.");
    stateSamples(output, "kstars_ekos_meridian_flip_state",
    { "none", "planned", "waiting", "accepted", "running", "completed", "error" }, m_flipState);
    descriptor(output, "kstars_ekos_meridian_flip_results", "counter", "Terminal meridian-flip results.");
    sample(output, "kstars_ekos_meridian_flip_results_total", m_flipCompleted, "result=\"completed\"");
    sample(output, "kstars_ekos_meridian_flip_results_total", m_flipFailed, "result=\"failed\"");

    descriptor(output, "kstars_ekos_scheduler_job_active", "gauge", "Whether a scheduler job is active.");
    sample(output, "kstars_ekos_scheduler_job_active", m_schedulerActive ? 1.0 : 0.0);
    descriptor(output, "kstars_ekos_scheduler_job_info", "gauge", "Active scheduler job name.");
    if (m_schedulerActive)
        sample(output, "kstars_ekos_scheduler_job_info", 1.0, label("job_name", m_schedulerJob));
    descriptor(output, "kstars_ekos_scheduler_job_elapsed_seconds", "gauge", "Elapsed time in the active scheduler job.",
               "seconds");
    if (m_schedulerActive && m_schedulerStartMs)
        sample(output, "kstars_ekos_scheduler_job_elapsed_seconds", std::max(0.0, (nowMs - *m_schedulerStartMs) / 1000.0));
    descriptor(output, "kstars_ekos_scheduler_job_last_duration_seconds", "gauge",
               "Duration of the most recently ended scheduler job.", "seconds");
    optionalSample(output, "kstars_ekos_scheduler_job_last_duration_seconds", m_schedulerDurationSeconds);
    descriptor(output, "kstars_ekos_scheduler_jobs_started", "counter", "Scheduler jobs started.");
    sample(output, "kstars_ekos_scheduler_jobs_started_total", m_schedulerStarted);
    descriptor(output, "kstars_ekos_scheduler_jobs_ended", "counter", "Scheduler jobs ended.");
    sample(output, "kstars_ekos_scheduler_jobs_ended_total", m_schedulerEnded);

    output += "# EOF\n";
    return output;
}

} // namespace Ekos
