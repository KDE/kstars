/*
 * worm_gear_guider.cpp — PINN + residual MLP for worm-gear mounts
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "worm_gear_guider.h"

#include "Options.h"
#include "ekos_guide_debug.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <cmath>
#include <algorithm>

namespace
{
bool fpDoubleClose(double a, double b, double tol = 1e-4)
{
    return std::abs(a - b) <= tol;
}
}  // namespace

// ── Static member initialization ──────────────────────────────────────────────
Eigen::Vector4d WormGearGuider::m_rls_theta { Eigen::Vector4d::Zero() };
Eigen::Matrix4d WormGearGuider::m_rls_P { Eigen::Matrix4d::Identity() * 100.0 };
int WormGearGuider::m_frameCount { 0 };
double WormGearGuider::m_typicalRMS { 0.5 };
double WormGearGuider::s_activePePeriod { -1.0 };
double WormGearGuider::m_pe_phase { 0.0 };
double WormGearGuider::m_pe_amplitude { 1.0 };
double WormGearGuider::m_d_ra_extra { 0.0 };
double WormGearGuider::m_uncorrectedPosRA { 0.0 };
double WormGearGuider::m_uncorrectedPosDEC { 0.0 };

WormGearGuider::WormGearGuider()
{
    m_innovRA.clear();
    m_innovDec.clear();
}

bool WormGearGuider::validateFingerprint(const QJsonObject &fp)
{
    if (fp.isEmpty())
        return true;

    const struct
    {
        const char *key;
        double current;
        double tol;
    } checks[] =
    {
        { "guide_exposure_s",      Options::guideExposure(),         0.05 },
        { "ra_proportional_gain",  Options::rAProportionalGain(),    1e-4 },
        { "dec_proportional_gain", Options::dECProportionalGain(),   1e-4 },
        { "ra_integral_gain",      Options::rAIntegralGain(),        1e-4 },
        { "dec_integral_gain",     Options::dECIntegralGain(),       1e-4 },
        { "ra_min_pulse_arcsec",   Options::rAMinimumPulseArcSec(),  1e-4 },
        { "dec_min_pulse_arcsec",  Options::dECMinimumPulseArcSec(), 1e-4 },
        { "ra_max_pulse_arcsec",   static_cast<double>(Options::rAMaximumPulseArcSec()),  1e-4 },
        { "dec_max_pulse_arcsec",  static_cast<double>(Options::dECMaximumPulseArcSec()), 1e-4 },
        { "ra_hysteresis",         Options::rAHysteresis(),          1e-4 },
        { "dec_hysteresis",        Options::dECHysteresis(),         1e-4 },
    };

    bool ok = true;
    for (const auto &c : checks)
    {
        if (fp.contains(c.key) && !fpDoubleClose(fp[c.key].toDouble(), c.current, c.tol))
        {
            qCWarning(KSTARS_EKOS_GUIDE) << "AI weights rejected:" << c.key << "recorded"
                                         << fp[c.key].toDouble() << "current" << c.current;
            ok = false;
        }
    }

    if (fp.contains("guide_binning") && fp["guide_binning"].toString() != Options::guideBinning())
    {
        qCWarning(KSTARS_EKOS_GUIDE) << "AI weights rejected: guide_binning recorded"
                                     << fp["guide_binning"].toString() << "current" << Options::guideBinning();
        ok = false;
    }

    // Pulse algorithm is Standard (0) in training fingerprint; runtime uses AI — skip.

    return ok;
}

bool WormGearGuider::loadWeights(const QString &weightsPath)
{
    m_weightsLoaded = false;

    QFile file(weightsPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject())
        return false;

    QJsonObject root = doc.object();

    if (root["mount_type"].toString() != "WORM_GEAR")
        return false;

    if (!validateFingerprint(root["model_fingerprint"].toObject()))
        return false;

    QJsonObject phys = root["physics"].toObject();
    m_pe_amplitude = phys["pe_amplitude"].toDouble(1.0);
    m_pe_period    = phys["pe_period"].toDouble(480.0);
    m_k_ref        = phys["k_ref"].toDouble(0.0);
    m_d_ra_extra   = phys["d_ra_extra"].toDouble(0.0);
    m_d_polar      = phys["d_polar"].toDouble(0.0);
    m_k_ref_dec    = phys["k_ref_dec"].toDouble(0.0);
    m_fit_alt_min  = phys["fit_alt_min"].toDouble(35.0);
    m_fit_alt_max  = phys["fit_alt_max"].toDouble(65.0);

    QJsonObject norm = root["normalization"].toObject();
    m_alt_scale      = norm["alt_scale"].toDouble(90.0);
    m_snr_scale      = norm["snr_scale"].toDouble(100.0);
    m_pulse_scale_ms = norm["pulse_scale_ms"].toDouble(1000.0);
    m_dt_scale       = norm["dt_scale"].toDouble(2.0);

    QJsonObject mlp = root["mlp"].toObject();
    if (mlp["w1"].toArray().size() != 15 * 32)
        return false;

    auto loadMatrix = [](const QJsonArray & arr, Eigen::MatrixXf & dst, int rows, int cols) -> bool
    {
        if (arr.size() != rows * cols) return false;
        dst.resize(rows, cols);
        for (int i = 0; i < rows; i++)
        {
            for (int j = 0; j < cols; j++)
                dst(i, j) = static_cast<float>(arr[i * cols + j].toDouble());
        }
        return true;
    };
    auto loadVector = [](const QJsonArray & arr, Eigen::VectorXf & dst, int size) -> bool
    {
        if (arr.size() != size) return false;
        dst.resize(size);
        for (int i = 0; i < size; i++)
            dst(i) = static_cast<float>(arr[i].toDouble());
        return true;
    };

    if (!loadMatrix(mlp["w1"].toArray(), m_w1, 32, 15) ||
            !loadVector(mlp["b1"].toArray(), m_b1, 32) ||
            !loadMatrix(mlp["w2"].toArray(), m_w2, 16, 32) ||
            !loadVector(mlp["b2"].toArray(), m_b2, 16) ||
            !loadMatrix(mlp["w_out"].toArray(), m_w_out, 2, 16) ||
            !loadVector(mlp["b_out"].toArray(), m_b_out, 2))
        return false;

    m_weightsLoaded = true;
    return true;
}

void WormGearGuider::resetSession(bool forceReset)
{
    m_confidence = 0.0;
    m_lastDt = 2.0;
    m_lastAltRad = M_PI / 4.0;
    m_lastSessionSec = 0.0;
    m_lastPredDriftDEC = 0.0;
    m_hasLastPred = false;
    m_innovRA.clear();
    m_innovDec.clear();

    // If the loaded weights describe a different mount (different PE period), the persisted
    // static RLS state belongs to the previous mount — discard it rather than reusing a
    // phase fit for the wrong worm.
    if (m_pe_period != s_activePePeriod)
    {
        s_activePePeriod = m_pe_period;
        forceReset = true;
    }

    // Only wipe the phase memory (RLS state) if the period is invalid, or if forced (e.g. meridian flip).
    // By making the RLS state static, this allows the AI to survive cloud passes, dithers,
    // AND algorithm toggles (where Ekos destroys and recreates the C++ object).
    // Once m_rls_P converges (trace < 100), it stays converged forever unless forced to reset!
    if (m_pe_period <= 0.0 || forceReset)
    {
        m_frameCount = 0;
        m_pe_phase   = 0.0;
        m_typicalRMS = 0.5;
        m_uncorrectedPosRA = 0.0;
        m_uncorrectedPosDEC = 0.0;
        m_rls_theta = Eigen::Vector4d::Zero();
        m_rls_P = Eigen::Matrix4d::Identity() * 100.0;
    }
}

GuideOutput WormGearGuider::predict(const GuideFrameData &frame)
{
    if (frame.dt > 10.0 && m_frameCount > 0)
    {
        // For long pauses (like dithering), just invalidate the last prediction delta.
        // DO NOT reset the session, as the worm gear kept physically turning!
        m_hasLastPred = false;
    }

    m_frameCount++;

    m_lastDt = frame.dt;
    m_lastAltRad = frame.altitude_deg * M_PI / 180.0;
    m_lastSessionSec = frame.t_session_sec;
    m_lastSNR = frame.snr;
    m_lastPixelScale = frame.pixel_scale;
    m_lastParallacticAngleDeg = frame.parallactic_angle_deg;
    m_lastRAPulseMs = frame.ra_pulse_ms;
    m_lastDECPulseMs = frame.dec_pulse_ms;
    m_lastPierSide = frame.pier_side_east ? 1.0f : -1.0f;

    const double t_next = frame.t_session_sec + frame.dt;
    const double phys_ra  = physicsRA(t_next, frame.altitude_deg) * frame.dt;
    const double phys_dec = physicsDEC(frame.altitude_deg, frame.parallactic_angle_deg) * frame.dt;

    const auto mlp_out = runMLP(static_cast<float>(frame.altitude_deg),
                                static_cast<float>(frame.snr),
                                static_cast<float>(-frame.ra_pulse_ms),
                                static_cast<float>(frame.dec_pulse_ms),
                                static_cast<float>(frame.dt),
                                frame.t_session_sec,
                                static_cast<float>(frame.parallactic_angle_deg),
                                m_lastPierSide);

    GuideOutput out;
    out.valid      = (m_frameCount > warmupFrames());
    out.confidence = m_confidence;

    m_lastPredDriftRA  = phys_ra + mlp_out[0];
    m_lastPredDriftDEC = phys_dec + mlp_out[1];
    m_hasLastPred = true;

    if (!out.valid)
        return out;

    out.ra_correction_arcsec  = m_lastPredDriftRA * frame.pixel_scale;
    out.dec_correction_arcsec = m_lastPredDriftDEC * frame.pixel_scale;

    out.physics_ra_arcsec = phys_ra * frame.pixel_scale;
    out.physics_dec_arcsec = phys_dec * frame.pixel_scale;
    out.mlp_ra_arcsec = mlp_out[0] * frame.pixel_scale;
    out.mlp_dec_arcsec = mlp_out[1] * frame.pixel_scale;

    return out;
}

void WormGearGuider::update(double /*ra_error_px*/, double /*dec_error_px*/,
                            double uncorrected_drift_ra_px_delta, double uncorrected_drift_dec_px_delta, double snr,
                            double /*ra_pulse_px*/, double /*dec_pulse_px*/)
{
    m_uncorrectedPosRA += uncorrected_drift_ra_px_delta;
    m_uncorrectedPosDEC += uncorrected_drift_dec_px_delta;

    if (m_hasLastPred)
    {
        const double innov_ra  = uncorrected_drift_ra_px_delta - m_lastPredDriftRA;
        const double innov_dec = uncorrected_drift_dec_px_delta - m_lastPredDriftDEC;
        updateConfidence(innov_ra, innov_dec, snr);
    }

    updatePhase(m_uncorrectedPosRA, m_lastSessionSec);
}

