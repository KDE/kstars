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

        // ── v3 online residual filter (2026-08-27) ────────────────────────────────
        // Replaces RLS's ROLE as the online-reactive component for the Shape Net path (the
        // existing RLS phase/amplitude tracking above is UNCHANGED and still runs -- this is
        // an additional layer, not a replacement of that). Motivation: live A/B testing this
        // same night showed KStars' existing, purely-online GPG algorithm outperforming every
        // Shape-Net+RLS variant tried, including at the hardest (lowest-altitude) conditions
        // tested. Root cause isn't numerical (the RLS divergence bug earlier that night is
        // fixed) -- it's structural: RLS forces the online signal into a rigid fixed-harmonic
        // basis with no way to say "there's no real periodic structure right now, back off".
        // GPG has no such commitment; it just adaptively smooths recent history, so it
        // degrades gracefully when the true signal is weak or absent (which four independent
        // tests that night found to be the common case here).
        //
        // This filter gives the Shape Net path the same graceful-degradation property WITHOUT
        // copying GPG's mechanism: a minimal (one-state) Kalman/recursive-least-variance filter
        // that tracks a RESIDUAL RATE -- how much correction is needed beyond what the Shape
        // Net's own predicted rate (physicsRA()'s existing finite-difference derivative)
        // already accounts for. Working in rate space (not accumulated position) deliberately
        // sidesteps the earlier RLS bug's whole failure class: there is no growing regressor,
        // no unbounded quantity anywhere in this filter's state or math, so no bounds backstop
        // is needed here the way RLS needed one. Its Kalman gain is exactly what supplies the
        // graceful-degradation behavior: when the residual observation looks like noise around
        // zero (no real leftover structure), the filter's own steady-state covariance keeps its
        // correction small; when there's a real, persistent residual drift, the gain lets it
        // track that. The Shape Net supplies the (offline-learned, "our own model") prior/bias
        // this filter corrects around -- it is not a GPG reimplementation, it is a from-scratch
        // recursive Bayesian estimator whose reference function happens to be our trained net.
        //
        // Deliberately fed the SAME m_uncorrectedPosRA-derived per-frame delta already computed
        // in update() (not a new measurement channel), and gated by the exact same
        // ra_pulse_has_ai freeze guard as updatePhase() above, for the same closed-loop
        // system-identification reason documented there.
        static double m_kfResidualRate;  ///< px/s, current best residual-rate estimate
        static double m_kfP;             ///< scalar Kalman covariance, (px/s)^2
        /// Process noise, (px/s)^2 added per second of elapsed time. NOT a universal constant --
        /// loaded per-mount from weights.json's "residual_filter" section (see loadWeights()),
        /// because the right value depends on this specific mount's own noise floor and PE
        /// character, which varies across mount classes/hardware (confirmed 2026-08-28: this
        /// whole investigation was on one EQ8-class worm-gear mount; nothing here has been
        /// checked against a harmonic-drive or direct-drive mount, or even a different worm-gear
        /// unit). The values below are only the fallback default for weights files that predate
        /// this section, derived from targeting roughly a 100-frame (~200s at 2s cadence)
        /// effective smoothing window given this one mount's ~0.4px/frame position noise:
        /// N_eff ~ sqrt(R / (Q * dt)) => Q ~ R / (N_eff^2 * dt). A proper per-mount value should
        /// come from the same offline analysis (causal-EMA/autocorrelation sweep) run on that
        /// mount's own pooled sysid data, the same way pe_period/pe_harmonic_amplitude etc. are
        /// already mount-specific rather than hardcoded.
        double m_kfQPerSec { 2e-6 };
        /// Per-frame measurement noise, (px/s)^2 -- see m_kfQPerSec's comment; same
        /// mount-specific caveat and same loadWeights() section.
        double m_kfR { 0.04 };
        /// Multiplier applied to m_kfR on frames where the just-applied RA pulse included this
        /// guider's own prediction (ra_pulse_has_ai true). Live-tested 2026-08-28 on the one EQ8
        /// mount this was built against: gating this filter behind the SAME hard freeze
        /// updatePhase()/RLS uses (see update()'s comment on ra_pulse_has_ai for the real
        /// closed-loop-bias risk that freeze protects against) made physics_ra vs actual RA
        /// error go from a highly significant, GROWING correlation in Shadow mode (r=0.25,
        /// t=5.7, n=496, strengthening 0.18->0.34 across the session) to a non-significant one
        /// within minutes of switching Active (r=-0.05), with RMS visibly worsening over time
        /// (1.21"->1.57" across four chronological quarters) -- the filter's whole value came
        /// from continuous adaptation, and freezing it the moment it starts being used discards
        /// exactly that. Full removal of the freeze isn't safe either (that bias risk is real,
        /// verified by a real divergence incident). This is the middle path: keep updating, but
        /// trust each Active-mode observation less, so a single biased sample can't hijack the
        /// filter (Kalman gain shrinks accordingly) while genuine slow drift still gets tracked
        /// instead of going stale. The specific factor of 20 is this mount's own value, not a
        /// principle -- how much a pulse-backout bias actually contaminates an observation is
        /// itself mount/calibration-dependent and hasn't been measured directly anywhere.
        double m_kfRActiveMultiplier { 20.0 };
        void updateResidualKF(double measuredRate_px_s, double shapeNetRate_px_s, double dt,
                              bool activeContaminated);

        // ── DEC online drift tracker (2026-08-28) ─────────────────────────────────
        // physicsDEC() below was, until now, a fixed offline formula (refraction + a constant
        // m_d_polar term) with NO online-adaptive state at all -- unlike RA, nothing here ever
        // calibrated itself to tonight's actual conditions. Direct offline analysis of that
        // night's own AI debug CSVs found this was leaving real, sizeable signal on the table:
        // DEC's raw (uncorrected) drift has a fundamentally different character from RA's --
        // RA's autocorrelation collapses to noise after ~1 frame, but DEC's stays substantially
        // positive (0.15-0.28) out to 30+ seconds, without oscillating -- the signature of a
        // slow, smoothly-varying trend (consistent with polar alignment error), not a period to
        // fit and not noise. A trivial CAUSAL exponential moving average (only past frames, no
        // model, no offline training) explained 6-21% of DEC's total raw-drift variance across
        // three separate files spanning the night -- several times RA's entire offline-
        // validated ceiling (R^2~0.03) -- and did so consistently, unlike the DEC reversal-event
        // data (checked the same night, found no cross-night consistency at fixed conditions).
        // This filter is that online tracker, made real: same 1-state recursive design as the RA
        // residual filter above (rate-space, no unbounded quantity, no divergence backstop
        // needed), but standing alone -- there is no offline "shape" here to be a residual
        // against, physicsDECBase()'s existing refraction+polar term IS the base this tracks a
        // residual on top of, the same relationship the RA filter has to the Shape Net.
        //
        // Q/R were NOT hand-picked the way the RA filter's first cut was (see that filter's own
        // comment) -- they're derived from the same offline analysis: the best-performing causal
        // EMA smoothing constant found across three files from ONE EQ8 mount's data clustered
        // around alpha=0.08-0.15 (~0.1 central). An EMA is mathematically the steady-state
        // behavior of exactly this Kalman structure, so the defaults below were solved to
        // reproduce a steady-state gain of ~0.1 at a ~2s frame cadence -- derived from data, not
        // guessed, but still ONE mount's data. Same mount-specific caveat as m_kfQPerSec above:
        // these are fallback defaults loaded from weights.json's "residual_filter" section (see
        // loadWeights()), not a value that should be assumed to hold for a harmonic-drive or
        // direct-drive mount, or even a different worm-gear unit -- a proper per-mount value
        // needs the same causal-EMA sweep run on that mount's own pooled sysid data.
        static double m_kfResidualRateDec;
        static double m_kfPDec;
        double m_kfQPerSecDec { 2.2e-4 };
        double m_kfRDec { 0.04 };
        // Same closed-loop-bias defense as m_kfRActiveMultiplier, applied to DEC -- arguably
        // more important here, not less: DEC's target signal is itself persistent/autocorrelated
        // (that's the whole point of this filter), which makes a systematic pulse-backout bias
        // easier for a recursive filter to mistake for real signal and reinforce, not harder.
        // Same mount-specific caveat: this factor of 20 is carried over from the RA filter's
        // value on this one mount, not independently derived or validated for DEC at all.
        double m_kfRDecActiveMultiplier { 20.0 };
        void updateResidualKFDec(double measuredRate_px_s, double physicsDecRate_px_s, double dt,
                                 bool activeContaminated);

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
        /// physicsRA() minus the v3 residual term -- exposed separately so update() can compute
        /// the same base prediction the residual filter should be measured against, without
        /// double-counting the residual it's in the middle of updating.
        double physicsRABase(double t_sec, double altitude_deg) const;
        double physicsDEC(double altitude_deg, double parallactic_angle_deg) const;
        /// physicsDEC() minus the online drift-tracker residual -- same reason physicsRABase()
        /// exists, see its comment.
        double physicsDECBase(double altitude_deg, double parallactic_angle_deg) const;
        std::array<float, 2> runMLP(float altitude, float snr, float last_ra_pulse, float last_dec_pulse, float dt,
                                    double t_session_sec, float parallactic_angle_deg, float pier_side) const;
        void   updatePhase(double uncorrected_position_px, double t_session_sec);
        void   updateConfidence(double innovRA, double innovDec, double snr);
};
