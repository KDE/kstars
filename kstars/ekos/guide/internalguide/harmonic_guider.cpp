/*
 * harmonic_guider.cpp — Neural Kalman Filter for harmonic drive mounts
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "harmonic_guider.h"

#include "Options.h"
#include "ekos_guide_debug.h"

#include <Eigen/LU>

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
Eigen::Matrix<double, HarmonicGuider::N_STATES, 1>
HarmonicGuider::m_x { Eigen::Matrix<double, N_STATES, 1>::Zero() };

Eigen::Matrix<double, HarmonicGuider::N_STATES, HarmonicGuider::N_STATES>
HarmonicGuider::m_P { Eigen::Matrix<double, N_STATES, N_STATES>::Identity() * 10.0 };

double HarmonicGuider::m_uncorrPosRA { 0.0 };
double HarmonicGuider::m_uncorrPosDEC { 0.0 };
int HarmonicGuider::m_frameCount { 0 };
double HarmonicGuider::m_typicalRMS { 0.5 };
double HarmonicGuider::s_activePePeriod { -1.0 };

HarmonicGuider::HarmonicGuider()
{
    m_innovRA.clear();
    m_innovDec.clear();

    // Q-net weights default to zero. With zero weights the net outputs log_q=0 → Q≈1.0 px²
    // (a high, uninformative process noise). Overwritten by loadWeights when the JSON has a qnet block.
    m_qw1.setZero();
    m_qb1.setZero();
    m_qw2.setZero();
    m_qb2.setZero();
}

bool HarmonicGuider::validateFingerprint(const QJsonObject &fp)
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

    return ok;
}

bool HarmonicGuider::loadWeights(const QString &weightsPath)
{
    m_weightsLoaded = false;

    QFile file(weightsPath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject())
        return false;

    QJsonObject root = doc.object();

    if (root["mount_type"].toString() != "HARMONIC_DRIVE")
        return false;

    if (!validateFingerprint(root["model_fingerprint"].toObject()))
        return false;

    QJsonObject phys = root["physical"].toObject();
    m_kappa_ra  = phys["kappa_ra"].toDouble(0.2);
    m_tau_ra    = phys["tau_ra"].toDouble(1.5);
    m_kappa_dec = phys["kappa_dec"].toDouble(0.2);
    m_tau_dec   = phys["tau_dec"].toDouble(1.5);
    m_drift_ra  = phys["drift_ra"].toDouble(0.0);
    m_drift_dec = phys["drift_dec"].toDouble(0.0);
    m_k_ref     = phys["k_ref"].toDouble(0.0);
    m_d_polar   = phys["d_polar"].toDouble(0.0);
    m_k_ref_dec = phys["k_ref_dec"].toDouble(0.0);
    m_pe_period = phys["pe_period"].toDouble(0.0);
    m_pe_amplitude = phys["pe_amplitude"].toDouble(0.0);
    m_fit_alt_min = phys["fit_alt_min"].toDouble(35.0);
    m_fit_alt_max = phys["fit_alt_max"].toDouble(65.0);

    // Sanity bounds on spring parameters
    m_kappa_ra  = std::clamp(m_kappa_ra, 0.0, 0.9);
    m_kappa_dec = std::clamp(m_kappa_dec, 0.0, 0.9);
    m_tau_ra    = std::clamp(m_tau_ra, 0.1, 10.0);
    m_tau_dec   = std::clamp(m_tau_dec, 0.1, 10.0);

    // Load Q-net MLP weights
    QJsonObject qnet = root["qnet"].toObject();

    auto loadMatrixFixed = [](const QJsonArray & arr, auto & dst, int rows, int cols) -> bool
    {
        if (arr.size() != rows * cols) return false;
        for (int i = 0; i < rows; i++)
        {
            for (int j = 0; j < cols; j++)
                dst(i, j) = static_cast<float>(arr[i * cols + j].toDouble());
        }
        return true;
    };
    auto loadVectorFixed = [](const QJsonArray & arr, auto & dst, int size) -> bool
    {
        if (arr.size() != size) return false;
        for (int i = 0; i < size; i++)
            dst(i) = static_cast<float>(arr[i].toDouble());
        return true;
    };

    // Q-net is optional. If the block is absent the weights stay zero → Q≈1.0 px² (high,
    // uninformative); trainer-produced files always include qnet, so this only affects foreign JSON.
    if (!qnet.isEmpty())
    {
        loadMatrixFixed(qnet["w1"].toArray(), m_qw1, 8, 5);
        loadVectorFixed(qnet["b1"].toArray(), m_qb1, 8);
        loadMatrixFixed(qnet["w2"].toArray(), m_qw2, 2, 8);
        loadVectorFixed(qnet["b2"].toArray(), m_qb2, 2);
    }

    m_weightsLoaded = true;
    return true;
}

void HarmonicGuider::resetSession(bool forceReset)
{
    m_confidence = 0.0;
    m_lastDt = 2.0;
    m_lastAltRad = M_PI / 4.0;
    m_lastSessionSec = 0.0;
    m_lastSNR = 0.0;
    m_lastSNRDelta = 0.0;
    m_prevSNR = 0.0;
    m_lastPredRA = 0.0;
    m_lastPredDEC = 0.0;
    m_hasLastPred = false;
    m_innovRA.clear();
    m_innovDec.clear();

    // Q-net feature history — start fresh so the first frame emits a zero feature
    // rather than diffing against a stale previous session's error.
    m_prevRaRawPx  = 0.0;
    m_prevDecRawPx = 0.0;
    m_hasPrevRaw   = false;
    m_qFeatRA      = 0.0;
    m_qFeatDec     = 0.0;

    // If the loaded weights describe a different mount (different PE period), the
    // persisted static Kalman state belongs to the previous mount — discard it entirely.
    if (m_pe_period != s_activePePeriod)
    {
        s_activePePeriod = m_pe_period;
        forceReset = true;
    }

    m_uncorrPosRA = 0.0;
    m_uncorrPosDEC = 0.0;

    if (forceReset)
    {
        // Full reset: first use, meridian flip, or mount/weights change.
        m_frameCount = 0;
        m_typicalRMS = 0.5;
        m_x.setZero();
        m_P = Eigen::Matrix<double, N_STATES, N_STATES>::Identity() * 10.0;
    }
    else
    {
        // New guiding session on the same mount. The tracking-error state
        // (position / velocity / spring wind-up) is session-specific and must NOT
        // carry over, or the filter injects a stale correction on the first frame.
        // Only the mechanical PE oscillator state (PE_SIN/PE_COS) is allowed to persist.
        const int transient[] = { RA_POS, RA_VEL, RA_SPRING, DEC_POS, DEC_VEL, DEC_SPRING };
        for (int idx : transient)
        {
            m_x(idx) = 0.0;
            // Decorrelate the reset states from the rest of the filter and re-inflate
            // their variance so the filter re-learns them quickly. Zeroing the whole
            // row/column keeps the covariance matrix consistent (positive semi-definite).
            m_P.row(idx).setZero();
            m_P.col(idx).setZero();
            m_P(idx, idx) = 10.0;
        }

        // Re-run warmup so no feed-forward is emitted until the filter re-converges.
        m_frameCount = 0;
    }
}

// ── State transition matrix builder ──────────────────────────────────────────
void HarmonicGuider::buildF(Eigen::Matrix<double, N_STATES, N_STATES> &F, double dt) const
{
    F.setIdentity();

    // Position evolves with velocity: pos(t+dt) = pos(t) + vel(t)*dt
    F(RA_POS, RA_VEL) = dt;
    F(DEC_POS, DEC_VEL) = dt;

    // Spring decays exponentially: spring(t+dt) = spring(t) * exp(-dt/tau)
    F(RA_SPRING, RA_SPRING) = std::exp(-dt / m_tau_ra);
    F(DEC_SPRING, DEC_SPRING) = std::exp(-dt / m_tau_dec);

    // PE states evolve as 2D rotation at the PE frequency
    if (m_pe_period > 0.0)
    {
        const double omega = 2.0 * M_PI / m_pe_period;
        const double cos_wdt = std::cos(omega * dt);
        const double sin_wdt = std::sin(omega * dt);

        // RA PE rotation
        F(RA_PE_SIN, RA_PE_SIN) = cos_wdt;
        F(RA_PE_SIN, RA_PE_COS) = sin_wdt;
        F(RA_PE_COS, RA_PE_SIN) = -sin_wdt;
        F(RA_PE_COS, RA_PE_COS) = cos_wdt;

        // DEC PE rotation
        F(DEC_PE_SIN, DEC_PE_SIN) = cos_wdt;
        F(DEC_PE_SIN, DEC_PE_COS) = sin_wdt;
        F(DEC_PE_COS, DEC_PE_SIN) = -sin_wdt;
        F(DEC_PE_COS, DEC_PE_COS) = cos_wdt;
    }
}

// ── Q-net: compute adaptive process noise ────────────────────────────────────
Eigen::Matrix<double, HarmonicGuider::N_STATES, HarmonicGuider::N_STATES>
HarmonicGuider::computeQ(double snr, double snr_delta,
                         double innov_ra, double innov_dec, double dt) const
{
    // Q-net input: [snr_norm, snr_delta_norm, |Δra_raw_px|, |Δdec_raw_px|, dt_norm].
    // innov_ra/innov_dec here carry the trainer's feature (raw frame-to-frame error
    // change), matching train_harmonic.py::_train_qnet — not the Kalman innovation.
    Eigen::Vector<float, 5> qin;
    qin << static_cast<float>(snr / 100.0),
        static_cast<float>(snr_delta / 10.0),
        static_cast<float>(std::abs(innov_ra)),
        static_cast<float>(std::abs(innov_dec)),
        static_cast<float>(dt / 2.0);

    // Forward pass: 5 → 8 (ReLU) → 2
    const Eigen::Vector<float, 8> h1 = (m_qw1 * qin + m_qb1).cwiseMax(0.0f);
    const Eigen::Vector2f log_q = m_qw2 * h1 + m_qb2;

    // Convert from log-scale: Q_diagonal = exp(log_q)
    // Clamp to prevent numerical issues (Q between 1e-4 and 100)
    const double q_ra  = std::clamp(std::exp(static_cast<double>(log_q(0))), 1e-4, 100.0);
    const double q_dec = std::clamp(std::exp(static_cast<double>(log_q(1))), 1e-4, 100.0);

    // Build diagonal process noise matrix
    Eigen::Matrix<double, N_STATES, N_STATES> Q = Eigen::Matrix<double, N_STATES, N_STATES>::Zero();

    // Position process noise (scaled by Q-net output)
    Q(RA_POS, RA_POS) = q_ra * dt;
    Q(DEC_POS, DEC_POS) = q_dec * dt;

    // Fixed small noise: velocity tracks only the slow residual drift trend
    Q(RA_VEL, RA_VEL) = 1e-5 * dt;
    Q(DEC_VEL, DEC_VEL) = 1e-5 * dt;

    // Spring process noise (very small — spring params are stable)
    Q(RA_SPRING, RA_SPRING) = 0.001 * dt;
    Q(DEC_SPRING, DEC_SPRING) = 0.001 * dt;

    // PE process noise (small — PE is nearly deterministic)
    if (m_pe_period > 0.0)
    {
        Q(RA_PE_SIN, RA_PE_SIN) = 0.001 * dt;
        Q(RA_PE_COS, RA_PE_COS) = 0.001 * dt;
        Q(DEC_PE_SIN, DEC_PE_SIN) = 0.001 * dt;
        Q(DEC_PE_COS, DEC_PE_COS) = 0.001 * dt;
    }

    return Q;
}

// ── Drift model (px/s); altitude clamped to the fitted range ─────────────────
void HarmonicGuider::driftRates(double alt_deg, double parallactic_angle_deg,
                                double &ra_rate, double &dec_rate) const
{
    const double alt_rad = std::clamp(alt_deg, m_fit_alt_min, m_fit_alt_max) * M_PI / 180.0;
    const double cos_alt = std::cos(alt_rad);
    const double q_rad = parallactic_angle_deg * M_PI / 180.0;

    ra_rate = m_drift_ra;
    dec_rate = m_drift_dec + m_d_polar;
    if (std::abs(cos_alt) > 1e-4)
    {
        ra_rate += m_k_ref / (cos_alt * cos_alt);
        dec_rate += m_k_ref_dec * std::sin(q_rad) / (cos_alt * cos_alt);
    }
}

// ── Kalman predict step: free dynamics only, pulses are applied in update() ──
void HarmonicGuider::kalmanPredict(double dt, double alt_deg, double parallactic_angle_deg)
{
    Eigen::Matrix<double, N_STATES, N_STATES> F;
    buildF(F, dt);

    // Capture the spring tension BEFORE F decays it: released = spring_before·(1 - e^(-dt/τ)).
    const double spring_before_ra  = m_x(RA_SPRING);
    const double spring_before_dec = m_x(DEC_SPRING);

    m_x = F * m_x;

    // Spring release continues the correction as absorbed pulse energy unwinds
    const double spring_released_ra  = spring_before_ra  * (1.0 - std::exp(-dt / m_tau_ra));
    const double spring_released_dec = spring_before_dec * (1.0 - std::exp(-dt / m_tau_dec));
    m_x(RA_POS)  -= spring_released_ra;
    m_x(DEC_POS) -= spring_released_dec;

    double ra_drift_rate = 0.0, dec_drift_rate = 0.0;
    driftRates(alt_deg, parallactic_angle_deg, ra_drift_rate, dec_drift_rate);
    m_x(RA_POS)  += ra_drift_rate * dt;
    m_x(DEC_POS) += dec_drift_rate * dt;

    // Predict covariance. The Q-net features (m_qFeatRA/Dec) are the raw frame-to-frame
    // error changes matching train_harmonic.py — NOT the Kalman innovation.
    Eigen::Matrix<double, N_STATES, N_STATES> Q = computeQ(
            m_lastSNR, m_lastSNRDelta,
            m_qFeatRA,
            m_qFeatDec,
            dt);
    m_P = F * m_P * F.transpose() + Q;
}

// ── Kalman update step ───────────────────────────────────────────────────────
void HarmonicGuider::kalmanUpdate(double ra_meas_px, double dec_meas_px, double snr)
{
    // Observation matrix: observe position + PE_sin
    // H extracts: ra_obs = ra_err + pe_sin_ra, dec_obs = dec_err + pe_sin_dec
    Eigen::Matrix<double, N_OBS, N_STATES> H = Eigen::Matrix<double, N_OBS, N_STATES>::Zero();
    H(0, RA_POS) = 1.0;
    H(1, DEC_POS) = 1.0;
    if (m_pe_period > 0.0)
    {
        H(0, RA_PE_SIN) = 1.0;
        H(1, DEC_PE_SIN) = 1.0;
    }

    // Measurement noise from the current frame's SNR (~0.5 px at SNR 30)
    const double snr_safe = std::max(snr, 5.0);
    const double r_val = std::clamp(0.25 * (30.0 / snr_safe) * (30.0 / snr_safe), 0.04, 4.0);
    Eigen::Matrix2d R = Eigen::Matrix2d::Identity() * r_val;

    // Innovation
    Eigen::Vector2d z(ra_meas_px, dec_meas_px);
    Eigen::Vector2d y = z - H * m_x;

    // Innovation covariance
    Eigen::Matrix2d S = H * m_P * H.transpose() + R;

    // Kalman gain (2x10 matrix)
    Eigen::Matrix<double, N_STATES, N_OBS> K = m_P * H.transpose() * S.inverse();

    // State update
    m_x = m_x + K * y;

    // Covariance update (Joseph form for numerical stability)
    Eigen::Matrix<double, N_STATES, N_STATES> IKH =
        Eigen::Matrix<double, N_STATES, N_STATES>::Identity() - K * H;
    m_P = IKH * m_P * IKH.transpose() + K * R * K.transpose();
}

// ── Main predict entry point ─────────────────────────────────────────────────
GuideOutput HarmonicGuider::predict(const GuideFrameData &frame)
{
    if (frame.dt > 10.0 && m_frameCount > 0)
    {
        // Long pause (dithering etc.) — invalidate last prediction
        m_hasLastPred = false;
    }

    m_frameCount++;

    // Cache frame data for darkPredict
    m_lastDt = frame.dt;
    m_lastAltRad = frame.altitude_deg * M_PI / 180.0;
    m_lastSessionSec = frame.t_session_sec;
    m_lastSNRDelta = frame.snr - m_prevSNR;
    m_prevSNR = frame.snr;
    m_lastSNR = frame.snr;
    m_lastPixelScale = frame.pixel_scale;
    m_lastParallacticAngleDeg = frame.parallactic_angle_deg;
    m_lastRAPulseMs = frame.ra_pulse_ms;
    m_lastDECPulseMs = frame.dec_pulse_ms;

    // Q-net input feature: raw frame-to-frame change of tracking error, computed the
    // same way as train_harmonic.py (|ra_raw_px - prev_ra_raw_px|). Feeding the Kalman
    // innovation here instead is out-of-distribution for the
    // trained Q-net (the trainer never runs the filter; it uses raw frame differences).
    if (m_hasPrevRaw)
    {
        m_qFeatRA  = std::abs(frame.ra_raw_px  - m_prevRaRawPx);
        m_qFeatDec = std::abs(frame.dec_raw_px - m_prevDecRawPx);
    }
    else
    {
        m_qFeatRA = m_qFeatDec = 0.0;
    }
    m_prevRaRawPx  = frame.ra_raw_px;
    m_prevDecRawPx = frame.dec_raw_px;
    m_hasPrevRaw   = true;

    // Posterior (POS + PE) before propagation
    double post_ra  = m_x(RA_POS);
    double post_dec = m_x(DEC_POS);
    if (m_pe_period > 0.0)
    {
        post_ra  += m_x(RA_PE_SIN);
        post_dec += m_x(DEC_PE_SIN);
    }

    kalmanPredict(frame.dt, frame.altitude_deg, frame.parallactic_angle_deg);

    double pred_ra  = m_x(RA_POS);
    double pred_dec = m_x(DEC_POS);
    if (m_pe_period > 0.0)
    {
        pred_ra  += m_x(RA_PE_SIN);
        pred_dec += m_x(DEC_PE_SIN);
    }

    // Prediction is the expected uncorrected drift over the next interval
    m_lastPredRA  = pred_ra - post_ra;
    m_lastPredDEC = pred_dec - post_dec;
    m_hasLastPred = true;

    GuideOutput out;
    out.valid = (m_frameCount > warmupFrames());
    out.confidence = m_confidence;

    if (!out.valid)
        return out;

    // Convert prediction from pixels to arcseconds
    out.ra_correction_arcsec  = m_lastPredRA * frame.pixel_scale;
    out.dec_correction_arcsec = m_lastPredDEC * frame.pixel_scale;

    // Debug breakdown: physics = trend (VEL), mlp = spring + PE (the "learned" part)
    const double drift_ra_px = m_x(RA_VEL) * frame.dt;
    const double drift_dec_px = m_x(DEC_VEL) * frame.dt;
    out.physics_ra_arcsec  = drift_ra_px * frame.pixel_scale;
    out.physics_dec_arcsec = drift_dec_px * frame.pixel_scale;
    out.mlp_ra_arcsec  = (m_lastPredRA - drift_ra_px) * frame.pixel_scale;
    out.mlp_dec_arcsec = (m_lastPredDEC - drift_dec_px) * frame.pixel_scale;

    return out;
}

void HarmonicGuider::update(double /*ra_error_px*/, double /*dec_error_px*/,
                            double uncorrected_drift_ra_px, double uncorrected_drift_dec_px, double snr,
                            double ra_pulse_px, double dec_pulse_px)
{
    // The absorbed κ·pulse enters the uncorrected trajectory now and releases later
    m_x(RA_SPRING)  += m_kappa_ra * ra_pulse_px;
    m_x(DEC_SPRING) += m_kappa_dec * dec_pulse_px;
    m_x(RA_POS)     += m_kappa_ra * ra_pulse_px;
    m_x(DEC_POS)    += m_kappa_dec * dec_pulse_px;

    // The filter observes the integrated uncorrected position
    m_uncorrPosRA  += uncorrected_drift_ra_px;
    m_uncorrPosDEC += uncorrected_drift_dec_px;

    kalmanUpdate(m_uncorrPosRA, m_uncorrPosDEC, snr);

    if (m_hasLastPred)
    {
        const double innov_ra  = uncorrected_drift_ra_px - m_lastPredRA;
        const double innov_dec = uncorrected_drift_dec_px - m_lastPredDEC;
        updateConfidence(innov_ra, innov_dec, snr);
    }
}

