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

// ── Static member initialization ──────────────────────────────────────────────
Eigen::Matrix<double, WormGearGuider::N_STATES, 1> WormGearGuider::m_rls_theta
{ Eigen::Matrix<double, WormGearGuider::N_STATES, 1>::Zero() };
Eigen::Matrix<double, WormGearGuider::N_STATES, WormGearGuider::N_STATES> WormGearGuider::m_rls_P
{ Eigen::Matrix<double, WormGearGuider::N_STATES, WormGearGuider::N_STATES>::Identity() * 100.0 };
int WormGearGuider::m_frameCount { 0 };
double WormGearGuider::m_typicalRMS_ra { 0.5 };
double WormGearGuider::m_typicalRMS_dec { 0.5 };
double WormGearGuider::s_activePePeriod { -1.0 };
double WormGearGuider::m_pe_phase { 0.0 };
std::array<double, WormGearGuider::N_HARMONICS> WormGearGuider::m_pe_harmonic_amplitude { {1.0, 0.0, 0.0, 0.0} };
double WormGearGuider::m_pe_amplitude { 1.0 };
double WormGearGuider::m_d_ra_extra { 0.0 };
double WormGearGuider::m_uncorrectedPosRA { 0.0 };
double WormGearGuider::m_uncorrectedPosDEC { 0.0 };
double WormGearGuider::m_rlsTimeOrigin { -1.0 };
double WormGearGuider::m_kfResidualRate { 0.0 };
double WormGearGuider::m_kfP { 0.04 }; ///< bootstrap default; resetSession() reseeds from the loaded m_kfR
double WormGearGuider::m_kfResidualRateDec { 0.0 };
double WormGearGuider::m_kfPDec { 0.04 }; ///< bootstrap default; resetSession() reseeds from the loaded m_kfRDec

WormGearGuider::WormGearGuider()
{
    m_innovRA.clear();
    m_innovDec.clear();
}

bool WormGearGuider::loadWeights(const QString &weightsPath)
{
    m_weightsLoaded = false;
    m_FingerprintError.clear();
    m_FingerprintApplied.clear();

    QFile file(weightsPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        m_FingerprintError = "weights file could not be opened";
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject())
    {
        m_FingerprintError = "weights file is not valid JSON";
        return false;
    }

    QJsonObject root = doc.object();

    if (root["mount_type"].toString() != "WORM_GEAR")
    {
        m_FingerprintError = "weights were trained for a different mount class";
        return false;
    }

    // Reinstate the settings this model was trained under (exposure, binning, gains, …),
    // rather than requiring the user to have already matched them by hand. Pulse algorithm
    // is Standard (0) in the training fingerprint but runtime uses AI, so it is not one of
    // the fields applyFingerprintToOptions() touches.
    const QStringList changes = applyFingerprintToOptions(root["model_fingerprint"].toObject());
    if (!changes.isEmpty())
    {
        m_FingerprintApplied = changes.join("\n");
        qCInfo(KSTARS_EKOS_GUIDE) << "AI weights: applied recorded settings -" << changes.join("; ");
    }

    QJsonObject phys = root["physics"].toObject();
    m_pe_amplitude = phys["pe_amplitude"].toDouble(1.0);
    // pe_harmonic_amplitudes is the offline joint multi-harmonic fit (2026-08-24); older
    // weights.json files that only ever fit the fundamental won't have it, so fall back to
    // [pe_amplitude, 0, 0, 0] -- harmonics 2-4 then cold-start with no prior, same as
    // before this extension existed, rather than failing to load.
    const QJsonArray harmonicsArr = phys["pe_harmonic_amplitudes"].toArray();
    for (int k = 0; k < N_HARMONICS; ++k)
        m_pe_harmonic_amplitude[k] = (k < harmonicsArr.size()) ? harmonicsArr[k].toDouble(0.0) : 0.0;
    if (harmonicsArr.isEmpty())
        m_pe_harmonic_amplitude[0] = m_pe_amplitude;
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

    // v3 residual filter constants -- see m_kfQPerSec's declaration for why these are per-mount
    // loaded values, not hardcoded: everything here so far was derived from one EQ8-class
    // mount's own noise floor and PE character, and nothing guarantees that transfers to a
    // different mount, class, or even a different unit of the same class. Absent values fall
    // back to that one mount's numbers, same defaults the members already carry -- an older
    // weights.json (or one for a mount that hasn't had this analysis run yet) still loads and
    // behaves exactly as it did before this section existed.
    QJsonObject residFilter = root["residual_filter"].toObject();
    m_kfQPerSec           = residFilter["kf_q_per_sec"].toDouble(m_kfQPerSec);
    m_kfR                 = residFilter["kf_r"].toDouble(m_kfR);
    m_kfRActiveMultiplier = residFilter["kf_r_active_multiplier"].toDouble(m_kfRActiveMultiplier);
    m_kfQPerSecDec           = residFilter["kf_q_per_sec_dec"].toDouble(m_kfQPerSecDec);
    m_kfRDec                 = residFilter["kf_r_dec"].toDouble(m_kfRDec);
    m_kfRDecActiveMultiplier = residFilter["kf_r_dec_active_multiplier"].toDouble(m_kfRDecActiveMultiplier);

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

    // Optional v2 Shape Net (see header comment). Absent in every weights.json generated
    // before this build -- m_hasShapeNet stays false and physicsRA() keeps using the
    // unchanged harmonic-sum path, so loading an old-format file is not an error here.
    QJsonObject shapeNet = root["shape_net"].toObject();
    if (!shapeNet.isEmpty() &&
            loadMatrix(shapeNet["w1"].toArray(), m_shapeW1, 16, 3) &&
            loadVector(shapeNet["b1"].toArray(), m_shapeB1, 16) &&
            loadMatrix(shapeNet["w2"].toArray(), m_shapeW2, 16, 16) &&
            loadVector(shapeNet["b2"].toArray(), m_shapeB2, 16) &&
            loadMatrix(shapeNet["w_out"].toArray(), m_shapeWOut, 1, 16) &&
            loadVector(shapeNet["b_out"].toArray(), m_shapeBOut, 1))
    {
        m_shapeNetRefAmplitude = shapeNet["ref_amplitude_px"].toDouble(1.0);
        m_hasShapeNet = true;
        qCInfo(KSTARS_EKOS_GUIDE) << "[AI GUIDER] v2 Shape Net loaded -- physicsRA() will use it "
                                  "instead of the harmonic-sum path. ref_amplitude_px="
                                  << m_shapeNetRefAmplitude;
    }
    else
    {
        m_hasShapeNet = false;
    }

    m_weightsLoaded = true;
    return true;
}

