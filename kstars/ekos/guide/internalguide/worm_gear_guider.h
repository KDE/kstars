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