QString WormGearGuider::stateString() const
{
    return QString("WormGear A=%1 T=%2 φ=%3 conf=%4")
           .arg(m_pe_amplitude, 0, 'f', 2)
           .arg(m_pe_period, 0, 'f', 0)
           .arg(m_pe_phase, 0, 'f', 3)
           .arg(m_confidence, 0, 'f', 2);
}

double WormGearGuider::physicsRA(double t_sec, double /*altitude_deg*/) const
{
    const double omega = 2.0 * M_PI / m_pe_period;

    // Derivative of position model y(t) = c1*sin(wt) + c2*cos(wt) + v*t + C
    // y'(t) = c1*w*cos(wt) - c2*w*sin(wt) + v
    const double pe_rate = m_rls_theta(0) * omega * std::cos(omega * t_sec)
                           - m_rls_theta(1) * omega * std::sin(omega * t_sec);

    return pe_rate + m_rls_theta(2);
}

double WormGearGuider::physicsDEC(double altitude_deg, double parallactic_angle_deg) const
{
    // Refraction fit is only valid inside the fitted altitude range.
    const double alt_rad = std::clamp(altitude_deg, m_fit_alt_min, m_fit_alt_max) * M_PI / 180.0;
    const double q_rad = parallactic_angle_deg * M_PI / 180.0;
    const double cos_alt = std::cos(alt_rad);

    // Protect against division by zero near zenith
    if (std::abs(cos_alt) < 1e-4) return m_d_polar;

    return m_d_polar + m_k_ref_dec * std::sin(q_rad) / (cos_alt * cos_alt);
}