double WormGearGuider::evalShapeNet(double sinPhase, double cosPhase, double altNorm) const
{
    // 3 -> 16 -> 16 -> 1, ReLU, no dropout (inference-mode, matching train_shape_net.py's
    // make_net() with dropout layers omitted -- PyTorch's own .eval() disables them
    // identically). See header comment for the full validation this architecture earned.
    Eigen::Vector3f x(static_cast<float>(sinPhase), static_cast<float>(cosPhase), static_cast<float>(altNorm));
    const Eigen::VectorXf h1 = (m_shapeW1 * x + m_shapeB1).cwiseMax(0.0f);
    const Eigen::VectorXf h2 = (m_shapeW2 * h1 + m_shapeB2).cwiseMax(0.0f);
    const Eigen::VectorXf out = m_shapeWOut * h2 + m_shapeBOut;
    return static_cast<double>(out(0));
}

void WormGearGuider::resetSession(bool forceReset)
{
    m_confidenceRA = 0.0;
    m_confidenceDEC = 0.0;
    m_isStable = false;
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
        m_typicalRMS_ra = 0.5;
        m_typicalRMS_dec = 0.5;
        m_uncorrectedPosRA = 0.0;
        m_uncorrectedPosDEC = 0.0;

        // Warm-start the RLS from the offline-trained physics fit instead of a cold zero,
        // independently per harmonic (2026-08-24: verified real, comparable-power content
        // through harmonic 4 on this mount -- see file header). For harmonic k, theta[2k-2]
        // is sin_k, theta[2k-1] is cos_k, and phase_k = atan2(cos_k, sin_k) -- i.e. sin_k =
        // A_k*cos(phase_k), cos_k = A_k*sin(phase_k). Seeding each harmonic's phase at
        // exactly 0 therefore means sin_k=A_k, cos_k=0: the seed vector lands exactly on
        // the sin_k axis, so THAT axis is the amplitude/radial direction for harmonic k and
        // cos_k is its phase/tangential direction, with no rotation needed to tell them
        // apart -- same convention as the original single-harmonic design, just repeated
        // per harmonic.
        // Diagnostic escape hatch (2026-08-27): AI_GUIDER_COLD_START=1 skips the warm-start
        // below entirely, seeding theta at zero with a fully uninformative P instead. Exists
        // to answer a live-testing question the warm-started filter can't: is the ~0.3-0.4px
        // amplitude it reports each night actual freshly-detected live signal, or mostly the
        // offline prior riding along because tonight's real signal is too weak to move it much?
        // A cold start has nothing to ride along on -- whatever amplitude it converges to (or
        // doesn't) on live data alone is a direct answer. Not meant to ship enabled; remove once
        // that question is settled.
        const bool coldStart = qEnvironmentVariableIsSet("AI_GUIDER_COLD_START");
        m_rls_theta.setZero();
        if (!coldStart)
        {
            for (int k = 0; k < N_HARMONICS; ++k)
                m_rls_theta(2 * k) = m_pe_harmonic_amplitude[k];
            m_rls_theta(N_HARMONICS * 2) = m_d_ra_extra;
        }

        // Anisotropic, not uniform, confidence -- same reasoning as the original
        // single-harmonic design, applied to each harmonic independently. Each harmonic's
        // amplitude (and the linear drift term) is a real mechanical property of THIS
        // mount, worth trusting moderately so a cold, weak per-frame signal doesn't force
        // climbing back to it from scratch every session. Each harmonic's OWN phase
        // deserves none of that trust: like the fundamental's, it depends on the worm's
        // absolute rotational position at session start, unrecoverable offline -- seeding
        // it at 0 is an arbitrary guess, not evidence, and NOT assumed to be phase-locked
        // to the fundamental (a general periodic mechanical waveform has no reason its
        // harmonics share the fundamental's phase). Giving a harmonic's cos_k (phase) the
        // same moderate confidence as its sin_k (amplitude) would make the filter resist
        // correcting a guess that deserves zero trust -- worst case (true phase near π,
        // i.e. exactly wrong), that's actively worse than a cold start, not just slower to
        // help. So every cos_k keeps the fully uninformative default (corrects as fast as a
        // cold start would) while every sin_k and the drift term v get the moderate prior.
        // The constant-offset term C has no offline prior either and stays uninformative.
        m_rls_P = Eigen::Matrix<double, N_STATES, N_STATES>::Identity() * 100.0;
        if (!coldStart)
        {
            for (int k = 0; k < N_HARMONICS; ++k)
                m_rls_P(2 * k, 2 * k) = 4.0;
            m_rls_P(N_HARMONICS * 2, N_HARMONICS * 2) = 4.0; // drift term v
        }

        // Re-arm the drift regressor's time origin (see m_rlsTimeOrigin's declaration) so the
        // next updatePhase() call re-anchors to whatever t_session_sec it's handed next, instead
        // of carrying on from a stale origin that belongs to the state we just discarded.
        m_rlsTimeOrigin = -1.0;

        // Reset the v3 residual filter alongside everything else -- a fresh session (or a
        // forced reset, e.g. meridian flip) means whatever residual pattern the filter had
        // converged to no longer applies. P resets to this weights file's own loaded m_kfR
        // (uninformative) so the filter starts fully open to the next real observation, same
        // spirit as RLS's own reset above.
        m_kfResidualRate = 0.0;
        m_kfP = m_kfR;

        // DEC's tracker rides along on the same trigger (forceReset covers meridian flips, which
        // change DEC's mechanical relationship to gravity/pier side too, not just RA's phase) --
        // harmless to also reset it on the rarer m_pe_period<=0 case, just means it re-learns
        // fast rather than needing a separate gating condition for no real benefit.
        m_kfResidualRateDec = 0.0;
        m_kfPDec = m_kfRDec;
    }
}

