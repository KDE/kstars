/*
 * mount_guider.cpp — shared helpers for MountSpecificGuider implementations
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "mount_guider.h"

#include "Options.h"

#include <QJsonObject>
#include <cmath>

QStringList applyFingerprintToOptions(const QJsonObject &fp)
{
    QStringList changes;
    if (fp.isEmpty())
        return changes;

    const struct
    {
        const char *key;
        double current;
        double tol;
        void (*setter)(double);
    } doubleFields[] =
    {
        { "guide_exposure_s",      Options::guideExposure(),         0.05,  &Options::setGuideExposure },
        { "ra_proportional_gain",  Options::rAProportionalGain(),    1e-4,  &Options::setRAProportionalGain },
        { "dec_proportional_gain", Options::dECProportionalGain(),   1e-4,  &Options::setDECProportionalGain },
        { "ra_integral_gain",      Options::rAIntegralGain(),        1e-4,  &Options::setRAIntegralGain },
        { "dec_integral_gain",     Options::dECIntegralGain(),       1e-4,  &Options::setDECIntegralGain },
        { "ra_min_pulse_arcsec",   Options::rAMinimumPulseArcSec(),  1e-4,  &Options::setRAMinimumPulseArcSec },
        { "dec_min_pulse_arcsec",  Options::dECMinimumPulseArcSec(), 1e-4,  &Options::setDECMinimumPulseArcSec },
        { "ra_hysteresis",         Options::rAHysteresis(),          1e-4,  &Options::setRAHysteresis },
        { "dec_hysteresis",        Options::dECHysteresis(),         1e-4,  &Options::setDECHysteresis },
    };

    for (const auto &f : doubleFields)
    {
        if (!fp.contains(f.key))
            continue;
        const double recorded = fp[f.key].toDouble();
        if (std::abs(recorded - f.current) > f.tol)
        {
            changes << QString("%1: %2 -> %3").arg(f.key).arg(f.current, 0, 'g', 6).arg(recorded, 0, 'g', 6);
            f.setter(recorded);
        }
    }

    if (fp.contains("guide_binning"))
    {
        const QString recorded = fp["guide_binning"].toString();
        if (!recorded.isEmpty() && recorded != Options::guideBinning())
        {
            changes << QString("guide_binning: %1 -> %2").arg(Options::guideBinning(), recorded);
            Options::setGuideBinning(recorded);
        }
    }

    const struct
    {
        const char *key;
        uint current;
        void (*setter)(uint);
    } uintFields[] =
    {
        { "ra_max_pulse_arcsec",  Options::rAMaximumPulseArcSec(),  &Options::setRAMaximumPulseArcSec },
        { "dec_max_pulse_arcsec", Options::dECMaximumPulseArcSec(), &Options::setDECMaximumPulseArcSec },
    };

    for (const auto &f : uintFields)
    {
        if (!fp.contains(f.key))
            continue;
        const uint recorded = static_cast<uint>(std::lround(fp[f.key].toDouble()));
        if (recorded != f.current)
        {
            changes << QString("%1: %2 -> %3").arg(f.key).arg(f.current).arg(recorded);
            f.setter(recorded);
        }
    }

    return changes;
}