std::array<float, 2> WormGearGuider::runMLP(float altitude, float snr,
        float last_ra_pulse, float last_dec_pulse, float dt, double t_session_sec, float parallactic_angle_deg,
        float pier_side) const
{
    // Extract true mechanical phase from the RLS state
    // y(t) = theta(0)*sin(wt) + theta(1)*cos(wt) -> A * sin(wt + phi)
    const double phi_rls = std::atan2(m_rls_theta(1), m_rls_theta(0));
    const double phase = 2.0 * M_PI * t_session_sec / m_pe_period + phi_rls;

    Eigen::Vector<float, 15> x;
    x << altitude / static_cast<float>(m_alt_scale),
    static_cast<float>(std::sin(phase)),
    static_cast<float>(std::cos(phase)),
    static_cast<float>(std::sin(2.0 * phase)),
    static_cast<float>(std::cos(2.0 * phase)),
    static_cast<float>(std::sin(3.0 * phase)),
    static_cast<float>(std::cos(3.0 * phase)),
    static_cast<float>(std::sin(4.0 * phase)),
    static_cast<float>(std::cos(4.0 * phase)),
    snr / static_cast<float>(m_snr_scale),
    last_ra_pulse / static_cast<float>(m_pulse_scale_ms),
    last_dec_pulse / static_cast<float>(m_pulse_scale_ms),
    dt / static_cast<float>(m_dt_scale),
    parallactic_angle_deg / 180.0f,
    pier_side;

    const Eigen::Vector<float, 32> h1 = (m_w1 * x + m_b1).cwiseMax(0.0f);
    const Eigen::Vector<float, 16> h2 = (m_w2 * h1 + m_b2).cwiseMax(0.0f);
    const Eigen::Vector<float, 2> out = m_w_out * h2 + m_b_out;

    return {out(0), out(1)};
}

