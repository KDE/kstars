#pragma once
/*
 * worm_gear_guider.h — Physics-Informed NN guider for worm-gear mounts
 *
 * Architecture: Physics layer (deterministic multi-harmonic PE + drift model) +
 *               small residual MLP (~192 parameters).
 *
 * The physics layer models:
 *   PE_ra(t) = sum_{k=1}^{N_HARMONICS} [sin_k * sin(k*2π*t/T) + cos_k * cos(k*2π*t/T)]
 *                                                 [learned T offline; sin_k/cos_k online via RLS]
 *   drift_ra(alt) = k_ref / cos²(alt)             [refraction, learned k_ref]
 *   drift_dec     = d_polar + k_ref_dec * sin(q) / cos²(alt) [polar + refraction]
 *
 * Each harmonic's (sin_k, cos_k) pair is fit independently -- verified 2026-08-24 on real
 * free-drift data that a worm-gear mount's PE is NOT a clean single sinusoid: a joint
 * multi-harmonic batch fit found harmonics 1, 2, and 4 all carrying comparable, highly
 * significant power (SNR 9-13), with residual dropping meaningfully through the 4th
 * harmonic before flattening at the 5th. A single-sinusoid model (the old 4-state RLS)
 * could never converge cleanly against real data with this much harmonic content, no
 * matter how it was tuned -- an independent batch least-squares fit over the same data
 * showed the identical non-convergence, ruling out an online-algorithm-specific cause.
 *
 * The MLP's phase-basis input features (sin(kφ)/cos(kφ) for k=1..4, all derived from the
 * FUNDAMENTAL's phase only) are unchanged by this -- it still handles whatever the physics
 * layer's multi-harmonic fit doesn't capture (non-phase-locked harmonic content, backlash,
 * etc.), just starting from a much smaller residual now that harmonics 2-4 are handled
 * explicitly by the physics layer instead of implicitly assumed away.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "mount_guider.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <Eigen/Core>
#include <QJsonObject>

class WormGearGuider : public MountSpecificGuider
{
    public:
        WormGearGuider();
        ~WormGearGuider() override = default;

        bool        loadWeights(const QString &weightsPath) override;
        void        resetSession(bool forceReset = false) override;
        GuideOutput predict(const GuideFrameData &frame) override;
        GuideOutput darkPredict(double dt_sec) override;
        void        update(double ra_error_px, double dec_error_px, double uncorrected_drift_ra_px, double uncorrected_drift_dec_px,
                           double snr, double ra_pulse_px, double dec_pulse_px,
                           bool ra_pulse_has_ai, bool dec_pulse_has_ai) override;
        double      confidenceRA() const override
        {
            return m_confidenceRA;
        }
        double      confidenceDEC() const override
        {
            return m_confidenceDEC;
        }
        int         warmupFrames() const override
        {
            return 50;
        }
        bool        isLoaded() const override
        {
            return m_weightsLoaded;
        }
        QString     stateString() const override;

    private:
        // ── Physics layer parameters (loaded from weights JSON) ──────────────
        static constexpr int N_HARMONICS = 4;
        static constexpr int N_STATES = N_HARMONICS * 2 + 2; ///< 4x(sin,cos) + drift + offset
        /// Per-harmonic amplitude prior from the offline joint multi-harmonic fit (pixels),
        /// index 0 = fundamental. Falls back to [pe_amplitude, 0, 0, 0] when loading an
        /// older weights.json that only ever fit the fundamental (see loadWeights()).
        static std::array<double, N_HARMONICS> m_pe_harmonic_amplitude;
        /// Fundamental's amplitude, kept as its own field (== m_pe_harmonic_amplitude[0])
        /// since stateString() and the live RLS-derived value it also holds predate the
        /// multi-harmonic extension and are cheaper to keep than thread through everywhere.
        static double m_pe_amplitude;  ///< Worm PE amplitude, fundamental only (pixels)
        double m_pe_period     { 480.0 };///< Worm PE period (seconds) — fixed after FFT fit
        double m_k_ref         { 0.0 };  ///< RA refraction coefficient, k_ref/cos²(alt) (px/s); see physicsRA()
        static double m_d_ra_extra;    ///< Continuous RA drift rate (pixels/second)
        double m_d_polar       { 0.0 };  ///< Polar drift rate (pixels/second)
        double m_k_ref_dec     { 0.0 };  ///< DEC Refraction coefficient
        double m_fit_alt_min   { 35.0 }; ///< Altitude range the drift fit is valid for
        double m_fit_alt_max   { 65.0 };

        // ── Online phase estimation (N_STATES-State Position RLS) ─────────────
        // theta = [sin_1, cos_1, sin_2, cos_2, sin_3, cos_3, sin_4, cos_4, v, C] --
        // harmonic k occupies indices 2*(k-1), 2*(k-1)+1; drift/offset are the last two.
        // Harmonic 1 (indices 0,1) is what the rest of the file (runMLP's phase basis,
        // stateString, the phase-uncertainty confidence penalty) already reads via
        // m_rls_theta(0)/m_rls_theta(1) -- unchanged by the extension to more harmonics.
        static Eigen::Matrix<double, N_STATES, 1> m_rls_theta;
        static Eigen::Matrix<double, N_STATES, N_STATES> m_rls_P; ///< RLS covariance
        static constexpr double RLS_LAMBDA = 0.999; ///< RLS forgetting factor
        // Origin (in t_session_sec) the drift regressor (x(N_HARMONICS*2), below) is measured
        // from -- NOT the raw, ever-growing session clock. t_session_sec keeps counting up for
        // the whole night (it isn't part of what resetSession() clears), so using it directly
        // as a linear regressor made the fit numerically worse the longer a session ran: after
        // a couple of hours the drift term's regressor was O(1e4) while every harmonic
        // regressor stayed O(1), so a tiny error in the fitted drift RATE produced a huge
        // predicted-position error once multiplied back out by that same O(1e4) elapsed time --
        // which then fed back into the next update as a huge "error", compounding. Confirmed
        // 2026-08-27 in live Shadow-mode testing: after ~2h of continuous guiding the fundamental
        // amplitude estimate ran away from a trained ~0.43px to >27px, persisting even through a
        // resetSession(true) hard reset (meridian flip) because that reset cleared theta/P but
        // not the underlying session clock, so the very next update still handed it a
        // multi-thousand-second regressor with a freshly-uninformative (large) P. Sentinel -1.0
        // means "not yet anchored"; updatePhase() lazily captures the current t_session_sec into
        // this on the first call after any hard reset, then always regresses on
        // (t_session_sec - m_rlsTimeOrigin) instead -- bounded, small, and resets together with
        // theta/P by construction instead of needing resetSession()'s signature to carry time.
        static double m_rlsTimeOrigin;
        static double m_uncorrectedPosRA;
        static double m_uncorrectedPosDEC;

        // ── Normalization constants ──────────────────────────────────────────
        static double m_pe_phase;  ///< Current PE phase estimate (radians)
        double m_alt_scale      { 90.0 };
        double m_snr_scale      { 100.0 };
        double m_pulse_scale_ms { 1000.0 };
        double m_dt_scale       { 2.0 };

        // ── Residual MLP weights (loaded from weights JSON) ──────────────────
        // Architecture: 15 inputs → 32 hidden → 16 hidden → 2 outputs
        // Input: [alt_norm, sin(φ), cos(φ), sin(2φ), cos(2φ), sin(3φ), cos(3φ), sin(4φ), cos(4φ),
        //         snr_norm, ra_pulse_norm, dec_pulse_norm, dt_norm, q_norm, pier_side_norm]
        Eigen::MatrixXf m_w1;
        Eigen::VectorXf m_b1;
        Eigen::MatrixXf m_w2;
        Eigen::VectorXf m_b2;
        Eigen::MatrixXf m_w_out;
        Eigen::VectorXf m_b_out;

        // ── v2 offline Shape Net (optional, loaded from weights JSON) ────────────
        // AI Guider v2 (/home/stellarmate/ai_guider_v2_architecture.md, Stage 1): a small
        // net trained offline on multi-night POOLED, phase-aligned free-drift data --
        // predicts detrended RA POSITION (not rate) at a given [sin(phase), cos(phase),
        // alt_norm], replacing the online RLS harmonic sum in physicsRA() when present.
        // Validated 2026-08-26 via leave-one-night-out CV + a night-level cluster bootstrap:
        // beats a fixed 4-harmonic Fourier fit on the same held-out data by +0.0658 R^2 (95%
        // CI [0.0323, 0.1095], excludes zero), winning in 7/7 individual held-out nights; the
        // Fourier fit's own cross-night R^2 was negative (fails to generalize to a night it
        // wasn't fit on at all). That validation was entirely offline (held-out historical
        // data) -- this is its first appearance in the live runtime, unvalidated in actual
        // closed-loop guiding. Architecture: 3 -> 16 -> 16 -> 1 (ReLU, no dropout at
        // inference), the size that won a sweep from 41 to 673 params on the same data.
        //
        // Deliberately OPTIONAL and backward-compatible: m_hasShapeNet stays false (and
        // physicsRA() falls back to the existing, unchanged harmonic-sum path) unless the
        // loaded weights.json has a valid "shape_net" section -- the currently-deployed
        // weights file has none, so building this in changes nothing about current behavior
        // until a Shape-Net-trained weights file is explicitly generated and loaded.
        //
        // Deliberately reuses the EXISTING online RLS state (m_rls_theta, unchanged, still
        // fit by updatePhase() exactly as before) for online calibration instead of adding
        // new tracked state: the fundamental's already-derived phase/amplitude (theta(0),
        // theta(1)) become the Shape Net's phase input and an amplitude scale factor -- the
        // "reused RLS calibration" the architecture doc's section 6 called for, without
        // touching the already-validated tracking math itself.
        bool m_hasShapeNet { false };
        Eigen::MatrixXf m_shapeW1;   ///< 16x3
        Eigen::VectorXf m_shapeB1;   ///< 16
        Eigen::MatrixXf m_shapeW2;   ///< 16x16
        Eigen::VectorXf m_shapeB2;   ///< 16
        Eigen::MatrixXf m_shapeWOut; ///< 1x16
        Eigen::VectorXf m_shapeBOut; ///< 1
        /// Fundamental amplitude (px) the Shape Net's training data itself carried, recorded
        /// at export time -- physicsRA() scales the net's raw output by
        /// (currently-tracked-online-amplitude / this), so the offline-trained shape adapts
        /// to tonight's actual PE strength without retraining.
        double m_shapeNetRefAmplitude { 1.0 };
        /// Forward pass through the Shape Net (see above). sinPhase/cosPhase/altNorm must use
        /// the exact same conventions as runMLP()'s phase basis / m_alt_scale normalization.
        double evalShapeNet(double sinPhase, double cosPhase, double altNorm) const;

        // ── Runtime state ─────────────────────────────────────────────────────
        bool   m_weightsLoaded    { false };
        double m_confidenceRA     { 0.0 };
        double m_confidenceDEC    { 0.0 };
        static int    m_frameCount;
        static double m_typicalRMS_ra;
        static double m_typicalRMS_dec;
        /// PE period the persisted static RLS state was built for; a change means the
        /// loaded weights describe a different mount, so the static state is discarded.
        static double s_activePePeriod;
        double m_lastDt           { 2.0 };
        double m_lastAltRad       { M_PI / 4.0 };
        double m_lastSessionSec   { 0.0 };
        double m_lastPredDriftRA   { 0.0 };
        double m_lastPredDriftDEC  { 0.0 };
        double m_lastSNR          { 0.0 };
        double m_lastPixelScale   { 1.0 };
        double m_lastParallacticAngleDeg { 0.0 };
        double m_lastRAPulseMs    { 0.0 };
        double m_lastDECPulseMs   { 0.0 };
        float  m_lastPierSide     { 1.0f };
        bool   m_hasLastPred      { false };
        bool   m_isStable         { false };
        std::deque<double> m_innovRA;
        std::deque<double> m_innovDec;
        static constexpr int INNOV_WINDOW = 20;
        /// Windowed innovation above this rate means the loop is unstable, not noisy.
        static constexpr double INSTABILITY_ARCSEC_PER_SEC = 1.5;
        /// Confidence falls freely but may rise at most this much per frame.
        static constexpr double CONF_RISE_PER_FRAME = 0.05;
        /// Minimum worm cycles of REAL ELAPSED SESSION TIME (t_session_sec, not a frame
        /// count) required before the RA phase estimate is trusted at all -- checked
        /// directly in predict()/darkPredict()/updateConfidence(), deliberately not folded
        /// into warmupFrames(). An earlier version tried exactly that (converting via
        /// m_lastDt to a frame-count threshold) and it was wrong: warmupFrames() is
        /// re-evaluated every frame against the INSTANTANEOUS last dt, so a run of slow/
        /// laggy frames (verified 2026-08-24: dt spiked 2s->4-8s right at a premature
        /// WARMUP->ACTIVE transition) transiently shrinks the computed frame threshold and
        /// can satisfy frameCount > warmupFrames() well before 1.5 real cycles have
        /// actually elapsed. Comparing elapsed time directly has no such failure mode.
        static constexpr double MIN_PE_CYCLES = 1.5;

        // ── Helpers ───────────────────────────────────────────────────────────
        double physicsRA(double t_sec, double altitude_deg) const;
        double physicsDEC(double altitude_deg, double parallactic_angle_deg) const;
        std::array<float, 2> runMLP(float altitude, float snr, float last_ra_pulse, float last_dec_pulse, float dt,
                                    double t_session_sec, float parallactic_angle_deg, float pier_side) const;
        void   updatePhase(double uncorrected_position_px, double t_session_sec);
        void   updateConfidence(double innovRA, double innovDec, double snr);
};