void WormGearGuider::updateResidualKF(double measuredRate_px_s, double shapeNetRate_px_s, double dt,
                                      bool activeContaminated)
{
    if (dt <= 0.0) return;

    // Predict: pure random walk (F=1), covariance grows with elapsed time.
    m_kfP += m_kfQPerSec * dt;

    // Update: innovation is how much of the measured rate the Shape Net's own prediction plus
    // the filter's current residual estimate still fails to explain. See m_kfRActiveMultiplier's
    // comment for why this frame's measurement noise is inflated (not the update skipped
    // entirely) when the just-applied pulse included this guider's own AI prediction.
    const double innovation = (measuredRate_px_s - shapeNetRate_px_s) - m_kfResidualRate;
    const double R = activeContaminated ? (m_kfR * m_kfRActiveMultiplier) : m_kfR;
    const double S = m_kfP + R;
    const double K = m_kfP / S;
    m_kfResidualRate += K * innovation;
    m_kfP = (1.0 - K) * m_kfP;
}

void WormGearGuider::updateResidualKFDec(double measuredRate_px_s, double physicsDecRate_px_s, double dt,
                                         bool activeContaminated)
{
    if (dt <= 0.0) return;

    m_kfPDec += m_kfQPerSecDec * dt;

    const double innovation = (measuredRate_px_s - physicsDecRate_px_s) - m_kfResidualRateDec;
    const double R = activeContaminated ? (m_kfRDec * m_kfRDecActiveMultiplier) : m_kfRDec;
    const double S = m_kfPDec + R;
    const double K = m_kfPDec / S;
    m_kfResidualRateDec += K * innovation;
    m_kfPDec = (1.0 - K) * m_kfPDec;
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
    // Hold the AI out of an unstable loop. The elapsed-time check (not just a frame
    // count) is what actually protects the RA phase estimate from being trusted before
    // enough real worm cycles have been observed -- see MIN_PE_CYCLES's comment.
    const bool phaseTimeReady = (m_pe_period <= 0.0) || (frame.t_session_sec >= MIN_PE_CYCLES * m_pe_period);
    out.valid      = (m_frameCount > warmupFrames()) && phaseTimeReady && m_isStable;
    out.confidence_ra  = m_confidenceRA;
    out.confidence_dec = m_confidenceDEC;

    // Logged regardless of out.valid so the AI debug CSV can show the raw RLS state
    // through warmup/fallback, not just while the model is actively driving pulses.
    out.rls_sin_coeff  = m_rls_theta(0);
    out.rls_cos_coeff  = m_rls_theta(1);
    out.rls_drift_rate = m_rls_theta(2);
    out.rls_offset     = m_rls_theta(3);

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
                            double /*ra_pulse_px*/, double /*dec_pulse_px*/,
                            bool ra_pulse_has_ai, bool dec_pulse_has_ai)
{
    // Freeze RA's online phase/amplitude adaptation while the just-applied RA pulse included
    // this guider's own prediction. The pulse backout in gmath.cpp assumes one fixed linear
    // ms/arcsec calibration factor regardless of who commanded the pulse; any residual from
    // that approximation is correlated with the same sin/cos phase basis the RLS fits on (and
    // that the AI prediction itself reads out of), so without this guard the RLS partially
    // re-learns its own leftover prediction error -- a closed-loop system-identification
    // bias, not ordinary measurement noise.
    //
    // Verified 2026-08-19: PE amplitude/phase held stable (0.15-0.42", 2.7-3.0 rad) through
    // ~30 minutes of Shadow-mode guiding (Standard pulses only -- this loop never closes in
    // Shadow), then diverged to amplitude 1.38" and a ~pi phase flip within ~10 minutes of
    // switching RA to AI Guider. 88.4% of RA frames in that Active window had the AI
    // response term opposing the proportional term's sign, and mean net pulse dropped from
    // 82.5ms to 35.7ms -- consistent with a corrupted phase estimate actively fighting the
    // real-time correction rather than just being a weak/noisy one.
    //
    // DEC now has online-adaptive state too (m_kfResidualRateDec, added 2026-08-28 -- see its
    // declaration), but this accumulator isn't what feeds it (that filter reads the raw
    // uncorrected_drift_dec_px_delta directly, below), so it's left unguarded here same as
    // before; the new filter's own closed-loop-bias defense lives in updateResidualKFDec()'s
    // activeContaminated handling instead of a freeze at this accumulation step.
    if (!ra_pulse_has_ai)
        m_uncorrectedPosRA += uncorrected_drift_ra_px_delta;
    m_uncorrectedPosDEC += uncorrected_drift_dec_px_delta;

    // Confidence is intentionally NOT frozen alongside the RLS state above (fixed
    // 2026-08-22, caught before this reached wider testing): theta is a recursively
    // accumulated state that can genuinely diverge from contaminated input, which is the
    // real bug the freeze above exists to prevent. Confidence is not that kind of state --
    // it is bounded [0,1] and rate-limited (CONF_RISE_PER_FRAME), recomputed fresh each
    // frame from a windowed innovation deque, so it cannot run away the same way. Freezing
    // it too bought no protection against divergence; it only meant that once Active
    // engaged, RA lost its only defense against its OWN prediction quality degrading later
    // in a session (temperature drift, wind, changing mount behavior) -- DEC's confidence
    // keeps updating live and can fall to near-zero if DEC goes bad, but a frozen RA
    // confidence would keep trusting a since-degraded RA prediction indefinitely. Computing
    // it from the same (still slightly contaminated) signal the RLS freeze above protects
    // against is an accepted, pre-existing tradeoff -- the same one DEC's confidence has
    // always operated under -- not a new risk introduced by unfreezing this.
    if (m_hasLastPred)
    {
        const double innov_ra  = uncorrected_drift_ra_px_delta - m_lastPredDriftRA;
        const double innov_dec = uncorrected_drift_dec_px_delta - m_lastPredDriftDEC;
        updateConfidence(innov_ra, innov_dec, snr);
    }

    if (!ra_pulse_has_ai)
        updatePhase(m_uncorrectedPosRA, m_lastSessionSec);

    // v3 residual filter: only meaningful on the Shape Net path (see its declaration).
    // measuredRate is this frame's actual drift rate; shapeNetRate is what the existing physics
    // model (Shape Net + refraction + RLS drift term) already predicts for it -- the filter
    // tracks whatever's still left over between the two. Deliberately NOT gated behind
    // ra_pulse_has_ai the way updatePhase() above is -- see m_kfRActiveMultiplier's comment for
    // why this filter instead keeps updating through Active mode with inflated measurement
    // noise on contaminated frames, rather than freezing entirely.
    if (m_hasShapeNet && m_lastDt > 0.0)
    {
        const double measuredRate = uncorrected_drift_ra_px_delta / m_lastDt;
        const double altitude_deg = m_lastAltRad * 180.0 / M_PI;
        const double shapeNetRate = physicsRABase(m_lastSessionSec, altitude_deg);
        updateResidualKF(measuredRate, shapeNetRate, m_lastDt, ra_pulse_has_ai);
    }

    // DEC online drift tracker: see m_kfResidualRateDec's declaration. Always runs (no
    // hasShapeNet-style gate -- there's no offline DEC shape to depend on), same
    // inflate-don't-freeze defense as the RA filter against AI-driven-frame closed-loop bias.
    if (m_lastDt > 0.0)
    {
        const double measuredRateDec = uncorrected_drift_dec_px_delta / m_lastDt;
        const double altitude_deg = m_lastAltRad * 180.0 / M_PI;
        const double physicsDecRate = physicsDECBase(altitude_deg, m_lastParallacticAngleDeg);
        updateResidualKFDec(measuredRateDec, physicsDecRate, m_lastDt, dec_pulse_has_ai);
    }
}

