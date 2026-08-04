/*
 * mount_guider.cpp — shared helpers for MountSpecificGuider implementations
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "mount_guider.h"

#include "Options.h"
#include "ekos/guide/opsguide.h"

#include <KConfigDialog>
#include <QJsonObject>
#include <QMetaObject>
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

    // Loading a trained model implies the intent to actually use it, so switch both
    // axes to the AI Guider pulse algorithm if they aren't already — otherwise the
    // just-applied gains sit unused (raAlgorithmIsAI()/decAlgorithmIsAI() in gmath.cpp
    // gate the AI feed-forward blend on this exact setting). The user can still switch
    // back manually afterward; this only runs at load time.
    if (Options::rAGuidePulseAlgorithm() != static_cast<uint>(Ekos::OpsGuide::AI_ALGORITHM))
    {
        changes << QString("ra_guide_algorithm: %1 -> AI Guider").arg(Options::rAGuidePulseAlgorithm());
        Options::setRAGuidePulseAlgorithm(static_cast<uint>(Ekos::OpsGuide::AI_ALGORITHM));
    }
    if (Options::dECGuidePulseAlgorithm() != static_cast<uint>(Ekos::OpsGuide::AI_ALGORITHM - 1))
    {
        changes << QString("dec_guide_algorithm: %1 -> AI Guider").arg(Options::dECGuidePulseAlgorithm());
        Options::setDECGuidePulseAlgorithm(static_cast<uint>(Ekos::OpsGuide::AI_ALGORITHM - 1));
    }

    // The Guide Options dialog ("guidesettings") binds its kcfg_ widgets to Options once,
    // when the page is built, and never re-syncs them on its own. Without this, the dialog
    // would keep showing stale values (e.g. aggressiveness 1.0) after the fingerprint above
    // changed Options directly — and clicking Apply there would silently clobber the
    // just-loaded values back to whatever the stale widgets displayed. updateWidgets() is a
    // protected slot, but Qt's meta-object dispatch does not enforce C++ access control, so
    // invoking it by name is the standard way to trigger a re-sync from outside the class.
    if (!changes.isEmpty())
    {
        if (KConfigDialog *dialog = KConfigDialog::exists("guidesettings"))
            QMetaObject::invokeMethod(dialog, "updateWidgets");
    }

    return changes;
}
