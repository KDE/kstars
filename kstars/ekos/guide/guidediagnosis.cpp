/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "guidediagnosis.h"

#include <KLocalizedString>

#include <algorithm>

namespace Ekos
{

using namespace Diagnosis;

namespace
{
Item makeItem(const QString &id, Kind kind, Tier tier, Likelihood likelihood, const QString &title, const QString &why)
{
    Item item;
    item.id = id;
    item.kind = kind;
    item.tier = tier;
    item.likelihood = likelihood;
    item.title = title;
    item.why = why;
    return item;
}

Verify makeVerify(const QString &action, const QString &metric = QString(), const QString &op = QString(),
                  double threshold = 0)
{
    Verify verify;
    verify.action = action;
    verify.metric = metric;
    verify.op = op;
    verify.threshold = threshold;
    return verify;
}

bool isCalibrationFailure(GuideFailReason reason)
{
    switch (reason)
    {
        case GuideFailReason::CalibrationRaDriftTooShort:
        case GuideFailReason::CalibrationDecDriftTooShort:
        case GuideFailReason::CalibrationDriftTooShort:
        case GuideFailReason::CalibrationRaNoReturn:
        case GuideFailReason::CalibrationDecNoReturn:
        case GuideFailReason::CalibrationStarLost:
        case GuideFailReason::CalibrationStartFailed:
        case GuideFailReason::Phd2CalibrationFailed:
            return true;
        default:
            return false;
    }
}

bool isInternal(const GuideFacts &facts)
{
    return facts.guiderType == GuideDiagnosis::GUIDER_INTERNAL;
}

QString pulseDriftText(const GuideFacts &facts, const QString &axis)
{
    if (facts.failPulse > 0 && facts.failDrift >= 0)
        return i18nc("%3 is RA or DEC", "Pulses of %1 ms moved the star only %2 pixels in %3.", QString::number(facts.failPulse),
                     QString::number(facts.failDrift, 'f', 1), axis);
    return i18nc("%1 is RA or DEC", "The star barely moved while the mount was pulsed in %1.", axis);
}

// The steps every check of a mount axis shares
QStringList commonAxisSteps(const GuideFacts &facts)
{
    QStringList steps;
    steps << i18n("Check the mount power supply: a low voltage weakens the motors.")
          << i18n("Check that no cable drags or snags as the mount moves.");
    if (facts.guideViaCamera)
        steps << i18n("The pulses go through the camera %1: check the ST4 cable between the camera and the mount.",
                      facts.cameraName);
    steps << i18n("Check the guide rate in the mount driver or hand controller: 0.5x sidereal is typical.");
    return steps;
}

Item raAxisItem(const GuideFacts &facts, Likelihood likelihood)
{
    auto item = makeItem("check_ra_axis", Kind::Manual, Tier::Hardware, likelihood, i18n("Check the RA axis of the mount"),
                         pulseDriftText(facts, i18n("RA")));
    item.steps << i18n("Check the RA balance: make it slightly east-heavy so the gears stay engaged.")
               << i18n("Check that the RA clutch is locked.")
               << commonAxisSteps(facts);
    item.verify = makeVerify("retry");
    return item;
}

Item decAxisItem(const GuideFacts &facts, Likelihood likelihood)
{
    auto item = makeItem("check_dec_axis", Kind::Manual, Tier::Hardware, likelihood, i18n("Check the DEC axis of the mount"),
                         pulseDriftText(facts, i18n("DEC")));
    item.steps << i18n("Check the DEC balance.")
               << i18n("Check that the DEC clutch is locked.")
               << i18n("Check the DEC backlash: a large backlash takes up the pulses before the telescope moves.")
               << commonAxisSteps(facts);
    item.verify = makeVerify("retry");
    return item;
}

Item backlashItem(const GuideFacts &facts, const QString &axis)
{
    auto item = makeItem("check_backlash", Kind::Manual, Tier::Hardware, Likelihood::Likely,
                         i18nc("%1 is RA or DEC", "Check the backlash and balance of the %1 axis", axis),
                         facts.failDrift >= 0 ?
                         i18nc("%2 is RA or DEC", "The star stayed %1 pixels away from where the %2 calibration started.",
                               QString::number(facts.failDrift, 'f', 1), axis) :
                         i18nc("%1 is RA or DEC", "The star did not come back to where the %1 calibration started.", axis));
    item.steps << i18nc("%1 is RA or DEC", "Unbalance the %1 axis slightly, so its gears stay engaged in one direction.", axis)
               << i18n("Check the backlash adjustment of the mount (the gear mesh).")
               << i18n("Check that no cable pulls on the telescope.");
    item.verify = makeVerify("retry");
    return item;
}

Item pulseRaiseItem(const GuideFacts &facts, Likelihood likelihood)
{
    auto item = makeItem("guide_pulse_raise", Kind::Auto, Tier::Configuration, likelihood,
                         i18n("Raise the calibration pulse to %1 ms", QString::number(GuideDiagnosis::raisedPulse(facts.calibrationPulse))),
                         i18n("Longer pulses move the star further, past the backlash of the mount."));
    item.cost = ChangesSettings;
    item.verify = makeVerify("retry");
    return item;
}

Item skyItem(const GuideFacts &facts, Likelihood likelihood, const QString &why)
{
    auto item = makeItem("check_guide_sky", Kind::Manual, Tier::Data, likelihood, i18n("Check the sky and the guide camera"),
                         why);
    item.steps << i18n("Check for clouds, and for dew on the guide scope.")
               << i18n("Check that the guide camera is in focus.")
               << i18n("Check that the guide scope cap is off.");
    if (facts.guiderType == GuideDiagnosis::GUIDER_PHD2)
        item.steps << i18n("Check the star and its SNR in PHD2.");
    item.verify = makeVerify("retry");
    return item;
}

Item exposureItem(const GuideFacts &facts, Likelihood likelihood)
{
    auto item = makeItem("guide_exposure_raise", Kind::Auto, Tier::Data, likelihood,
                         i18n("Raise the guide exposure to %1 seconds",
                              QString::number(GuideDiagnosis::raisedExposure(facts.exposure), 'g', 3)),
                         i18n("A %1 second exposure may be too short to show a bright enough guide star.", facts.exposure));
    item.cost = ChangesSettings;
    item.verify = makeVerify("retry");
    return item;
}

Item binItem(Likelihood likelihood)
{
    auto item = makeItem("guide_bin_2x2", Kind::Auto, Tier::Data, likelihood, i18n("Bin the guide camera 2x2"),
                         i18n("Binning collects more light per pixel, so the guide star stands out from the noise."));
    item.cost = ChangesSettings;
    item.verify = makeVerify("retry");
    return item;
}

Item squareItem(const GuideFacts &facts, Likelihood likelihood)
{
    auto item = makeItem("guide_square_larger", Kind::Auto, Tier::Tuning, likelihood,
                         i18n("Enlarge the guide square to %1 pixels", facts.largerSquareSize),
                         i18n("A larger square keeps the star in view when it moves further between frames."));
    item.cost = ChangesSettings;
    item.verify = makeVerify("retry");
    return item;
}

void describeCause(const GuideFacts &facts, Report &report)
{
    switch (facts.reason)
    {
        case GuideFailReason::CalibrationRaDriftTooShort:
            report.title = i18n("The star barely moved in RA");
            report.summary =
                i18n("Calibration pulses the mount in RA and measures how far the star moves. It did not move far enough.");
            break;
        case GuideFailReason::CalibrationDecDriftTooShort:
            report.title = i18n("The star barely moved in DEC");
            report.summary =
                i18n("Calibration pulses the mount in DEC and measures how far the star moves. It did not move far enough.");
            break;
        case GuideFailReason::CalibrationDriftTooShort:
            report.title = i18n("The calibration drift is too short");
            report.summary = i18n("The star did not move far enough to measure how the mount responds to pulses.");
            break;
        case GuideFailReason::CalibrationRaNoReturn:
            report.title = i18n("The star did not come back in RA");
            report.summary = i18n("After pulsing RA out, the mount pulsed RA back, but the star did not return to its start.");
            break;
        case GuideFailReason::CalibrationDecNoReturn:
            report.title = i18n("The star did not come back in DEC");
            report.summary = i18n("After pulsing DEC out, the mount pulsed DEC back, but the star did not return to its start.");
            break;
        case GuideFailReason::CalibrationStarLost:
            report.title = i18n("The guide star was lost during calibration");
            report.summary = i18n("The guide star disappeared from several frames in a row.");
            break;
        case GuideFailReason::CalibrationStartFailed:
            report.title = i18n("Calibration could not start");
            report.summary = i18n("Guide needs the guide camera, the optics and the pixel size to calibrate.");
            break;
        case GuideFailReason::NoGuideStar:
            report.title = i18n("No guide star was found");
            report.summary = i18n("Guide could not select a guide star in the frame.");
            break;
        case GuideFailReason::StarLost:
            report.title = i18n("The guide star was lost");
            report.summary = i18n("The guide star disappeared while guiding and was not found again in %1 seconds.",
                                  QString::number(facts.lostStarTimeout, 'f', 0));
            break;
        case GuideFailReason::CaptureTimeout:
            report.title = i18n("The guide camera stopped responding");
            report.summary = i18n("Guide exposures timed out, also after the camera driver was restarted.");
            break;
        case GuideFailReason::CameraLost:
            report.title = i18n("The guide camera is gone");
            report.summary = i18n("The guide camera did not come back after its driver was restarted.");
            break;
        case GuideFailReason::DitherFailed:
            report.title = i18n("The dither did not settle");
            report.summary = i18n("Guiding was stopped because a dither failed, as the dither options say.");
            break;
        case GuideFailReason::Phd2CalibrationFailed:
            report.title = i18n("PHD2 failed to calibrate");
            report.summary = facts.detail.isEmpty() ? i18n("PHD2 reported a failed calibration.")
                             : i18n("PHD2 reported: %1", facts.detail);
            break;
        case GuideFailReason::None:
            report.title = i18n("Guiding failed");
            break;
    }
}

void addEvidence(const GuideFacts &facts, Report &report)
{
    auto add = [&report](const QString & key, const QString & label, const QVariant & value, const QString & unit,
                         Evidence::Level level)
    {
        Evidence evidence;
        evidence.key = key;
        evidence.label = label;
        evidence.value = value;
        evidence.unit = unit;
        evidence.level = level;
        report.evidence << evidence;
    };

    const auto reason = facts.reason;
    if (facts.hasCamera)
        add("camera", i18n("Guide camera"), facts.cameraName, QString(), facts.cameraConnected ? Evidence::Info : Evidence::Bad);
    if (facts.hasGuider)
        add("guide_via", i18n("Guide via"), facts.guiderName, QString(), Evidence::Info);
    if (!facts.detail.isEmpty())
        add("detail", i18n("Detail"), facts.detail, QString(), Evidence::Bad);

    if (facts.failDrift >= 0)
        add("drift", i18n("Star moved"), QString::number(facts.failDrift, 'f', 1), QStringLiteral("px"), Evidence::Bad);
    if (facts.failPulse > 0)
        add("pulse", i18n("Pulse"), facts.failPulse, QStringLiteral("ms"), Evidence::Info);

    if (reason == GuideFailReason::CalibrationStartFailed)
    {
        add("focal_length", i18n("Focal length"), facts.focalLength, QStringLiteral("mm"),
            facts.focalLength > 0 ? Evidence::Ok : Evidence::Bad);
        add("aperture", i18n("Aperture"), facts.aperture, QStringLiteral("mm"), facts.aperture > 0 ? Evidence::Ok : Evidence::Bad);
        add("pixel_size", i18n("Pixel size"), QString("%1 x %2").arg(facts.pixelW).arg(facts.pixelH), QStringLiteral("um"),
            facts.pixelW > 0 && facts.pixelH > 0 ? Evidence::Ok : Evidence::Bad);
    }

    if (GuideDiagnosis::isFrameReason(reason))
    {
        add("exposure", i18n("Exposure"), facts.exposure, QStringLiteral("s"), Evidence::Info);
        add("binning", i18n("Binning"), QString("%1x%1").arg(facts.binIndex + 1), QString(), Evidence::Info);
        if (facts.squareSize > 0)
            add("square", i18n("Guide square"), facts.squareSize, QStringLiteral("px"), Evidence::Info);
        if (facts.image && facts.image->verdict != ImageVerdict::Unknown)
            add("image", i18n("Image"), imageVerdictKey(facts.image->verdict), QString(),
                facts.image->verdict == ImageVerdict::Good ? Evidence::Ok : Evidence::Bad);
    }
}

/** Items from the star shapes of the guide frame. */
void frameItems(const GuideFacts &facts, QList<Item> &items, Report &report)
{
    if (!facts.image || facts.image->verdict == ImageVerdict::Unknown)
    {
        report.notChecked << i18n("Image: the guide frame was not measured.");
        return;
    }

    const auto &image = *facts.image;
    switch (image.verdict)
    {
        case ImageVerdict::Good:
            report.ruledOut << i18n("The guide frame has %1 round stars.", image.stars);
            break;
        case ImageVerdict::FewStars:
            items << skyItem(facts, Likelihood::Likely, i18n("Only %1 stars were found in the guide frame.", image.stars));
            break;
        case ImageVerdict::OutOfFocus:
        {
            auto item = makeItem("guide_focus", Kind::Manual, Tier::Data, Likelihood::Likely, i18n("Focus the guide camera"),
                                 i18n("The stars in the guide frame are discs or rings with a median HFR of %1 pixels.",
                                      QString::number(image.hfr, 'f', 1)));
            item.steps << i18n("Loop guide exposures and adjust the guide scope focus until the stars are small points.")
                       << i18n("Lock the focuser of the guide scope, so it does not slip.");
            item.verify = makeVerify("retry");
            items << item;
            break;
        }
        case ImageVerdict::Trailed:
        {
            auto item = makeItem("check_tracking", Kind::Manual, Tier::Data, Likelihood::Possible, i18n("Check the mount tracking"),
                                 i18n("All stars trailed %1 pixels in the same direction.", QString::number(image.trailLength, 'f', 0)));
            item.steps << i18n("Check that the mount is tracking at the sidereal rate.")
                       << i18n("Check the balance, and that nothing touches the telescope during the exposure, e.g. wind or cables.")
                       << i18n("Use a shorter guide exposure.");
            item.verify = makeVerify("retry");
            items << item;
            break;
        }
        case ImageVerdict::DoubleImages:
        {
            auto item = makeItem("check_mount_jump", Kind::Manual, Tier::Data, Likelihood::Possible,
                                 i18n("Check the mount for a sudden jump"),
                                 i18n("Every star appears twice, %1 pixels apart: the mount jumped during the exposure.",
                                      QString::number(image.doubleOffset, 'f', 0)));
            item.steps << i18n("Check the balance of both axes, and that the clutches are tight.")
                       << i18n("Check that no cable snags or pulls on the telescope.");
            item.verify = makeVerify("retry");
            items << item;
            break;
        }
        case ImageVerdict::Unknown:
            break;
    }
}

void calibrationItems(const GuideFacts &facts, const QStringList &skip, QList<Item> &items)
{
    const bool internal = isInternal(facts);
    const bool canRaisePulse = internal && GuideDiagnosis::raisedPulse(facts.calibrationPulse) > 0;

    switch (facts.reason)
    {
        case GuideFailReason::CalibrationRaDriftTooShort:
            items << raAxisItem(facts, Likelihood::Likely);
            if (canRaisePulse)
                items << pulseRaiseItem(facts, Likelihood::Possible);
            break;

        case GuideFailReason::CalibrationDecDriftTooShort:
            items << decAxisItem(facts, Likelihood::Likely);
            if (internal && !facts.decBacklashRemoval)
            {
                auto item = makeItem("guide_dec_backlash_on", Kind::Auto, Tier::Configuration, Likelihood::Possible,
                                     i18n("Remove the DEC backlash before calibrating"),
                                     i18n("Calibration then takes up the DEC backlash before it measures."));
                item.cost = ChangesSettings;
                item.verify = makeVerify("retry");
                items << item;
            }
            if (canRaisePulse)
                items << pulseRaiseItem(facts, Likelihood::Possible);
            break;

        case GuideFailReason::CalibrationDriftTooShort:
            items << raAxisItem(facts, Likelihood::Likely) << decAxisItem(facts, Likelihood::Likely);
            if (canRaisePulse)
                items << pulseRaiseItem(facts, Likelihood::Possible);
            break;

        case GuideFailReason::CalibrationRaNoReturn:
            items << backlashItem(facts, i18n("RA"));
            break;

        case GuideFailReason::CalibrationDecNoReturn:
            items << backlashItem(facts, i18n("DEC"));
            if (internal && !facts.decBacklashRemoval)
            {
                auto item = makeItem("guide_dec_backlash_on", Kind::Auto, Tier::Configuration, Likelihood::Likely,
                                     i18n("Remove the DEC backlash before calibrating"),
                                     i18n("Calibration then takes up the DEC backlash before it measures."));
                item.cost = ChangesSettings;
                item.verify = makeVerify("retry");
                items << item;
            }
            break;

        case GuideFailReason::CalibrationStarLost:
        {
            // The star moved out of the square, or faded
            if (internal && facts.largerSquareSize > 0)
                items << squareItem(facts, Likelihood::Possible);
            if (internal && GuideDiagnosis::loweredPulse(facts.calibrationPulse) > 0)
            {
                auto item = makeItem("guide_pulse_lower", Kind::Auto, Tier::Configuration, Likelihood::Possible,
                                     i18n("Lower the calibration pulse to %1 ms", QString::number(GuideDiagnosis::loweredPulse(facts.calibrationPulse))),
                                     i18n("Shorter pulses keep the star inside the guide square."));
                item.cost = ChangesSettings;
                item.verify = makeVerify("retry");
                items << item;
            }
            if (internal && GuideDiagnosis::raisedExposure(facts.exposure) > 0 && !skip.contains("guide_exposure_raise"))
                items << exposureItem(facts, Likelihood::Possible);
            break;
        }

        case GuideFailReason::CalibrationStartFailed:
        {
            if (!facts.hasCamera)
            {
                auto item = makeItem("train_select_camera", Kind::Open, Tier::Hardware, Likelihood::Likely,
                                     i18n("Select a guide camera in the optical train"), i18n("The guide optical train has no camera."));
                item.openTarget = QStringLiteral("optical_train");
                items << item;
            }
            if (!facts.hasGuider)
            {
                auto item = makeItem("train_select_guider", Kind::Open, Tier::Hardware, Likelihood::Likely,
                                     i18n("Select where the guide pulses go"),
                                     i18n("The guide optical train has no Guide Via device: select the mount, or the camera for an ST4 cable."));
                item.openTarget = QStringLiteral("optical_train");
                items << item;
            }
            if (facts.focalLength <= 0 || facts.aperture <= 0)
            {
                auto item = makeItem("train_set_optics", Kind::Open, Tier::Configuration, Likelihood::Likely,
                                     i18n("Set the guide scope focal length and aperture"),
                                     i18n("Focal length %1 mm, aperture %2 mm.", QString::number(facts.focalLength, 'f', 0),
                                          QString::number(facts.aperture, 'f', 0)));
                item.cost = ChangesSettings;
                item.openTarget = QStringLiteral("optical_train");
                items << item;
            }
            if (facts.hasCamera && (facts.pixelW <= 0 || facts.pixelH <= 0))
            {
                auto item = makeItem("camera_set_pixel_size", Kind::Open, Tier::Configuration, Likelihood::Likely,
                                     i18n("Set the guide camera pixel size"),
                                     i18n("The camera driver reports a pixel size of %1 x %2 um.", facts.pixelW, facts.pixelH));
                item.cost = ChangesSettings;
                item.openTarget = QString("indi_panel:%1:Image Info").arg(facts.cameraName);
                items << item;
            }
            break;
        }

        case GuideFailReason::Phd2CalibrationFailed:
        {
            auto item = makeItem("check_phd2", Kind::Manual, Tier::Configuration, Likelihood::Likely,
                                 i18n("Check the calibration in PHD2"),
                                 facts.detail.isEmpty() ? i18n("PHD2 reported a failed calibration.") : i18n("PHD2 reported: %1",
                                     facts.detail));
            item.steps << i18n("Read the calibration details in PHD2 (its calibration step graph and log).")
                       << i18n("Check that PHD2 found a guide star with a good SNR.")
                       << i18n("Check the camera, the pixel size and the focal length in the PHD2 equipment profile.");
            item.verify = makeVerify("retry");
            items << item;
            // PHD2 fails for the mount's reasons too
            items << raAxisItem(facts, Likelihood::Possible) << decAxisItem(facts, Likelihood::Possible);
            break;
        }

        default:
            break;
    }
}

void guidingItems(const GuideFacts &facts, const QStringList &skip, QList<Item> &items)
{
    const bool internal = isInternal(facts);

    switch (facts.reason)
    {
        case GuideFailReason::NoGuideStar:
            if (internal && GuideDiagnosis::raisedExposure(facts.exposure) > 0 && !skip.contains("guide_exposure_raise"))
                items << exposureItem(facts, Likelihood::Likely);
            if (internal && facts.canBin && facts.binIndex == 0 && facts.binCount > 1 && !skip.contains("guide_bin_2x2"))
                items << binItem(Likelihood::Possible);
            items << skyItem(facts, Likelihood::Possible, i18n("No star bright enough to guide on was found."));
            break;

        case GuideFailReason::StarLost:
        {
            items << skyItem(facts, Likelihood::Likely, i18n("Clouds, dew or a passing obstruction hide the guide star."));
            auto item = makeItem("check_guide_cables", Kind::Manual, Tier::Hardware, Likelihood::Possible,
                                 i18n("Check for cable snags, wind and flexure"),
                                 i18n("A sudden jump of the telescope moves the star out of the guide square."));
            item.steps << i18n("Check that no cable snags as the mount tracks.")
                       << i18n("Shield the telescope from the wind.")
                       << i18n("Check that the guide scope and camera are mounted firmly.");
            item.verify = makeVerify("retry");
            items << item;
            if (internal && GuideDiagnosis::raisedLostStarTimeout(facts.lostStarTimeout) > 0)
            {
                auto timeout = makeItem("guide_lost_star_timeout_raise", Kind::Auto, Tier::Tuning, Likelihood::Possible,
                                        i18n("Search for a lost star for %1 seconds",
                                             QString::number(GuideDiagnosis::raisedLostStarTimeout(facts.lostStarTimeout), 'f', 0)),
                                        i18n("Guide waits longer for a passing cloud before it stops."));
                timeout.cost = ChangesSettings;
                timeout.verify = makeVerify("retry");
                items << timeout;
            }
            if (internal && facts.largerSquareSize > 0)
                items << squareItem(facts, Likelihood::Possible);
            break;
        }

        case GuideFailReason::CaptureTimeout:
        case GuideFailReason::CameraLost:
        {
            if (facts.hasCamera && !facts.cameraConnected)
            {
                auto connect = makeItem("camera_connect", Kind::Auto, Tier::Hardware, Likelihood::Likely, i18n("Connect the guide camera"),
                                        i18n("The camera %1 is disconnected.", facts.cameraName));
                connect.verify = makeVerify("device_state", "connected", "==", 1);
                items << connect;
            }
            auto item = makeItem("check_camera_usb_power", Kind::Manual, Tier::Hardware, Likelihood::Likely,
                                 i18n("Check the guide camera cable and power"),
                                 facts.reason == GuideFailReason::CaptureTimeout ? i18n("Guide exposures timed out several times.")
                                 : i18n("The guide camera did not come back after a driver restart."));
            item.steps << i18n("Check the guide camera USB cable and use a powered hub if the camera shares one.")
                       << i18n("Use a shorter or better USB cable.")
                       << i18n("Restart the guide camera driver.");
            item.verify = makeVerify("retry");
            items << item;
            break;
        }

        case GuideFailReason::DitherFailed:
        {
            auto item = makeItem("check_dither_settle", Kind::Manual, Tier::Hardware, Likelihood::Likely,
                                 i18n("Check that guiding settles after a dither"),
                                 i18n("Guiding did not return within the settle limits after the mount was moved for the dither."));
            item.steps << i18n("Check the balance and the backlash of both axes: they slow down the return after a dither.")
                       << i18n("Check that guiding was steady before the dither.")
                       << i18n("Lower the dither distance in the dither options.");
            item.verify = makeVerify("retry");
            items << item;
            if (internal && GuideDiagnosis::raisedDitherIterations(facts.ditherMaxIterations) > 0)
            {
                auto iterations = makeItem("dither_iterations_raise", Kind::Auto, Tier::Tuning, Likelihood::Possible,
                                           i18n("Allow %1 dither iterations", GuideDiagnosis::raisedDitherIterations(facts.ditherMaxIterations)),
                                           i18n("The dither gets more guide frames to settle."));
                iterations.cost = ChangesSettings;
                iterations.verify = makeVerify("retry");
                items << iterations;
            }
            if (facts.ditherFailAborts)
            {
                auto keep = makeItem("dither_continue", Kind::Auto, Tier::Tuning, Likelihood::Possible,
                                     i18n("Keep guiding when a dither fails"),
                                     i18n("A failed dither then only costs one frame instead of stopping guiding."));
                keep.cost = ChangesSettings;
                keep.verify = makeVerify("retry");
                items << keep;
            }
            break;
        }

        default:
            break;
    }
}

void addRuledOut(const GuideFacts &facts, Report &report)
{
    if (facts.hasCamera && facts.cameraConnected && facts.reason != GuideFailReason::CameraLost
            && facts.reason != GuideFailReason::CaptureTimeout)
        report.ruledOut << i18n("The guide camera %1 is connected.", facts.cameraName);
    if (facts.hasMount && facts.mountConnected)
        report.ruledOut << i18n("The mount is connected.");
}

}

QString guideFailReasonKey(GuideFailReason reason)
{
    switch (reason)
    {
        case GuideFailReason::None:
            return QStringLiteral("none");
        case GuideFailReason::CalibrationRaDriftTooShort:
            return QStringLiteral("calibration_ra_drift_too_short");
        case GuideFailReason::CalibrationDecDriftTooShort:
            return QStringLiteral("calibration_dec_drift_too_short");
        case GuideFailReason::CalibrationDriftTooShort:
            return QStringLiteral("calibration_drift_too_short");
        case GuideFailReason::CalibrationRaNoReturn:
            return QStringLiteral("calibration_ra_no_return");
        case GuideFailReason::CalibrationDecNoReturn:
            return QStringLiteral("calibration_dec_no_return");
        case GuideFailReason::CalibrationStarLost:
            return QStringLiteral("calibration_star_lost");
        case GuideFailReason::CalibrationStartFailed:
            return QStringLiteral("calibration_start_failed");
        case GuideFailReason::NoGuideStar:
            return QStringLiteral("no_guide_star");
        case GuideFailReason::StarLost:
            return QStringLiteral("star_lost");
        case GuideFailReason::CaptureTimeout:
            return QStringLiteral("capture_timeout");
        case GuideFailReason::CameraLost:
            return QStringLiteral("camera_lost");
        case GuideFailReason::DitherFailed:
            return QStringLiteral("dither_failed");
        case GuideFailReason::Phd2CalibrationFailed:
            return QStringLiteral("phd2_calibration_failed");
    }
    return QStringLiteral("none");
}

bool GuideDiagnosis::isDiagnosable(GuideFailReason reason)
{
    return reason != GuideFailReason::None;
}

bool GuideDiagnosis::isFrameReason(GuideFailReason reason)
{
    return reason == GuideFailReason::NoGuideStar || reason == GuideFailReason::StarLost
           || reason == GuideFailReason::CalibrationStarLost;
}

int GuideDiagnosis::raisedPulse(int pulse)
{
    if (pulse <= 0)
        return 0;
    const int raised = std::min(pulse * 2, MAX_CALIBRATION_PULSE);
    return raised > pulse ? raised : 0;
}

int GuideDiagnosis::loweredPulse(int pulse)
{
    if (pulse <= 0)
        return 0;
    const int lowered = std::max(pulse / 2, MIN_CALIBRATION_PULSE);
    return lowered < pulse ? lowered : 0;
}

double GuideDiagnosis::raisedExposure(double exposure)
{
    if (exposure <= 0)
        return 0;
    const double raised = std::min(MAX_GUIDE_EXPOSURE, std::max(1.0, exposure * 2));
    return raised > exposure ? raised : 0;
}

double GuideDiagnosis::raisedLostStarTimeout(double timeout)
{
    const double raised = std::min(MAX_LOST_STAR_TIMEOUT, std::max(30.0, timeout * 2));
    return raised > timeout ? raised : 0;
}

int GuideDiagnosis::raisedDitherIterations(int iterations)
{
    if (iterations <= 0)
        return 0;
    const int raised = std::min(iterations * 2, MAX_DITHER_ITERATIONS);
    return raised > iterations ? raised : 0;
}

Report GuideDiagnosis::diagnose(const GuideFacts &facts, const QStringList &tried, const QStringList &dismissed)
{
    Report report;
    report.module = QStringLiteral("guide");
    report.train = facts.train;
    report.state = facts.state;
    report.reason = static_cast<int>(facts.reason);
    report.reasonKey = guideFailReasonKey(facts.reason);
    report.tried = tried;

    describeCause(facts, report);
    addEvidence(facts, report);
    addRuledOut(facts, report);

    const QStringList skip = tried + dismissed;

    QList<Item> items;
    if (isCalibrationFailure(facts.reason))
        calibrationItems(facts, skip, items);
    else
        guidingItems(facts, skip, items);
    if (isFrameReason(facts.reason))
        frameItems(facts, items, report);

    QStringList seen;
    for (const auto &item : items)
    {
        if (seen.contains(item.id) || skip.contains(item.id))
            continue;
        seen << item.id;
        report.items << item;
    }

    if (report.items.isEmpty())
    {
        auto item = makeItem("guide_check_log", Kind::Info, Tier::Tuning, Likelihood::Check, i18n("Check the Guide log"),
                             i18n("No specific cause was found."));
        item.blocker = false;
        item.steps << i18n("Check the balance of both axes, the clutches and the cables.")
                   << i18n("Enable Guide Logging in Setup Tab -> Logs to get details on the failure.");
        report.items << item;
    }

    order(report.items);
    return report;
}

Report GuideDiagnosis::checkup(const GuideFacts &facts, const QStringList &dismissed)
{
    Report report;
    report.module = QStringLiteral("guide");
    report.train = facts.train;
    report.state = facts.state;
    report.reason = static_cast<int>(GuideFailReason::None);
    report.reasonKey = checkupReasonKey();

    QList<Item> items;
    const bool internal = isInternal(facts);

    // PHD2 and LinGuider use their own camera and optics
    if (internal)
    {
        if (!facts.hasCamera)
        {
            auto item = makeItem("train_select_camera", Kind::Open, Tier::Hardware, Likelihood::Likely,
                                 i18n("Select a guide camera in the optical train"), i18n("The guide optical train has no camera."));
            item.openTarget = QStringLiteral("optical_train");
            items << item;
        }
        else if (!facts.cameraConnected)
        {
            auto item = makeItem("camera_connect", Kind::Auto, Tier::Hardware, Likelihood::Likely, i18n("Connect the guide camera"),
                                 i18n("The camera %1 is disconnected.", facts.cameraName));
            item.verify = makeVerify("device_state", "connected", "==", 1);
            items << item;
        }
        else
            report.ruledOut << i18n("The guide camera %1 is connected.", facts.cameraName);

        if (!facts.hasGuider)
        {
            auto item = makeItem("train_select_guider", Kind::Open, Tier::Hardware, Likelihood::Likely,
                                 i18n("Select where the guide pulses go"),
                                 i18n("The guide optical train has no Guide Via device: select the mount, or the camera for an ST4 cable."));
            item.openTarget = QStringLiteral("optical_train");
            items << item;
        }
        else
            report.ruledOut << i18n("Guide pulses go to %1.", facts.guiderName);

        if (facts.focalLength <= 0 || facts.aperture <= 0)
        {
            auto item = makeItem("train_set_optics", Kind::Open, Tier::Configuration, Likelihood::Likely,
                                 i18n("Set the guide scope focal length and aperture"),
                                 i18n("Focal length %1 mm, aperture %2 mm.", QString::number(facts.focalLength, 'f', 0),
                                      QString::number(facts.aperture, 'f', 0)));
            item.cost = ChangesSettings;
            item.openTarget = QStringLiteral("optical_train");
            items << item;
        }

        // The guide rate the last calibration measured, in DEC, which does not depend on the declination
        if (facts.decRate > 0)
        {
            const double multiple = facts.decRate / SIDEREAL_RATE;
            if (multiple < MIN_GUIDE_RATE)
            {
                auto item = makeItem("check_guide_rate", Kind::Manual, Tier::Configuration, Likelihood::Possible,
                                     i18n("Raise the guide rate of the mount"),
                                     i18n("The last calibration measured a guide rate of %1x sidereal: the star barely moves for each pulse.",
                                          QString::number(multiple, 'f', 2)));
                item.steps << i18n("Set the guide rate to 0.5x sidereal in the mount driver or hand controller.")
                           << i18n("Calibrate again.");
                item.verify = makeVerify("retry");
                items << item;
            }
            else
                report.ruledOut << i18n("The last calibration measured a guide rate of %1x sidereal.", QString::number(multiple, 'f',
                                        2));
        }
    }

    if (facts.hasMount && !facts.mountConnected)
    {
        auto item = makeItem("mount_connect", Kind::Auto, Tier::Hardware, Likelihood::Likely, i18n("Connect the mount"),
                             i18n("The mount is disconnected."));
        item.verify = makeVerify("device_state", "connected", "==", 1);
        items << item;
    }

    QStringList seen;
    for (auto item : items)
    {
        if (seen.contains(item.id) || dismissed.contains(item.id))
            continue;
        seen << item.id;
        // Nothing failed: applying a suggestion never starts guiding by itself
        item.autoRetry = false;
        report.items << item;
    }
    order(report.items);

    if (report.items.isEmpty())
    {
        report.title = i18n("Guide looks ready");
        report.summary = i18n("No problems were found in the Guide setup.");
    }
    else
    {
        report.title = i18np("%1 thing to check", "%1 things to check", report.items.size());
        report.summary = i18n("Start with the first suggestion, it is the most likely to matter.");
    }
    return report;
}

}