void WormGearGuider::updatePhase(double uncorrected_position_px, double t_session_sec)
{
    if (m_pe_period <= 0) return;

    const double omega = 2.0 * M_PI / m_pe_period;

    Eigen::Vector4d x;
    x << std::sin(omega * t_session_sec),
    std::cos(omega * t_session_sec),
    t_session_sec,
    1.0;

    // Standard RLS Update for Position
    double h_P_h = x.transpose() * m_rls_P * x;
    double denom = RLS_LAMBDA + h_P_h;

    Eigen::Vector4d K = Eigen::Vector4d::Zero();
    if (denom > 1e-6)
        K = (m_rls_P * x) / denom;

    double prediction = x.transpose() * m_rls_theta;
    double error = uncorrected_position_px - prediction;

    m_rls_theta = m_rls_theta + K * error;
    m_rls_P = (m_rls_P - K * x.transpose() * m_rls_P) / RLS_LAMBDA;

    // Extract physical parameters for state reporting and MLP compatibility
    m_pe_amplitude = std::sqrt(m_rls_theta(0) * m_rls_theta(0) + m_rls_theta(1) * m_rls_theta(1));
    m_pe_phase = std::atan2(m_rls_theta(1), m_rls_theta(0));
    m_d_ra_extra = m_rls_theta(2);
}