QString WormGearGuider::stateString() const
{
    // A/φ remain the fundamental's (theta indices 0,1), matching what runMLP()'s phase
    // basis and every prior night's telemetry used -- Ah2..Ah4 are the additional
    // harmonics' amplitudes, appended rather than replacing the existing fields so older
    // log-parsing tools/scripts don't break on this string's format.
    const double amp2 = std::hypot(m_rls_theta(2), m_rls_theta(3));
    const double amp3 = std::hypot(m_rls_theta(4), m_rls_theta(5));
    const double amp4 = std::hypot(m_rls_theta(6), m_rls_theta(7));
    return QString("WormGear A=%1 T=%2 φ=%3 Ah2=%4 Ah3=%5 Ah4=%6 confRA=%7 confDEC=%8")
           .arg(m_pe_amplitude, 0, 'f', 2)
           .arg(m_pe_period, 0, 'f', 0)
           .arg(m_pe_phase, 0, 'f', 3)
           .arg(amp2, 0, 'f', 2)
           .arg(amp3, 0, 'f', 2)
           .arg(amp4, 0, 'f', 2)
           .arg(m_confidenceRA, 0, 'f', 2)
           .arg(m_confidenceDEC, 0, 'f', 2);
}

double WormGearGuider::physicsRA(double t_sec, double altitude_deg) const
{
    const double basePrediction = physicsRABase(t_sec, altitude_deg);
    // v3 residual filter only layers onto the Shape Net path -- see its declaration for why;
    // the v1 harmonic-sum fallback is left exactly as validated, no residual influence.
    return m_hasShapeNet ? (basePrediction + m_kfResidualRate) : basePrediction;
}

