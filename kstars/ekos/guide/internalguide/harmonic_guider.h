#pragma once
/*
 * harmonic_guider.h — Neural Kalman Filter guider for harmonic drive mounts
 *
 * Architecture: 10-state Kalman filter with spring dynamics and PE tracking,
 *               plus a small Q-net MLP (~66 parameters) for adaptive process noise.
 *
 * State vector:
 *   [ra_err, ra_vel, spring_ra, pe_sin_ra, pe_cos_ra,
 *    dec_err, dec_vel, spring_dec, pe_sin_dec, pe_cos_dec]
 *
 * The spring states model elastic wind-up: the flexspline absorbs a fraction κ
 * of each correction pulse and releases it exponentially with time constant τ.
 *
 * The PE states evolve as a 2D rotation at the detected PE frequency, allowing
 * the Kalman filter to automatically estimate PE amplitude and phase online.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "mount_guider.h"
#include <deque>
#include <Eigen/Core>
#include <QJsonObject>

class HarmonicGuider : public MountSpecificGuider
{
    public:
        HarmonicGuider();
        ~HarmonicGuider() override = default;

        bool        loadWeights(const QString &weightsPath) override;
        void        resetSession(bool forceReset = false) override;
        GuideOutput predict(const GuideFrameData &frame) override;
        GuideOutput darkPredict(double dt_sec) override;
        void        update(double ra_error_px, double dec_error_px, double uncorrected_drift_ra_px,
                           double uncorrected_drift_dec_px, double snr,
                           double ra_pulse_px, double dec_pulse_px) override;
        double      confidence() const override
        {
            return m_confidence;
        }
        int         warmupFrames() const override
        {
            return 30;
        }
        bool        isLoaded() const override
        {
            return m_weightsLoaded;
        }
        QString     stateString() const override;

    private:
        // ── Kalman dimensions ────────────────────────────────────────────────
        static constexpr int N_STATES = 10;
        static constexpr int N_OBS    = 2;

        // State indices
        static constexpr int RA_POS     = 0;
        static constexpr int RA_VEL     = 1;
        static constexpr int RA_SPRING  = 2;
        static constexpr int RA_PE_SIN  = 3;
        static constexpr int RA_PE_COS  = 4;
        static constexpr int DEC_POS    = 5;
        static constexpr int DEC_VEL    = 6;
        static constexpr int DEC_SPRING = 7;
        static constexpr int DEC_PE_SIN = 8;
        static constexpr int DEC_PE_COS = 9;

        // ── Spring parameters (loaded from weights JSON) ─────────────────────
        double m_kappa_ra    { 0.2 };   ///< Fraction of RA pulse absorbed by spring [0, 0.9]
        double m_tau_ra      { 1.5 };   ///< RA spring release time constant (seconds)
        double m_kappa_dec   { 0.2 };   ///< Fraction of DEC pulse absorbed
        double m_tau_dec     { 1.5 };   ///< DEC spring release time constant

        // ── PE parameters (loaded from weights JSON) ─────────────────────────
        double m_pe_period   { 0.0 };   ///< PE period in seconds (0 = no PE detected)
        double m_pe_amplitude { 0.0 };  ///< Initial PE amplitude estimate (px)

        // ── Drift / refraction parameters ────────────────────────────────────
        double m_drift_ra    { 0.0 };   ///< Baseline RA drift rate (px/s)
        double m_drift_dec   { 0.0 };   ///< Baseline DEC drift rate (px/s)
        double m_k_ref       { 0.0 };   ///< RA refraction coefficient
        double m_d_polar     { 0.0 };   ///< DEC polar drift rate (px/s)
        double m_k_ref_dec   { 0.0 };   ///< DEC refraction coefficient
        double m_fit_alt_min { 35.0 };  ///< Altitude range the drift fit is valid for
        double m_fit_alt_max { 65.0 };

        // ── Kalman filter state (static to survive object recreation) ────────
        static Eigen::Matrix<double, N_STATES, 1> m_x;       ///< State estimate
        static Eigen::Matrix<double, N_STATES, N_STATES> m_P; ///< Error covariance
        static double m_uncorrPosRA;   ///< Integrated uncorrected position (the measurement)
        static double m_uncorrPosDEC;
        static int    m_frameCount;
        static double m_typicalRMS;
        /// PE period the persisted static state was built for; a change means the
        /// loaded weights describe a different mount, so the static state is discarded.
        static double s_activePePeriod;

        // ── Q-net MLP weights (5 → 8 → 2) ───────────────────────────────────
        // Input: [snr_norm, snr_delta_norm, |innov_ra|, |innov_dec|, dt_norm]
        // Output: [log_Q_ra, log_Q_dec] (log-scale process noise)
        Eigen::Matrix<float, 8, 5> m_qw1;
        Eigen::Vector<float, 8>    m_qb1;
        Eigen::Matrix<float, 2, 8> m_qw2;
        Eigen::Vector2f            m_qb2;

        // ── Runtime state ────────────────────────────────────────────────────
        bool   m_weightsLoaded    { false };
        double m_confidence       { 0.0 };
        double m_lastDt           { 2.0 };
        double m_lastAltRad       { M_PI / 4.0 };
        double m_lastSessionSec   { 0.0 };
        double m_lastSNR          { 0.0 };
        double m_lastSNRDelta     { 0.0 };
        double m_lastPixelScale   { 1.0 };
        double m_lastParallacticAngleDeg { 0.0 };
        double m_lastRAPulseMs    { 0.0 };
        double m_lastDECPulseMs   { 0.0 };
        bool   m_hasLastPred      { false };
        double m_lastPredRA       { 0.0 };
        double m_lastPredDEC      { 0.0 };
        double m_prevSNR          { 0.0 };

        // Q-net input features. train_harmonic.py conditions process noise on the raw
        // frame-to-frame change of the tracking error, |ra_raw_px[i] - ra_raw_px[i-1]|
        // (NOT the Kalman innovation), so the runtime must feed the same quantity or the
        // trained Q-net sees out-of-distribution inputs.
        double m_prevRaRawPx      { 0.0 };
        double m_prevDecRawPx     { 0.0 };
        bool   m_hasPrevRaw       { false };
        double m_qFeatRA          { 0.0 };  ///< |ra_raw_px - prev_ra_raw_px| for the Q-net
        double m_qFeatDec         { 0.0 };  ///< |dec_raw_px - prev_dec_raw_px| for the Q-net

        // Innovation tracking for confidence
        std::deque<double> m_innovRA;
        std::deque<double> m_innovDec;
        static constexpr int INNOV_WINDOW = 20;

        // ── Helpers ──────────────────────────────────────────────────────────
        static bool validateFingerprint(const QJsonObject &fp);
        void buildF(Eigen::Matrix<double, N_STATES, N_STATES> &F, double dt) const;
        Eigen::Matrix<double, N_STATES, N_STATES> computeQ(double snr, double snr_delta,
                double innov_ra, double innov_dec, double dt) const;
        void driftRates(double alt_deg, double parallactic_angle_deg,
                        double &ra_rate, double &dec_rate) const;
        void kalmanPredict(double dt, double alt_deg, double parallactic_angle_deg);
        void kalmanUpdate(double ra_meas_px, double dec_meas_px, double snr);
        void updateConfidence(double innovRA, double innovDec, double snr);
};