void WormGearGuider::updateConfidence(double innovRA, double innovDec, double snr)
{
    m_innovRA.push_back(innovRA);
    if (m_innovRA.size() > INNOV_WINDOW)
        m_innovRA.pop_front();

    m_innovDec.push_back(innovDec);
    if (m_innovDec.size() > INNOV_WINDOW)
        m_innovDec.pop_front();

    auto axisRms = [](const std::deque<double> &buf) -> double
    {
        if (buf.empty())
            return 0.0;
        double sum_sq = 0.0;
        for (double e : buf)
            sum_sq += e * e;
        return std::sqrt(sum_sq / static_cast<double>(buf.size()));
    };

    const double ra_rms  = axisRms(m_innovRA);
    const double dec_rms = axisRms(m_innovDec);
    const double innov_rms = std::sqrt((ra_rms * ra_rms + dec_rms * dec_rms) / 2.0);

    // Use EMA after warmup so m_typicalRMS adapts to changing conditions
    // (seeing changes, altitude changes) instead of staying frozen from warmup.
    if (m_frameCount <= warmupFrames())
        m_typicalRMS = std::max(0.05, innov_rms);
    else
        m_typicalRMS = 0.99 * m_typicalRMS + 0.01 * innov_rms;

    const double error_ratio = innov_rms / (m_typicalRMS + 1e-6);

    double snr_factor = std::min(1.0, (snr - 10.0) / 20.0);
    snr_factor = std::max(0.0, snr_factor);

    double warmup_factor = 0.0;
    if (m_frameCount > warmupFrames())
    {
        // Start at 30% confidence immediately (since we already watched 30 frames),
        // and smoothly ramp the remaining 70% over the next 20 frames.
        warmup_factor = 0.3 + 0.7 * std::min(1.0, static_cast<double>(m_frameCount - warmupFrames()) / 20.0);
    }

    // Lorentzian confidence instead of exponential.
    // exp(-r) gives ~0.37 at r=1, crushing the AI contribution.
    // 1/(1+r²) gives ~0.50 at r=1, allowing meaningful feed-forward.
    const double prediction_quality = 1.0 / (1.0 + error_ratio * error_ratio);
    m_confidence = std::clamp(warmup_factor * snr_factor * prediction_quality, 0.0, 1.0);
}

GuideOutput WormGearGuider::darkPredict(double dt_sec)
{
    // Advance the time forward by the dark guiding timestep
    m_lastSessionSec += dt_sec;

    const double phys_ra  = physicsRA(m_lastSessionSec, m_lastAltRad * 180.0 / M_PI) * dt_sec;
    const double phys_dec = physicsDEC(m_lastAltRad * 180.0 / M_PI, m_lastParallacticAngleDeg) * dt_sec;

    const auto mlp_out = runMLP(static_cast<float>(m_lastAltRad * 180.0 / M_PI),
                                static_cast<float>(m_lastSNR),
                                static_cast<float>(-m_lastRAPulseMs),
                                static_cast<float>(m_lastDECPulseMs),
                                static_cast<float>(dt_sec),
                                m_lastSessionSec,
                                static_cast<float>(m_lastParallacticAngleDeg),
                                m_lastPierSide);

    GuideOutput out;
    out.valid      = (m_frameCount > warmupFrames());
    out.confidence = m_confidence; // Use last known confidence

    // We do NOT update m_lastPredDriftRA because there's no actual frame
    // to compare it to in update(). We just return the prediction.
    // Convert from pixels to arcseconds using the cached pixel_scale from the last real frame.
    out.ra_correction_arcsec  = (phys_ra + mlp_out[0]) * m_lastPixelScale;
    out.dec_correction_arcsec = (phys_dec + mlp_out[1]) * m_lastPixelScale;

    out.physics_ra_arcsec  = phys_ra * m_lastPixelScale;
    out.physics_dec_arcsec = phys_dec * m_lastPixelScale;
    out.mlp_ra_arcsec      = mlp_out[0] * m_lastPixelScale;
    out.mlp_dec_arcsec     = mlp_out[1] * m_lastPixelScale;

    return out;
}
