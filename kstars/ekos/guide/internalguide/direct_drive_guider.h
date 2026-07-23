#pragma once
/*
 * direct_drive_guider.h — Parametric refraction model for direct-drive mounts
 *
 * No PE (no gears). No MLP. Only 4 analytically-fitted parameters:
 *   drift_ra(alt)      = k_ref / cos²(alt)  +  d_ra_extra
 *   drift_dec(alt, q)  = d_polar  +  k_ref_dec * sin(q) / cos²(alt)
 *
 * Inference cost: ~8 float operations per frame. Effectively free.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "mount_guider.h"
#include <cmath>

class QJsonObject;

class DirectDriveGuider : public MountSpecificGuider
{
    public:
        DirectDriveGuider() = default;

        bool loadWeights(const QString &weightsPath) override;
        void resetSession(bool forceReset = false) override;
        GuideOutput predict(const GuideFrameData &frame) override;
        GuideOutput darkPredict(double dt_sec) override;

        // DirectDriveGuider update() is a no-op: there are no learnable online parameters.
        void update(double /*ra_px*/, double /*dec_px*/,
                    double /*uncorr_ra*/, double /*uncorr_dec*/, double /*snr*/,
                    double /*ra_pulse_px*/, double /*dec_pulse_px*/) override {}

        double confidence() const override
        {
            return m_weightsLoaded ? 0.95 : 0.0;
        }
        int    warmupFrames() const override
        {
            return 10;    // Minimal warmup: model is purely analytical
        }
        bool   isLoaded() const override
        {
            return m_weightsLoaded;
        }
        QString stateString() const override;

    private:
        bool m_weightsLoaded { false };
        int  m_frameCount    { 0 };

        double m_lastAltRad  { 0.0 };
        double m_lastParallacticAngleDeg { 0.0 };

        // --- Fitted parameters (loaded from JSON, written by train_direct_drive.py) ---
        double m_k_ref      { 0.0 };   ///< RA refraction coefficient in px/s at cos²alt=1 (×pixel_scale → arcsec)
        double m_d_ra_extra { 0.0 };   ///< Residual RA drift not explained by refraction (px/s)
        double m_d_polar    { 0.0 };   ///< Constant DEC polar drift (px/s)
        double m_k_ref_dec  { 0.0 };   ///< DEC refraction coefficient
        double m_fit_alt_min { 35.0 }; ///< Altitude range the fit is valid for
        double m_fit_alt_max { 65.0 };
        double m_phi_drift  { 0.0 };   ///< Polar drift vector angle (unused in inference, stored for info)

        double m_pixel_scale { 1.0 };  ///< arcsec/pixel, loaded from weights JSON

        // --- Physics helpers ---
        double physicsRA (double alt_deg) const;
        double physicsDEC(double alt_deg, double q_deg) const;

        // Rejects weights whose recorded equipment fingerprint (exposure, binning, gains, …)
        // does not match the current session, so a model trained at a different binning /
        // pixel-scale is not silently applied. Mirrors WormGearGuider / HarmonicGuider.
        static bool validateFingerprint(const QJsonObject &fp);
};
