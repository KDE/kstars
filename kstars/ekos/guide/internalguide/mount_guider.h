#pragma once
/*
 * mount_guider.h — Abstract base class for mount-specific AI guiders
 *
 *
 *
 * Each concrete implementation corresponds to one mount class:
 *   WormGearGuider    — PINN + residual MLP  (~200 params)
 *   HarmonicGuider    — Neural Kalman Filter (~90 params + 8 physical)
 *   DirectDriveGuider — Parametric refraction model (4 floats)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QString>
#include <cstdint>

// ---------------------------------------------------------------------------
// Input frame data — populated by InternalGuider::processGuiding() each frame
// ---------------------------------------------------------------------------
struct GuideFrameData
{
    double ra_raw_px       { 0.0 };  ///< RA tracking error (pixels, signed)
    double dec_raw_px      { 0.0 };  ///< DEC tracking error (pixels, signed)
    double ra_pulse_ms     { 0.0 };  ///< Last RA correction pulse (ms, signed East+)
    double dec_pulse_ms    { 0.0 };  ///< Last DEC correction pulse (ms, signed North+)
    double snr             { 0.0 };  ///< Guide star SNR
    double snr_delta       { 0.0 };  ///< SNR change from previous frame (seeing indicator)
    double dt              { 2.0 };  ///< Time since last frame (seconds)
    double pixel_scale     { 1.0 };  ///< Arcsec per pixel (from calibration)
    double ra_ms_per_arcsec  { 0.0 };///< RA guide-rate calibration (ms per arcsec; 0 = uncalibrated)
    double dec_ms_per_arcsec { 0.0 };///< DEC guide-rate calibration (ms per arcsec; 0 = uncalibrated)
    double altitude_deg    { 45.0 }; ///< Current telescope altitude (degrees)
    double hour_angle_deg  { 0.0 };  ///< from mount RA + LST; negative = east, positive = west
    double azimuth_deg     { 180.0 };///< needed to compute parallactic angle
    double parallactic_angle_deg { 0.0 }; ///< computed from alt/az + site latitude each frame
    bool   guide_optics_oag { false }; ///< true if Off-Axis Guider (for differential flexure logic)
    bool   pier_side_east  { false };///< true = pier East, false = pier West
    double t_session_sec   { 0.0 };  ///< Monotonic seconds since the AI subsystem first ran
    ///< (process-relative, not reset per session); used only
    ///< as a continuous phase clock, so absolute origin is irrelevant
    bool   ra_pulse_suppressed  { false }; ///< Pulse was zeroed by minMove cutoff
    bool   dec_pulse_suppressed { false }; ///< Pulse was zeroed by minMove cutoff
};

// ---------------------------------------------------------------------------
// Output from predict() — feed-forward correction to blend with P-controller
// ---------------------------------------------------------------------------
struct GuideOutput
{
    bool   valid                   { false }; ///< false during warmup period
    double ra_correction_arcsec    { 0.0 };   ///< Feed-forward RA correction (arcsec)
    double dec_correction_arcsec   { 0.0 };   ///< Feed-forward DEC correction (arcsec)

    // Debug specific outputs
    double physics_ra_arcsec       { 0.0 };   ///< Physics layer RA drift prediction
    double physics_dec_arcsec      { 0.0 };   ///< Physics layer DEC drift prediction
    double mlp_ra_arcsec           { 0.0 };   ///< MLP layer RA residual prediction
    double mlp_dec_arcsec          { 0.0 };   ///< MLP layer DEC residual prediction

    double confidence              { 0.0 };   ///< Model confidence in [0, 1]
    QString debug_log;                        ///< Optional human-readable state string
};

// ---------------------------------------------------------------------------
// Abstract base
// ---------------------------------------------------------------------------
class MountSpecificGuider
{
    public:
        virtual ~MountSpecificGuider() = default;

        /**
         * @brief Load trained weights from a JSON file produced by offline_trainer.
         * @param weightsPath  Absolute path to weights_<mount>.json
         * @return true on success; false if file missing, corrupt, or fingerprint mismatch
         */
        virtual bool loadWeights(const QString &weightsPath) = 0;

        /**
         * @brief Reset internal state at the start of each guiding session.
         * Must be called before the first predict() of a new session.
         */
        virtual void resetSession(bool forceReset = false) = 0;

        /**
         * @brief Per-frame inference.
         * @param frame  Current guide frame data (see GuideFrameData)
         * @return Feed-forward correction. valid=false during warmup period.
         *         When valid=false the caller must rely entirely on the P-controller.
         */
        virtual GuideOutput predict(const GuideFrameData &frame) = 0;

        /**
         * @brief Extrapolates the feed-forward correction when no new guide frame is available.
         * @param dt_sec Time elapsed since the last known frame or prediction in seconds.
         * @return Feed-forward correction based on the advanced physical state.
         */
        virtual GuideOutput darkPredict(double dt_sec) = 0;

        /**
         * @brief Called after each frame with the actual observed next-frame error.
         * Enables online adaptation of session-specific parameters (e.g. PE phase).
         * @param ra_error_px   Observed RA error on the NEXT frame (pixels)
         * @param dec_error_px  Observed DEC error on the NEXT frame (pixels)
         * @param uncorrected_drift_ra_px Uncorrected physical RA drift (pixels)
         * @param uncorrected_drift_dec_px Uncorrected physical DEC drift (pixels)
         * @param snr Guide star SNR
         * @param ra_pulse_px Signed RA pulse applied over this interval (pixels)
         * @param dec_pulse_px Signed DEC pulse applied over this interval (pixels)
         */
        virtual void update(double ra_error_px, double dec_error_px, double uncorrected_drift_ra_px,
                            double uncorrected_drift_dec_px, double snr,
                            double ra_pulse_px, double dec_pulse_px) = 0;

        /**
         * @brief Current model confidence in [0, 1].
         * The controller blends: correction = P_gain * measured + AI_gain * confidence * predicted
         */
        virtual double confidence() const = 0;

        /**
         * @brief Minimum number of frames before any feed-forward output is emitted.
         * During warmup, predict() always returns valid=false.
         */
        virtual int warmupFrames() const = 0;

        /**
         * @brief Human-readable description of model state for the Guide graph tooltip.
         */
        virtual QString stateString() const = 0;

        /**
         * @brief True when weights loaded successfully and fingerprint matches current settings.
         */
        virtual bool isLoaded() const
        {
            return false;
        }
};
