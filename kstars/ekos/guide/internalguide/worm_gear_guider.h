#pragma once
/*
 * worm_gear_guider.h — Physics-Informed NN guider for worm-gear mounts
 *
 * Architecture: Physics layer (deterministic sinusoidal PE + drift model) +
 *               small residual MLP (~192 parameters).
 *
 * The physics layer models:
 *   PE_ra(t)      = A * sin(2π*t/T + φ)          [learned A, T; online φ via RLS]
 *   drift_ra(alt) = k_ref / cos²(alt)             [refraction, learned k_ref]
 *   drift_dec     = d_polar + k_ref_dec * sin(q) / cos²(alt) [polar + refraction]
 *
 * The MLP handles unexplained residuals (PE harmonics, backlash, etc.).
 *
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "mount_guider.h"
#include <array>
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
                           double snr, double ra_pulse_px, double dec_pulse_px) override;
        double      confidence() const override
        {
            return m_confidence;
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
        static double m_pe_amplitude;  ///< Worm PE amplitude (pixels)
        double m_pe_period     { 480.0 };///< Worm PE period (seconds) — fixed after FFT fit
        double m_k_ref         { 0.0 };  ///< Refraction coefficient (pixels·cos²/s)
        static double m_d_ra_extra;    ///< Continuous RA drift rate (pixels/second)
        double m_d_polar       { 0.0 };  ///< Polar drift rate (pixels/second)
        double m_k_ref_dec     { 0.0 };  ///< DEC Refraction coefficient
        double m_fit_alt_min   { 35.0 }; ///< Altitude range the drift fit is valid for
        double m_fit_alt_max   { 65.0 };

        // ── Online phase estimation (4-State Position RLS) ────────────────
        static Eigen::Vector4d m_rls_theta; ///< [sin_coeff, cos_coeff, v, C]
        static Eigen::Matrix4d m_rls_P;     ///< RLS covariance
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
        double m_confidence       { 0.0 };
        static int    m_frameCount;
        static double m_typicalRMS;
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
        std::deque<double> m_innovRA;
        std::deque<double> m_innovDec;
        static constexpr int INNOV_WINDOW = 20;

        // ── Helpers ───────────────────────────────────────────────────────────
        static bool validateFingerprint(const QJsonObject &fp);
        double physicsRA(double t_sec, double altitude_deg) const;
        double physicsDEC(double altitude_deg, double parallactic_angle_deg) const;
        std::array<float, 2> runMLP(float altitude, float snr, float last_ra_pulse, float last_dec_pulse, float dt,
                                    double t_session_sec, float parallactic_angle_deg, float pier_side) const;
        void   updatePhase(double uncorrected_position_px, double t_session_sec);
        void   updateConfidence(double innovRA, double innovDec, double snr);
};
