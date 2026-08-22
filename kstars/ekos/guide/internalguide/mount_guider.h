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
#include <QStringList>
#include <QJsonObject>
#include <cstdint>

/**
 * @brief Applies every field present in a training fingerprint (guide exposure, binning,
 * RA/DEC proportional+integral gain, min/max pulse, hysteresis) directly to the live
 * Options, so a trained model's recorded settings are reinstated automatically rather
 * than requiring the user to go match them by hand across several settings panels.
 * Exposure/binning changes take effect on the next guide session start, not retroactively
 * mid-capture.
 * @return A human-readable "key: old -> new" line per field that actually changed value
 * (empty if the fingerprint was empty or every field already matched).
 */
QStringList applyFingerprintToOptions(const QJsonObject &fingerprint);

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
    double drift_ra_arcsec         { 0.0 };   ///< Static drift-model injection (RA)
    double drift_dec_arcsec        { 0.0 };   ///< Static drift-model injection (DEC)
    double physics_ra_arcsec       { 0.0 };   ///< Physics layer RA drift prediction
    double physics_dec_arcsec      { 0.0 };   ///< Physics layer DEC drift prediction
    double mlp_ra_arcsec           { 0.0 };   ///< MLP layer RA residual prediction
    double mlp_dec_arcsec          { 0.0 };   ///< MLP layer DEC residual prediction

    // Raw online RLS state (WormGearGuider only; 0 for implementations with no such state).
    // Exposed for the AI debug CSV so a live-vs-frozen phase-tracker divergence can be
    // diagnosed directly from theta instead of only the derived amplitude/phase string.
    double rls_sin_coeff           { 0.0 };   ///< theta(0): A*cos(phase), pixels
    double rls_cos_coeff           { 0.0 };   ///< theta(1): A*sin(phase), pixels
    double rls_drift_rate          { 0.0 };   ///< theta(2): linear drift term, px/s
    double rls_offset              { 0.0 };   ///< theta(3): constant offset, pixels

    double confidence_ra           { 0.0 };   ///< RA model confidence in [0, 1]
    double confidence_dec          { 0.0 };   ///< DEC model confidence in [0, 1]
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
         * @param ra_pulse_has_ai True if the RA pulse just applied (backed out of
         *        uncorrected_drift_ra_px above by the caller) included a contribution from
         *        this guider's own prediction, i.e. useAI was true and the AI branch fired
         *        for RA this cycle. Implementations with online-adaptive state derived from
         *        the same measurement (e.g. an RLS/Kalman phase tracker) should use this to
         *        avoid learning from a signal their own prior output partly produced -- a
         *        closed-loop system-ID bias. Always false during Shadow Mode, warmup, and
         *        fallback, since the AI never touches the applied pulse in those states.
         * @param dec_pulse_has_ai Same, for DEC.
         */
        virtual void update(double ra_error_px, double dec_error_px, double uncorrected_drift_ra_px,
                            double uncorrected_drift_dec_px, double snr,
                            double ra_pulse_px, double dec_pulse_px,
                            bool ra_pulse_has_ai, bool dec_pulse_has_ai) = 0;

        /**
         * @brief Current per-axis model confidence in [0, 1].
         * The controller blends, independently per axis:
         *   correction[axis] = P_gain * measured + AI_gain * confidence[axis] * predicted[axis]
         * Separate per axis because one axis's prediction quality says nothing about the
         * other's -- e.g. a mount with plenty of clean RA training data but chronically
         * sparse/poisoned DEC data should not have DEC's confidence dragged up (or RA's
         * dragged down) by the other axis's behavior. Implementations that have no basis
         * for distinguishing the axes (e.g. a single fixed-quality model) may simply
         * return the same value from both.
         */
        virtual double confidenceRA() const = 0;
        virtual double confidenceDEC() const = 0;

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

        /**
         * @brief Human-readable description of why the last loadWeights() call failed
         * (missing/corrupt file, or a mount_type mismatch). Empty on success; a
         * fingerprint mismatch is no longer a failure reason, see fingerprintApplied().
         */
        QString fingerprintError() const
        {
            return m_FingerprintError;
        }

        /**
         * @brief Human-readable "key: old -> new" summary of the settings applyFingerprintToOptions()
         * changed on the last successful loadWeights() call. Empty if nothing needed changing.
         */
        QString fingerprintApplied() const
        {
            return m_FingerprintApplied;
        }

    protected:
        QString m_FingerprintError;
        QString m_FingerprintApplied;
};