double WormGearGuider::physicsRABase(double t_sec, double altitude_deg) const
{
    const double omega = 2.0 * M_PI / m_pe_period;

    double pe_rate = 0.0;
    if (m_hasShapeNet)
    {
        // v2 path: the Shape Net was trained to predict detrended POSITION at a given phase
        // (see train_shape_net.py's pooled, phase-aligned target), not rate directly -- so
        // pe_rate here is a numerical (finite-difference) derivative of the net's own output,
        // exactly as a physical rate is the time-derivative of position. FD_DT is small
        // relative to m_pe_period (~200s) so the finite-difference error is negligible; it is
        // NOT the guide loop's actual dt (which varies frame to frame and isn't known inside
        // this const method).
        constexpr double FD_DT = 0.5;
        // Same phase convention as runMLP()'s phase basis (theta(1) as atan2's y, theta(0) as
        // its x) -- reusing that exact, already-validated formula rather than re-deriving it.
        const double phi_rls = std::atan2(m_rls_theta(1), m_rls_theta(0));
        const double phase_now = omega * t_sec + phi_rls;
        const double phase_next = omega * (t_sec + FD_DT) + phi_rls;

        const double online_amp = std::hypot(m_rls_theta(0), m_rls_theta(1));
        const double amp_scale = (m_shapeNetRefAmplitude > 1e-6) ? online_amp / m_shapeNetRefAmplitude : 1.0;

        const double alt_norm = std::clamp(altitude_deg, m_fit_alt_min, m_fit_alt_max) / 90.0;
        const double pos_now  = evalShapeNet(std::sin(phase_now), std::cos(phase_now), alt_norm) * amp_scale;
        const double pos_next = evalShapeNet(std::sin(phase_next), std::cos(phase_next), alt_norm) * amp_scale;
        pe_rate = (pos_next - pos_now) / FD_DT;
    }
    else
    {
        // v1 path (unchanged): derivative of position model
        // y(t) = sum_k [sin_k*sin(k*wt) + cos_k*cos(k*wt)] + v*t + C
        // y'(t) = sum_k [sin_k*(k*w)*cos(k*wt) - cos_k*(k*w)*sin(k*wt)] + v -- same
        // per-harmonic derivative form as the original single-harmonic model, summed over
        // each independently RLS-fit harmonic (see file header comment for why more than one
        // harmonic is real here, not just the offline trainer's k_ref/refraction mismatch
        // this comment used to describe alone).
        for (int k = 1; k <= N_HARMONICS; ++k)
        {
            const double k_omega = k * omega;
            const double sin_k = m_rls_theta(2 * (k - 1));
            const double cos_k = m_rls_theta(2 * (k - 1) + 1);
            pe_rate += sin_k * k_omega * std::cos(k_omega * t_sec) - cos_k * k_omega * std::sin(k_omega * t_sec);
        }
    }

    // Refraction term, matching train_worm_gear.py's _physics_drift() exactly (same sign,
    // same 1/cos^2(alt) form) -- this is what the offline-trained residual MLP's target
    // was actually computed against (target = true_drift - phys_drift, phys_drift
    // including this term), so omitting it here means the MLP's output is judged against
    // a physics baseline that was never actually served at runtime: a systematic,
    // altitude-dependent gap the MLP was never given the input range/capacity to fully
    // absorb on its own. Clamped to the fit's altitude range for the same reason
    // physicsDEC() below clamps -- k_ref/cos^2(alt) is a real refraction effect only
    // within the range it was fit over; extrapolating it unclamped toward the horizon
    // blows up.
    const double alt_rad = std::clamp(altitude_deg, m_fit_alt_min, m_fit_alt_max) * M_PI / 180.0;
    const double cos_alt = std::cos(alt_rad);
    const double refraction_rate = (std::abs(cos_alt) < 1e-4) ? 0.0 : m_k_ref / (cos_alt * cos_alt);

    return pe_rate + refraction_rate + m_rls_theta(N_HARMONICS * 2);
}

