/*
 * direct_drive_guider.cpp — Parametric refraction model for direct-drive mounts
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "direct_drive_guider.h"

#include "Options.h"
#include "ekos_guide_debug.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// loadWeights
// ---------------------------------------------------------------------------
bool DirectDriveGuider::loadWeights(const QString &weightsPath)
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

    if (root["mount_type"].toString() != "DIRECT_DRIVE")
    {
        m_FingerprintError = "weights were trained for a different mount class";
        return false;
    }

    // Reinstate the settings this model was trained under (exposure, binning, gains, …)
    // rather than requiring the user to have already matched them by hand.
    const QStringList changes = applyFingerprintToOptions(root["model_fingerprint"].toObject());
    if (!changes.isEmpty())
    {
        m_FingerprintApplied = changes.join("\n");
        qCInfo(KSTARS_EKOS_GUIDE) << "AI weights: applied recorded settings -" << changes.join("; ");
    }

    // Parameters block written by train_direct_drive.py
    QJsonObject params = root["parameters"].toObject();
    m_k_ref      = params["k_ref"].toDouble(0.0);
    m_d_polar    = params["d_polar"].toDouble(0.0);
    m_k_ref_dec  = params["k_ref_dec"].toDouble(0.0);
    m_d_ra_extra = params["d_ra_extra"].toDouble(0.0);
    m_phi_drift  = params["phi_drift"].toDouble(0.0);
    m_fit_alt_min = params["fit_alt_min"].toDouble(35.0);
    m_fit_alt_max = params["fit_alt_max"].toDouble(65.0);

    m_pixel_scale = root["pixel_scale"].toDouble(1.0);

    m_weightsLoaded = true;
    return true;
}

// ---------------------------------------------------------------------------
// resetSession
// ---------------------------------------------------------------------------
void DirectDriveGuider::resetSession(bool /*forceReset*/)
{
    m_frameCount = 0;
}

// ---------------------------------------------------------------------------
// predict — pure feed-forward, no learning at runtime
// ---------------------------------------------------------------------------
GuideOutput DirectDriveGuider::predict(const GuideFrameData &frame)
{
    m_frameCount++;

    m_lastAltRad = frame.altitude_deg * M_PI / 180.0;
    m_lastParallacticAngleDeg = frame.parallactic_angle_deg;

    GuideOutput out;
    out.valid      = (m_frameCount > warmupFrames());
    out.confidence = m_weightsLoaded ? 0.95 : 0.0;

    if (!out.valid)
        return out;

    // Drift rates in pixels/second, evaluated at NEXT frame time (feed-forward)
    const double phys_ra  = physicsRA (frame.altitude_deg);
    const double phys_dec = physicsDEC(frame.altitude_deg, frame.parallactic_angle_deg);

    // Drift in pixels over this frame interval
    const double pred_ra_px  = phys_ra  * frame.dt;
    const double pred_dec_px = phys_dec * frame.dt;

    // Convert to arcseconds for the output
    out.ra_correction_arcsec  = pred_ra_px  * frame.pixel_scale;
    out.dec_correction_arcsec = pred_dec_px * frame.pixel_scale;

    out.physics_ra_arcsec  = out.ra_correction_arcsec;
    out.physics_dec_arcsec = out.dec_correction_arcsec;
    out.mlp_ra_arcsec      = 0.0;  // No residual MLP for direct drive
    out.mlp_dec_arcsec     = 0.0;

    return out;
}

GuideOutput DirectDriveGuider::darkPredict(double dt_sec)
{
    GuideOutput out;
    out.valid      = (m_frameCount > warmupFrames());
    out.confidence = m_weightsLoaded ? 0.95 : 0.0;

    if (!out.valid)
        return out;

    const double phys_ra  = physicsRA (m_lastAltRad * 180.0 / M_PI);
    const double phys_dec = physicsDEC(m_lastAltRad * 180.0 / M_PI, m_lastParallacticAngleDeg);

    const double pred_ra_px  = phys_ra  * dt_sec;
    const double pred_dec_px = phys_dec * dt_sec;

    out.ra_correction_arcsec  = pred_ra_px  * m_pixel_scale;
    out.dec_correction_arcsec = pred_dec_px * m_pixel_scale;

    out.physics_ra_arcsec  = out.ra_correction_arcsec;
    out.physics_dec_arcsec = out.dec_correction_arcsec;
    out.mlp_ra_arcsec      = 0.0;
    out.mlp_dec_arcsec     = 0.0;

    return out;
}

// ---------------------------------------------------------------------------
// physicsRA  — altitude-dependent RA refraction drift (pixels/s; the trained
//              coefficients are fit in px/s by train_direct_drive.py)
// ---------------------------------------------------------------------------
double DirectDriveGuider::physicsRA(double alt_deg) const
{
    // Refraction fit is only valid inside the fitted altitude range.
    const double alt_rad = std::clamp(alt_deg, m_fit_alt_min, m_fit_alt_max) * M_PI / 180.0;
    const double cos_alt = std::cos(alt_rad);
    if (std::abs(cos_alt) < 1e-4) return m_d_ra_extra;
    return m_k_ref / (cos_alt * cos_alt) + m_d_ra_extra;
}

// ---------------------------------------------------------------------------
// physicsDEC — polar drift + parallactic-angle-dependent refraction (pixels/s)
// ---------------------------------------------------------------------------
double DirectDriveGuider::physicsDEC(double alt_deg, double q_deg) const
{
    const double alt_rad = std::clamp(alt_deg, m_fit_alt_min, m_fit_alt_max) * M_PI / 180.0;
    const double q_rad   = q_deg   * M_PI / 180.0;
    const double cos_alt = std::cos(alt_rad);
    if (std::abs(cos_alt) < 1e-4) return m_d_polar;
    return m_d_polar + m_k_ref_dec * std::sin(q_rad) / (cos_alt * cos_alt);
}

// ---------------------------------------------------------------------------
// stateString
// ---------------------------------------------------------------------------
QString DirectDriveGuider::stateString() const
{
    return QString("DirectDrive k_ref=%1 d_polar=%2 k_ref_dec=%3 d_ra_extra=%4")
           .arg(m_k_ref,      0, 'f', 5)
           .arg(m_d_polar,    0, 'f', 5)
           .arg(m_k_ref_dec,  0, 'f', 5)
           .arg(m_d_ra_extra, 0, 'f', 5);
}