QString HarmonicGuider::stateString() const
{
    return QString("Harmonic κ_ra=%1 τ_ra=%2 PE=%3s conf=%4")
           .arg(m_kappa_ra, 0, 'f', 2)
           .arg(m_tau_ra, 0, 'f', 1)
           .arg(m_pe_period, 0, 'f', 1)
           .arg(m_confidence, 0, 'f', 2);
}

// ── Confidence update — same pattern as WormGearGuider ───────────────────────
void HarmonicGuider::updateConfidence(double innovRA, double innovDec, double snr)
{
    m_innovRA.push_back(innovRA);
    if (static_cast<int>(m_innovRA.size()) > INNOV_WINDOW)
        m_innovRA.pop_front();

    m_innovDec.push_back(innovDec);
    if (static_cast<int>(m_innovDec.size()) > INNOV_WINDOW)
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
        warmup_factor = 0.3 + 0.7 * std::min(1.0, static_cast<double>(m_frameCount - warmupFrames()) / 20.0);
    }

    // Lorentzian confidence (same as WormGearGuider)
    const double prediction_quality = 1.0 / (1.0 + error_ratio * error_ratio);
    m_confidence = std::clamp(warmup_factor * snr_factor * prediction_quality, 0.0, 1.0);
}

// ── Dark guiding: propagate a copy statelessly, return the interval increment ─
GuideOutput HarmonicGuider::darkPredict(double dt_sec)
{
    m_lastSessionSec += dt_sec;

    Eigen::Matrix<double, N_STATES, 1> x = m_x;
    Eigen::Matrix<double, N_STATES, N_STATES> F;
    buildF(F, dt_sec);

    double post_ra = x(RA_POS);
    double post_dec = x(DEC_POS);
    if (m_pe_period > 0.0)
    {
        post_ra += x(RA_PE_SIN);
        post_dec += x(DEC_PE_SIN);
    }

    const double release_ra  = x(RA_SPRING)  * (1.0 - std::exp(-dt_sec / m_tau_ra));
    const double release_dec = x(DEC_SPRING) * (1.0 - std::exp(-dt_sec / m_tau_dec));
    x = F * x;
    x(RA_POS)  -= release_ra;
    x(DEC_POS) -= release_dec;

    double ra_rate = 0.0, dec_rate = 0.0;
    driftRates(m_lastAltRad * 180.0 / M_PI, m_lastParallacticAngleDeg, ra_rate, dec_rate);
    x(RA_POS)  += ra_rate * dt_sec;
    x(DEC_POS) += dec_rate * dt_sec;

    double pred_ra = x(RA_POS);
    double pred_dec = x(DEC_POS);
    if (m_pe_period > 0.0)
    {
        pred_ra += x(RA_PE_SIN);
        pred_dec += x(DEC_PE_SIN);
    }

    GuideOutput out;
    out.valid = (m_frameCount > warmupFrames());
    out.confidence = m_confidence;

    out.ra_correction_arcsec  = (pred_ra - post_ra) * m_lastPixelScale;
    out.dec_correction_arcsec = (pred_dec - post_dec) * m_lastPixelScale;

    out.physics_ra_arcsec  = x(RA_VEL) * dt_sec * m_lastPixelScale;
    out.physics_dec_arcsec = x(DEC_VEL) * dt_sec * m_lastPixelScale;
    out.mlp_ra_arcsec  = (pred_ra - post_ra - x(RA_VEL) * dt_sec) * m_lastPixelScale;
    out.mlp_dec_arcsec = (pred_dec - post_dec - x(DEC_VEL) * dt_sec) * m_lastPixelScale;

    return out;
}