double WormGearGuider::physicsDEC(double altitude_deg, double parallactic_angle_deg) const
{
    // See m_kfResidualRateDec's declaration: the online drift tracker always runs (it has no
    // "hasShapeNet"-style gate -- there's no offline DEC model to depend on), so it's added
    // unconditionally here, unlike physicsRA()'s Shape-Net-only residual.
    return physicsDECBase(altitude_deg, parallactic_angle_deg) + m_kfResidualRateDec;
}

double WormGearGuider::physicsDECBase(double altitude_deg, double parallactic_angle_deg) const
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

    // See m_rlsTimeOrigin's declaration: anchor on first use after a reset so the drift term's
    // regressor stays a small, bounded elapsed-time-since-reset instead of the raw session clock.
    if (m_rlsTimeOrigin < 0.0)
        m_rlsTimeOrigin = t_session_sec;
    const double t_rel_sec = t_session_sec - m_rlsTimeOrigin;

    const double omega = 2.0 * M_PI / m_pe_period;

    // Position-domain regressor for y(t) = sum_k [sin_k*sin(k*wt) + cos_k*cos(k*wt)] + v*t + C
    // -- same structure as the original single-harmonic fit, with one (sin,cos) pair per
    // harmonic instead of just the fundamental's. Each harmonic's own pair is fit
    // independently (not constrained to any fixed relationship to the fundamental's phase
    // -- see file header comment for why that's the right assumption here).
    Eigen::Matrix<double, N_STATES, 1> x;
    for (int k = 1; k <= N_HARMONICS; ++k)
    {
        x(2 * (k - 1))     = std::sin(k * omega * t_session_sec);
        x(2 * (k - 1) + 1) = std::cos(k * omega * t_session_sec);
    }
    x(N_HARMONICS * 2)     = t_rel_sec;
    x(N_HARMONICS * 2 + 1) = 1.0;

    // Standard RLS Update for Position
    double h_P_h = x.transpose() * m_rls_P * x;
    double denom = RLS_LAMBDA + h_P_h;

    Eigen::Matrix<double, N_STATES, 1> K = Eigen::Matrix<double, N_STATES, 1>::Zero();
    if (denom > 1e-6)
        K = (m_rls_P * x) / denom;

    double prediction = x.transpose() * m_rls_theta;
    double error = uncorrected_position_px - prediction;

    m_rls_theta = m_rls_theta + K * error;
    m_rls_P = (m_rls_P - K * x.transpose() * m_rls_P) / RLS_LAMBDA;

    // Extract physical parameters for state reporting and MLP compatibility -- fundamental
    // (harmonic 1, indices 0/1) only, matching what these fields have always represented.
    m_pe_amplitude = std::hypot(m_rls_theta(0), m_rls_theta(1));
    m_pe_phase = std::atan2(m_rls_theta(1), m_rls_theta(0));

    // Divergence backstop: this runs unattended overnight, so a bad outlier innovation (a
    // single frame's uncorrected_position_px jumping far from prediction, e.g. right after a
    // reset when m_rls_P is still large/uninformative -- see m_rlsTimeOrigin's fix for the one
    // confirmed way this happened on 2026-08-27) shouldn't be allowed to leave the fundamental
    // amplitude parked at some multiple of the real worm PE for the rest of the night. A real
    // worm PE amplitude has no mechanism to legitimately be more than a few times the
    // offline-fit prior; treat blowing well past that as evidence of a corrupted fit, not a
    // real reading, and recover the same way a meridian flip does -- a full resetSession(true),
    // not a partial patch, since a divergence this large means every RLS-derived quantity
    // (phase, other harmonics, drift) is suspect together, not just the fundamental.
    const double sanePriorAmplitude = std::max(m_pe_harmonic_amplitude[0], 0.1);
    if (m_pe_amplitude > 20.0 * sanePriorAmplitude)
    {
        qCWarning(KSTARS_EKOS_GUIDE) << "[AI GUIDER] RLS fundamental amplitude diverged ("
                                     << m_pe_amplitude << "px vs trained prior"
                                     << sanePriorAmplitude << "px) -- forcing a hard reset.";
        resetSession(true);
        return;
    }
    m_d_ra_extra = m_rls_theta(N_HARMONICS * 2);
}

void WormGearGuider::updateConfidence(double innovRA, double innovDec, double snr)
{
    // Both axes always update here, even while RA's RLS phase adaptation is frozen in
    // update() above -- see the comment there for why confidence and the RLS state are
    // deliberately NOT frozen together (2026-08-22).
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
    // m_isStable still gates on the JOINT innovation -- it answers "is the AI subsystem
    // operational at all" (out.valid), a coarser question than per-axis trust. One axis's
    // prediction quality is handled entirely by that axis's own confidence below; the
    // combined RMS here only decides whether the AI runs at all this frame, for either axis.
    const double innov_rms = std::sqrt((ra_rms * ra_rms + dec_rms * dec_rms) / 2.0);
    const double innov_arcsec_per_sec = innov_rms * m_lastPixelScale / std::max(0.5, m_lastDt);
    m_isStable = innov_arcsec_per_sec < INSTABILITY_ARCSEC_PER_SEC &&
                 m_innovRA.size() >= INNOV_WINDOW / 2;

    double snr_factor = std::min(1.0, (snr - 10.0) / 20.0);
    snr_factor = std::max(0.0, snr_factor);

    // Same elapsed-time gate as predict()'s out.valid -- m_lastSessionSec lags the
    // current frame by one cycle (update() runs before predict() each frame in
    // gmath.cpp), negligible against a multi-cycle threshold.
    const bool phaseTimeReady = (m_pe_period <= 0.0) || (m_lastSessionSec >= MIN_PE_CYCLES * m_pe_period);

    double warmup_factor = 0.0;
    if (m_frameCount > warmupFrames() && phaseTimeReady)
    {
        // Start at 30% confidence immediately (since we already watched 30 frames),
        // and smoothly ramp the remaining 70% over the next 20 frames.
        warmup_factor = 0.3 + 0.7 * std::min(1.0, static_cast<double>(m_frameCount - warmupFrames()) / 20.0);
    }

    // Per-axis: one axis's prediction quality says nothing about the other's (verified
    // 2026-08-18 -- a session where RA's residual MLP tracked well while DEC's, trained on
    // a fraction of the samples after backlash-poisoned sessions get zero-weighted, output
    // something close to noise; a single shared confidence let DEC's noise ride on RA's
    // trust and actively fight the correct proportional response). Same warmup/SNR terms
    // (not axis-specific concepts) but independent adaptive RMS baseline, Lorentzian
    // quality, and rise-limited confidence per axis.
    //
    // Phase-convergence penalty (2026-08-24, generalized to all N_HARMONICS the same day):
    // m_rls_P(2k-1,2k-1) is the RLS covariance for harmonic k's cos_k, its phase axis --
    // seeded fully uninformative (100.0) since phase can't be known offline, and only
    // shrinks once real data actually constrains it. physicsRA() now sums all N_HARMONICS
    // independently-fit harmonics, so a still-unconverged phase on ANY of them (not just
    // the fundamental) feeds a wrong term into the prediction -- take the worst (max) of
    // the four rather than just the fundamental's. Innovation-based confidence above
    // doesn't catch this on its own: a prediction built from a bad phase can still look
    // self-consistent frame to frame, since the "error" it's judged against is derived from
    // the model's own (equally wrong) prediction.
    //
    // IMPORTANT CAVEAT, verified 2026-08-24 by replaying the live RLS offline against real
    // captured data: P(k,k) is NOT a reliable convergence signal on its own -- it collapsed
    // to near-zero (falsely "confident") within ~150-190s while the actual theta estimate
    // was still visibly swinging through t=290s+. This penalty is kept as a secondary,
    // partial signal, but the REAL protection against trusting an unconverged phase is the
    // elapsed-time gate above (MIN_PE_CYCLES) and, more fundamentally, the multi-harmonic
    // model itself -- a single-sinusoid model couldn't converge at all against this mount's
    // real harmonic content (see file header), which is the deeper reason this covariance
    // signal alone was never going to be enough.
    double phase_uncertainty = 0.0;
    for (int k = 1; k <= N_HARMONICS; ++k)
        phase_uncertainty = std::max(phase_uncertainty, m_rls_P(2 * (k - 1) + 1, 2 * (k - 1) + 1) / 100.0);
    phase_uncertainty = std::clamp(phase_uncertainty, 0.0, 1.0);
    const double phase_quality = 1.0 / (1.0 + phase_uncertainty * phase_uncertainty);

    auto axisConfidence = [&](double rms, double &typicalRMS, double &confidence) -> void
    {
        const double axis_arcsec_per_sec = rms * m_lastPixelScale / std::max(0.5, m_lastDt);

        if (m_frameCount <= warmupFrames())
            typicalRMS = std::max(0.05, rms);
        else if (rms < typicalRMS * 1.5)
            typicalRMS = 0.99 * typicalRMS + 0.01 * rms;

        const double error_ratio = rms / (typicalRMS + 1e-6);

        // Lorentzian confidence instead of exponential.
        // exp(-r) gives ~0.37 at r=1, crushing the AI contribution.
        // 1/(1+r²) gives ~0.50 at r=1, allowing meaningful feed-forward.
        double prediction_quality = 1.0 / (1.0 + error_ratio * error_ratio);

        // Collapse on absolute innovation too; the ratio is blind to a poisoned baseline.
        const double instability = axis_arcsec_per_sec / INSTABILITY_ARCSEC_PER_SEC;
        if (instability > 1.0)
            prediction_quality /= instability * instability;

        // Falls freely, rebuilds slowly: a briefly quiet star must not restore full trust.
        const double target = std::clamp(warmup_factor * snr_factor * prediction_quality * phase_quality, 0.0, 1.0);
        confidence = std::min(target, confidence + CONF_RISE_PER_FRAME);
    };

    axisConfidence(ra_rms, m_typicalRMS_ra, m_confidenceRA);
    axisConfidence(dec_rms, m_typicalRMS_dec, m_confidenceDEC);
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
    const bool phaseTimeReady = (m_pe_period <= 0.0) || (m_lastSessionSec >= MIN_PE_CYCLES * m_pe_period);
    out.valid      = (m_frameCount > warmupFrames()) && phaseTimeReady && m_isStable;
    out.confidence_ra  = m_confidenceRA;  // Use last known confidence
    out.confidence_dec = m_confidenceDEC;

    // We do NOT update m_lastPredDriftRA because there's no actual frame
    // to compare it to in update(). We just return the prediction.
    // Convert from pixels to arcseconds using the cached pixel_scale from the last real frame.
    out.ra_correction_arcsec  = (phys_ra + mlp_out[0]) * m_lastPixelScale;
    out.dec_correction_arcsec = (phys_dec + mlp_out[1]) * m_lastPixelScale;

    out.physics_ra_arcsec  = phys_ra * m_lastPixelScale;
    out.physics_dec_arcsec = phys_dec * m_lastPixelScale;
    out.mlp_ra_arcsec      = mlp_out[0] * m_lastPixelScale;
    out.mlp_dec_arcsec     = mlp_out[1] * m_lastPixelScale;

    out.rls_sin_coeff  = m_rls_theta(0);
    out.rls_cos_coeff  = m_rls_theta(1);
    out.rls_drift_rate = m_rls_theta(2);
    out.rls_offset     = m_rls_theta(3);

    return out;
}
