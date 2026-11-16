/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "focusdiagnosis.h"

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

/** Autofocus ran but did not find the focus: the curve, the steps or the travel. */
bool isCurveFailure(AutofocusFailReason reason)
{
    switch (reason)
    {
        case FOCUS_FAIL_MAX_ITERS:
        case FOCUS_FAIL_FOCUSER_OOB:
        case FOCUS_FAIL_FLUCTUATIONS:
        case FOCUS_FAIL_DEADLOCK:
        case FOCUS_FAIL_SMALL_HFR:
        case FOCUS_FAIL_R2:
        case FOCUS_FAIL_CURVEFIT:
        case FOCUS_FAIL_TOLERANCE:
            return true;
        default:
            return false;
    }
}

bool isCaptureFailure(AutofocusFailReason reason)
{
    return reason == FOCUS_FAIL_CAPTURE_FAILED || reason == FOCUS_FAIL_CAPTURE_TIMEOUT;
}

bool isFocuserFailure(AutofocusFailReason reason)
{
    return reason == FOCUS_FAIL_FOCUSER_ERROR || reason == FOCUS_FAIL_FOCUSER_NO_MOVE;
}

QString algorithmName(int algorithm)
{
    switch (algorithm)
    {
        case 0:
            return QStringLiteral("Iterative");
        case 1:
            return QStringLiteral("Polynomial");
        case 2:
            return QStringLiteral("Linear");
        case FocusDiagnosis::ALGORITHM_LINEAR1PASS:
            return QStringLiteral("Linear 1 Pass");
        default:
            return QString::number(algorithm);
    }
}

bool hasRange(const FocusFacts &facts)
{
    return facts.canAbsMove && facts.focuserMax > facts.focuserMin;
}

int knownStars(const FocusFacts &facts)
{
    if (facts.image)
        return facts.image->stars;
    return facts.stars;
}

// Items shared by the failure rules and the check-up

Item selectCameraItem()
{
    auto item = makeItem("train_select_camera", Kind::Open, Tier::Hardware, Likelihood::Likely,
                         i18n("Select a camera in the optical train"), i18n("The optical train of Focus has no camera."));
    item.openTarget = QStringLiteral("optical_train");
    return item;
}

Item connectCameraItem(const FocusFacts &facts)
{
    auto item = makeItem("camera_connect", Kind::Auto, Tier::Hardware, Likelihood::Likely, i18n("Connect the camera"),
                         i18n("The camera %1 is disconnected.", facts.cameraName));
    item.verify = makeVerify("device_state", "connected", "==", 1);
    return item;
}

Item selectFocuserItem()
{
    auto item = makeItem("train_select_focuser", Kind::Open, Tier::Hardware, Likelihood::Likely,
                         i18n("Select a focuser in the optical train"), i18n("The optical train of Focus has no focuser."));
    item.openTarget = QStringLiteral("optical_train");
    return item;
}

Item connectFocuserItem(const FocusFacts &facts)
{
    auto item = makeItem("focuser_connect", Kind::Auto, Tier::Hardware, Likelihood::Likely, i18n("Connect the focuser"),
                         i18n("The focuser %1 is disconnected.", facts.focuserName));
    item.verify = makeVerify("device_state", "connected", "==", 1);
    return item;
}

Item connectFilterWheelItem()
{
    auto item = makeItem("filter_connect", Kind::Auto, Tier::Hardware, Likelihood::Likely, i18n("Connect the filter wheel"),
                         i18n("The filter wheel is disconnected."));
    item.verify = makeVerify("device_state", "connected", "==", 1);
    return item;
}

/** The step at which autofocus refuses to start with a focuser that moves by time. */
bool pulseTooShort(const FocusFacts &facts)
{
    return facts.hasFocuser && !facts.canAbsMove && !facts.canRelMove && facts.stepSize <= FOCUS_MINIMUM_PULSE_TIMER;
}

Item pulseStepItem(const FocusFacts &facts)
{
    auto item = makeItem("focus_step_min", Kind::Auto, Tier::Configuration, Likelihood::Likely,
                         i18n("Raise the step size to %1 ms", FocusDiagnosis::SUGGESTED_PULSE_STEP),
                         i18n("The focuser moves by time, and a step of %1 ms is too short to move it.", facts.stepSize));
    item.cost = ChangesSettings;
    item.verify = makeVerify("retry");
    return item;
}

Item exposureItem(const FocusFacts &facts, Likelihood likelihood, const QString &why)
{
    auto item = makeItem("focus_exposure_raise", Kind::Auto, Tier::Data, likelihood,
                         i18n("Raise the Focus exposure to %1 seconds",
                              QString::number(FocusDiagnosis::suggestedExposure(facts.exposure), 'g', 3)), why);
    item.cost = ChangesSettings;
    item.verify = makeVerify("retry");
    return item;
}

bool canRaiseExposure(const FocusFacts &facts)
{
    return facts.exposure > 0 && facts.exposure < FocusDiagnosis::MAX_SUGGESTED_EXPOSURE;
}

bool canBin(const FocusFacts &facts)
{
    return facts.canBin && facts.binIndex == 0 && facts.binCount > 1;
}

Item binItem(Likelihood likelihood)
{
    auto item = makeItem("focus_bin_2x2", Kind::Auto, Tier::Data, likelihood, i18n("Bin the Focus camera 2x2"),
                         i18n("Binning collects more light per pixel, so fainter stars are detected."));
    item.cost = ChangesSettings;
    item.verify = makeVerify("retry");
    return item;
}

Item nearFocusItem(Likelihood likelihood, const QString &why)
{
    auto item = makeItem("focus_near_focus", Kind::Manual, Tier::Data, likelihood, i18n("Move the focuser close to focus"),
                         why);
    item.steps << i18n("Point the telescope at a bright star, and loop short exposures from the Focus module.")
               << i18n("Move the focuser in or out until the star is small and sharp.")
               << i18n("Run autofocus again from there.");
    item.verify = makeVerify("retry");
    return item;
}

Item advisorItem(Likelihood likelihood)
{
    auto item = makeItem("focus_advisor", Kind::Open, Tier::Tuning, likelihood, i18n("Run Focus Advisor"),
                         i18n("Focus Advisor finds the focus area and sets the step size, backlash and overscan for this focuser."));
    item.openTarget = QStringLiteral("focus_advisor");
    return item;
}

Item stepItem(const FocusFacts &facts, Likelihood likelihood, const QString &why)
{
    auto item = makeItem("focus_step_raise", Kind::Auto, Tier::Tuning, likelihood,
                         i18n("Raise the initial step size to %1", FocusDiagnosis::suggestedStepSize(facts)), why);
    item.cost = ChangesSettings;
    item.verify = makeVerify("retry");
    return item;
}

Item conditionsItem(Likelihood likelihood, const QString &why)
{
    auto item = makeItem("check_conditions", Kind::Manual, Tier::Data, likelihood, i18n("Check the sky and the seeing"),
                         why);
    item.steps << i18n("Check for clouds, haze or dew on the optics.")
               << i18n("Check that wind does not shake the telescope.")
               << i18n("In poor seeing, use longer exposures so each measurement averages more of it.");
    item.verify = makeVerify("retry");
    return item;
}

void describeCause(const FocusFacts &facts, Report &report)
{
    switch (facts.reason)
    {
        case FOCUS_FAIL_NO_STARS:
            report.title = i18n("No stars were found");
            report.summary = i18n("Autofocus needs stars in the focus frame to measure.");
            break;
        case FOCUS_FAIL_MAX_ITERS:
            report.title = i18n("Autofocus did not converge");
            report.summary = i18n("Autofocus stopped after too many steps without finding the focus.");
            break;
        case FOCUS_FAIL_FOCUSER_NO_MOVE:
            report.title = i18n("The focuser did not move");
            report.summary = i18n("Autofocus asked the focuser to move, but the move failed.");
            break;
        case FOCUS_FAIL_R2:
            report.title = i18n("The focus curve is a poor fit");
            report.summary = i18n("The measurements scatter too much around the fitted curve.");
            break;
        case FOCUS_FAIL_SMALL_HFR:
            report.title = i18n("The star size barely changed");
            report.summary = i18n("The steps are too small to show a change in the star size.");
            break;
        case FOCUS_FAIL_FOCUSER_OOB:
            report.title = i18n("The focuser reached its limit");
            report.summary = i18n("Autofocus needed to move past the maximum travel or the focuser limits.");
            break;
        case FOCUS_FAIL_FLUCTUATIONS:
            report.title = i18n("The measurements fluctuate");
            report.summary = i18n("The star size went up and down too often to find a trend.");
            break;
        case FOCUS_FAIL_DEADLOCK:
            report.title = i18n("Autofocus is stuck");
            report.summary = i18n("Autofocus kept moving back and forth around the same positions.");
            break;
        case FOCUS_FAIL_TOLERANCE:
            report.title = i18n("The tolerance is not valid");
            report.summary = i18n("The tolerance of the autofocus algorithm is out of range.");
            break;
        case FOCUS_FAIL_CURVEFIT:
            report.title = i18n("No focus curve could be fitted");
            report.summary = i18n("The measurements do not form a focus curve.");
            break;
        case FOCUS_FAIL_FOCUSER_ERROR:
            report.title = i18n("The focuser reported an error");
            report.summary = i18n("The focuser %1 stopped responding during autofocus.", facts.focuserName);
            break;
        case FOCUS_FAIL_FILTER_MANAGER:
            report.title = i18n("The filter change failed");
            report.summary = i18n("The filter wheel did not reach the focus filter.");
            break;
        case FOCUS_FAIL_CAPTURE_TIMEOUT:
            report.title = i18n("The camera stopped responding");
            report.summary = i18n("A focus exposure timed out.");
            break;
        case FOCUS_FAIL_CAPTURE_FAILED:
            report.title = i18n("The camera failed to capture");
            report.summary = i18n("A focus exposure failed.");
            break;
        case FOCUS_FAIL_NO_CAMERA:
            report.title = i18n("No camera is available");
            report.summary = i18n("Focus has no connected camera to capture with.");
            break;
        case FOCUS_FAIL_NO_FOCUSER:
            report.title = i18n("No focuser is available");
            report.summary = i18n("Focus has no connected focuser to move.");
            break;
        case FOCUS_FAIL_LOW_PULSE:
            report.title = i18n("The step size is too small");
            report.summary = i18n("The focuser moves by time, and the step is too short to move it.");
            break;
        case FOCUS_FAIL_DUSTCAP_ERROR:
            report.title = i18n("The dust cap reports an error");
            report.summary = i18n("Autofocus cannot capture until the dust cap is open.");
            break;
        default:
            report.title = i18n("Autofocus failed");
            break;
    }
}

void addEvidence(const FocusFacts &facts, Report &report)
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
        add("camera", i18n("Camera"), facts.cameraName, QString(),
            facts.cameraConnected ? Evidence::Info : Evidence::Bad);
    if (facts.hasFocuser)
        add("focuser", i18n("Focuser"), facts.focuserName, QString(),
            facts.focuserConnected ? Evidence::Info : Evidence::Bad);

    if (!facts.reasonInfo.isEmpty())
        add("detail", i18n("Detail"), facts.reasonInfo, QString(), Evidence::Bad);

    if (reason == FOCUS_FAIL_NO_STARS || isCurveFailure(reason))
    {
        add("exposure", i18n("Exposure"), facts.exposure, QStringLiteral("s"), Evidence::Info);
        add("binning", i18n("Binning"), QString("%1x%1").arg(facts.binIndex + 1), QString(), Evidence::Info);
        if (facts.gain >= 0)
            add("gain", i18n("Gain"), facts.gain, QString(), Evidence::Info);

        const int stars = knownStars(facts);
        if (stars >= 0)
            add("stars", i18n("Stars found"), stars, QString(), stars == 0 ? Evidence::Bad : Evidence::Info);
        if (facts.hfr > 0)
            add("hfr", i18n("Last HFR"), QString::number(facts.hfr, 'f', 2), QStringLiteral("px"), Evidence::Info);
        if (facts.image && facts.image->verdict != ImageVerdict::Unknown)
            add("image", i18n("Image"), imageVerdictKey(facts.image->verdict), QString(),
                facts.image->verdict == ImageVerdict::Good ? Evidence::Ok : Evidence::Bad);
    }

    if (isCurveFailure(reason) || reason == FOCUS_FAIL_LOW_PULSE)
    {
        add("algorithm", i18n("Algorithm"), algorithmName(facts.algorithm), QString(), Evidence::Info);
        add("step_size", i18n("Initial step size"), facts.stepSize, facts.canAbsMove
            || facts.canRelMove ? QStringLiteral("steps") : QStringLiteral("ms"),
            reason == FOCUS_FAIL_SMALL_HFR || reason == FOCUS_FAIL_LOW_PULSE ? Evidence::Bad : Evidence::Info);
    }

    if (isCurveFailure(reason) && facts.canAbsMove)
    {
        add("max_travel", i18n("Maximum travel"), facts.maxTravel, QStringLiteral("steps"),
            reason == FOCUS_FAIL_FOCUSER_OOB ? Evidence::Bad : Evidence::Info);
        if (facts.startPosition >= 0)
            add("start_position", i18n("Start position"), facts.startPosition, QStringLiteral("steps"), Evidence::Info);
        if (hasRange(facts))
            add("focuser_range", i18n("Focuser range"), QString("%1 - %2").arg(facts.focuserMin, 0, 'f', 0)
                .arg(facts.focuserMax, 0, 'f', 0), QStringLiteral("steps"), Evidence::Info);
    }
}

void deviceItems(const FocusFacts &facts, QList<Item> &items)
{
    switch (facts.reason)
    {
        case FOCUS_FAIL_NO_CAMERA:
            items << (facts.hasCamera ? connectCameraItem(facts) : selectCameraItem());
            break;

        case FOCUS_FAIL_NO_FOCUSER:
            items << (facts.hasFocuser ? connectFocuserItem(facts) : selectFocuserItem());
            break;

        case FOCUS_FAIL_FOCUSER_ERROR:
        case FOCUS_FAIL_FOCUSER_NO_MOVE:
        {
            auto item = makeItem("check_focuser", Kind::Manual, Tier::Hardware, Likelihood::Likely, i18n("Check the focuser"),
                                 facts.reason == FOCUS_FAIL_FOCUSER_ERROR ? i18n("The focuser %1 stopped responding.", facts.focuserName)
                                 : i18n("The focuser %1 did not move as asked.", facts.focuserName));
            item.steps << i18n("Move the focuser in and out with the buttons of the Focus module, and check that its position changes.")
                       << i18n("Check the focuser cable and power. A USB port or hub that cannot supply enough power stalls the motor.")
                       << i18n("Check the maximum position and the limits set in the focuser driver.")
                       << i18n("Restart the focuser driver if it still does not move.");
            item.cost = MovesHardware;
            item.verify = makeVerify("retry");
            items << item;
            break;
        }

        case FOCUS_FAIL_CAPTURE_FAILED:
        case FOCUS_FAIL_CAPTURE_TIMEOUT:
        {
            auto item = makeItem("check_camera_usb_power", Kind::Manual, Tier::Hardware, Likelihood::Likely,
                                 i18n("Check the camera cable and power"),
                                 facts.reason == FOCUS_FAIL_CAPTURE_TIMEOUT ? i18n("A focus exposure of %1 seconds timed out.",
                                     facts.exposure) : i18n("A focus exposure failed."));
            item.steps << i18n("Check the camera USB cable and use a powered hub if the camera shares one.")
                       << i18n("Check the camera power supply.")
                       << i18n("Take a test exposure from the Capture module, and restart the camera driver if it fails.");
            item.verify = makeVerify("retry");
            items << item;
            break;
        }

        case FOCUS_FAIL_FILTER_MANAGER:
        {
            if (facts.hasFilterWheel && !facts.filterWheelConnected)
                items << connectFilterWheelItem();
            auto item = makeItem("filter_test_move", Kind::Manual, Tier::Hardware, Likelihood::Likely, i18n("Check the filter wheel"),
                                 i18n("The filter wheel did not reach the focus filter."));
            item.steps << i18n("Change the filter from the Filter Manager and check that it arrives.")
                       << i18n("Check that the filter selected for focus exists in the filter wheel.")
                       << i18n("Check the filter wheel cable and power.");
            item.cost = MovesHardware;
            item.verify = makeVerify("retry");
            items << item;
            break;
        }

        case FOCUS_FAIL_DUSTCAP_ERROR:
        {
            auto item = makeItem("remove_dust_cap", Kind::Manual, Tier::Hardware, Likelihood::Likely, i18n("Open the dust cap"),
                                 i18n("The dust cap reports an error."));
            item.steps << i18n("Open or remove the dust cap by hand.")
                       << i18n("Check the dust cap cable and power.");
            item.verify = makeVerify("retry");
            items << item;
            break;
        }

        case FOCUS_FAIL_LOW_PULSE:
            items << pulseStepItem(facts);
            break;

        default:
            break;
    }

    // A device that dropped out explains a failure further down the line too
    if ((isCaptureFailure(facts.reason) || facts.reason == FOCUS_FAIL_NO_STARS) && facts.hasCamera
            && !facts.cameraConnected)
        items.prepend(connectCameraItem(facts));
    if ((isFocuserFailure(facts.reason) || isCurveFailure(facts.reason)) && facts.hasFocuser && !facts.focuserConnected)
        items.prepend(connectFocuserItem(facts));
}

/** Items from the star shapes of the failed frame. */
void frameItems(const FocusFacts &facts, QList<Item> &items, Report &report, bool signalOffered)
{
    if (!facts.image || facts.image->verdict == ImageVerdict::Unknown)
    {
        report.notChecked << i18n("Image: the star shapes in the failed frame were not measured.");
        return;
    }

    const auto &image = *facts.image;
    switch (image.verdict)
    {
        case ImageVerdict::Good:
            report.ruledOut << i18n("The frame has %1 round stars with a median HFR of %2 pixels.", image.stars,
                                    QString::number(image.hfr, 'f', 1));
            break;

        case ImageVerdict::FewStars:
        {
            auto item = makeItem("check_sky_and_cap", Kind::Manual, Tier::Data,
                                 signalOffered ? Likelihood::Possible : Likelihood::Likely, i18n("Check the sky and the dust cap"),
                                 i18n("Only %1 stars were found in the focus frame.", image.stars));
            if (facts.hasDustCap)
                item.steps << i18n("Check that the dust cap is open.");
            else
                item.steps << i18n("Check that the dust cap or cover is off.");
            item.steps << i18n("Check for clouds, dew on the optics, or an obstruction in front of the telescope.")
                       << i18n("Point the telescope at an area with more stars.");
            item.verify = makeVerify("test_frame", "stars", ">=", 1);
            items << item;
            break;
        }

        case ImageVerdict::OutOfFocus:
        {
            // Rings or flat discs: the focuser starts far from focus
            const QString why = image.centreDip < ImageAnalysis::MAX_FOCUSED_CENTRE_DIP ?
                                i18n("The stars are rings, as stars are far from focus. Their median HFR is %1 pixels.",
                                     QString::number(image.hfr, 'f', 1)) :
                                i18n("The stars are flat discs, as stars are far from focus. Their median HFR is %1 pixels.",
                                     QString::number(image.hfr, 'f', 1));
            items << nearFocusItem(Likelihood::Likely, why);
            if (facts.canAbsMove)
                items << advisorItem(Likelihood::Possible);
            if (facts.algorithm == FocusDiagnosis::ALGORITHM_LINEAR1PASS && !facts.donutBuster)
            {
                auto item = makeItem("focus_donut_buster", Kind::Auto, Tier::Tuning, Likelihood::Possible,
                                     i18n("Turn on Donut Buster"),
                                     i18n("Donut Buster measures defocused stars that look like rings."));
                item.cost = ChangesSettings;
                item.verify = makeVerify("retry");
                items << item;
            }
            break;
        }

        case ImageVerdict::Trailed:
        {
            auto item = makeItem("check_tracking", Kind::Manual, Tier::Data, Likelihood::Possible, i18n("Check the mount tracking"),
                                 i18n("All stars trailed %1 pixels in the same direction.", QString::number(image.trailLength, 'f', 0)));
            item.steps << i18n("Check that the mount is tracking at the sidereal rate.")
                       << i18n("Check the balance, and that nothing touches the telescope during the exposure, e.g. wind or cables.")
                       << i18n("Use a shorter exposure.");
            item.verify = makeVerify("test_frame", "elongation", "<", ImageAnalysis::MIN_TRAIL_ELONGATION);
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
            item.verify = makeVerify("test_frame", "double_fraction", "<", ImageAnalysis::MIN_DOUBLE_FRACTION);
            items << item;
            break;
        }

        case ImageVerdict::Unknown:
            break;
    }
}

void noStarsItems(const FocusFacts &facts, const QStringList &skip, QList<Item> &items, Report &report)
{
    // Defocused stars are there but too faint to detect: exposure and binning come second then
    const bool defocused = facts.image && facts.image->verdict == ImageVerdict::OutOfFocus;
    const Likelihood signal = defocused ? Likelihood::Possible : Likelihood::Likely;

    bool signalOffered = false;
    if (canRaiseExposure(facts) && !skip.contains("focus_exposure_raise"))
    {
        items << exposureItem(facts, signal, i18n("No stars were detected in a %1 second exposure.", facts.exposure));
        signalOffered = true;
    }
    if (canBin(facts) && !skip.contains("focus_bin_2x2"))
    {
        items << binItem(signal);
        signalOffered = true;
    }

    // Unmeasured frame: the sky and the cap are the usual cause
    if (!facts.image || facts.image->verdict == ImageVerdict::Unknown)
    {
        auto item = makeItem("check_sky_and_cap", Kind::Manual, Tier::Data,
                             signalOffered ? Likelihood::Possible : Likelihood::Likely, i18n("Check the sky and the dust cap"),
                             i18n("No stars were found in the focus frame."));
        item.steps << i18n("Check that the dust cap or cover is off.")
                   << i18n("Check for clouds, dew on the optics, or an obstruction in front of the telescope.")
                   << i18n("Check that the focuser is not far from focus: stars far from focus are too faint to detect.");
        item.verify = makeVerify("test_frame", "stars", ">=", 1);
        items << item;
    }

    frameItems(facts, items, report, signalOffered);
}

void curveItems(const FocusFacts &facts, const QStringList &skip, QList<Item> &items, Report &report)
{
    const auto reason = facts.reason;
    const bool advisor = facts.canAbsMove;

    // The focuser itself: it slips or has backlash, so it does not reach the positions autofocus asks for
    if (reason == FOCUS_FAIL_MAX_ITERS || reason == FOCUS_FAIL_FLUCTUATIONS || reason == FOCUS_FAIL_DEADLOCK
            || reason == FOCUS_FAIL_CURVEFIT || reason == FOCUS_FAIL_R2)
    {
        auto item = makeItem("check_focuser_mechanics", Kind::Manual, Tier::Hardware, Likelihood::Possible,
                             i18n("Check the focuser for slip and backlash"),
                             i18n("A focuser that slips under the weight of the camera, or has backlash, does not reach the positions autofocus asks for."));
        item.steps << i18n("Tighten the focuser tension or lock screw, so it holds the weight of the camera.")
                   << i18n("Measure the backlash and set it in the focuser driver or the Focus mechanics settings. Focus Advisor finds a value.")
                   << i18n("Check that no cable drags on the focuser or the camera.");
        item.verify = makeVerify("retry");
        items << item;
    }

    switch (reason)
    {
        case FOCUS_FAIL_FOCUSER_OOB:
        {
            const double travel = FocusDiagnosis::suggestedMaxTravel(facts);
            if (travel > 0)
            {
                auto item = makeItem("focus_max_travel_raise", Kind::Auto, Tier::Configuration, Likelihood::Likely,
                                     i18n("Raise the maximum travel to %1 steps", QString::number(travel, 'f', 0)),
                                     i18n("Autofocus may move at most %1 steps, and the focuser can move from %2 to %3.",
                                          QString::number(facts.maxTravel, 'f', 0), QString::number(facts.focuserMin, 'f', 0),
                                          QString::number(facts.focuserMax, 'f', 0)));
                item.cost = ChangesSettings;
                item.verify = makeVerify("retry");
                items << item;
            }
            items << nearFocusItem(travel > 0 ? Likelihood::Possible : Likelihood::Likely,
                                   i18n("Autofocus started at %1 and needed to move past the limit.", facts.startPosition));
            if (advisor)
                items << advisorItem(Likelihood::Possible);
            break;
        }

        case FOCUS_FAIL_MAX_ITERS:
            items << nearFocusItem(Likelihood::Likely, i18n("Autofocus took too many steps: it likely started far from focus."));
            if (advisor)
                items << advisorItem(Likelihood::Possible);
            break;

        case FOCUS_FAIL_SMALL_HFR:
            if (FocusDiagnosis::suggestedStepSize(facts) > 0)
                items << stepItem(facts, Likelihood::Likely,
                                  i18n("Steps of %1 do not change the star size enough to measure.", facts.stepSize));
            if (advisor)
                items << advisorItem(Likelihood::Possible);
            break;

        case FOCUS_FAIL_FLUCTUATIONS:
        case FOCUS_FAIL_DEADLOCK:
        case FOCUS_FAIL_CURVEFIT:
            if (advisor)
                items << advisorItem(Likelihood::Likely);
            if (FocusDiagnosis::suggestedStepSize(facts) > 0)
                items << stepItem(facts, Likelihood::Possible,
                                  i18n("Larger steps change the star size more than the noise of the measurements."));
            if (canRaiseExposure(facts) && !skip.contains("focus_exposure_raise"))
                items << exposureItem(facts, Likelihood::Possible,
                                      i18n("A longer exposure averages the seeing and gives steadier measurements."));
            items << conditionsItem(Likelihood::Possible, i18n("Clouds, poor seeing or wind make the measurements scatter."));
            break;

        case FOCUS_FAIL_R2:
            items << conditionsItem(Likelihood::Likely, i18n("Clouds, poor seeing or wind make the measurements scatter."));
            if (canRaiseExposure(facts) && !skip.contains("focus_exposure_raise"))
                items << exposureItem(facts, Likelihood::Possible,
                                      i18n("A longer exposure averages the seeing and gives steadier measurements."));
            if (advisor)
                items << advisorItem(Likelihood::Possible);
            break;

        case FOCUS_FAIL_TOLERANCE:
            if (advisor)
                items << advisorItem(Likelihood::Likely);
            break;

        default:
            break;
    }

    // The frame can show what the curve can't, e.g. rings far from focus or a trailed frame
    frameItems(facts, items, report, false);
}

void addRuledOut(const FocusFacts &facts, Report &report)
{
    const bool devicesFailed = facts.reason == FOCUS_FAIL_NO_CAMERA || facts.reason == FOCUS_FAIL_NO_FOCUSER;
    if (!devicesFailed && facts.hasCamera && facts.cameraConnected && facts.hasFocuser && facts.focuserConnected)
        report.ruledOut.prepend(i18n("The camera and the focuser are connected."));
    if (isCurveFailure(facts.reason) && facts.stars > 0)
        report.ruledOut << i18n("The focus frame has stars.");
}

}

QString focusFailReasonKey(AutofocusFailReason reason)
{
    switch (reason)
    {
        case FOCUS_FAIL_NONE:
            return QStringLiteral("none");
        case FOCUS_FAIL_NO_STARS:
            return QStringLiteral("no_stars");
        case FOCUS_FAIL_MAX_ITERS:
            return QStringLiteral("max_iterations");
        case FOCUS_FAIL_FOCUSER_NO_MOVE:
            return QStringLiteral("focuser_no_move");
        case FOCUS_FAIL_R2:
            return QStringLiteral("r2");
        case FOCUS_FAIL_SMALL_HFR:
            return QStringLiteral("small_hfr_change");
        case FOCUS_FAIL_FOCUSER_OOB:
            return QStringLiteral("focuser_out_of_bounds");
        case FOCUS_FAIL_FLUCTUATIONS:
            return QStringLiteral("fluctuations");
        case FOCUS_FAIL_DEADLOCK:
            return QStringLiteral("deadlock");
        case FOCUS_FAIL_TOLERANCE:
            return QStringLiteral("tolerance");
        case FOCUS_FAIL_FOCUSER_ERROR:
            return QStringLiteral("focuser_error");
        case FOCUS_FAIL_FORCE_ABORT:
            return QStringLiteral("force_abort");
        case FOCUS_FAIL_FILTER_MANAGER:
            return QStringLiteral("filter_manager");
        case FOCUS_FAIL_CAPTURE_TIMEOUT:
            return QStringLiteral("capture_timeout");
        case FOCUS_FAIL_CAPTURE_FAILED:
            return QStringLiteral("capture_failed");
        case FOCUS_FAIL_NO_CAMERA:
            return QStringLiteral("no_camera");
        case FOCUS_FAIL_NO_FOCUSER:
            return QStringLiteral("no_focuser");
        case FOCUS_FAIL_LOW_PULSE:
            return QStringLiteral("low_pulse");
        case FOCUS_FAIL_INTERNAL:
            return QStringLiteral("internal");
        case FOCUS_FAIL_ABORT:
            return QStringLiteral("abort");
        case FOCUS_FAIL_CURVEFIT:
            return QStringLiteral("curve_fit");
        case FOCUS_FAIL_ADVISOR_COMPLETE:
            return QStringLiteral("advisor_complete");
        case FOCUS_FAIL_ADVISOR_RERUN:
            return QStringLiteral("advisor_rerun");
        case FOCUS_FAIL_OPTIMISED_OUT:
            return QStringLiteral("optimised_out");
        case FOCUS_FAIL_DUSTCAP_ERROR:
            return QStringLiteral("dustcap_error");
        case FOCUS_FAIL_MAX_REASONS:
            break;
    }
    return QStringLiteral("none");
}

bool FocusDiagnosis::isDiagnosable(AutofocusFailReason reason)
{
    switch (reason)
    {
        case FOCUS_FAIL_NONE:
        case FOCUS_FAIL_ABORT:
        case FOCUS_FAIL_FORCE_ABORT:
        case FOCUS_FAIL_INTERNAL:
        case FOCUS_FAIL_ADVISOR_COMPLETE:
        case FOCUS_FAIL_ADVISOR_RERUN:
        case FOCUS_FAIL_OPTIMISED_OUT:
        case FOCUS_FAIL_MAX_REASONS:
            return false;
        default:
            return true;
    }
}

bool FocusDiagnosis::isFrameReason(AutofocusFailReason reason)
{
    return reason == FOCUS_FAIL_NO_STARS || isCurveFailure(reason);
}

AutofocusFailReason FocusDiagnosis::reportedReason(AutofocusFailReason failCode, AutofocusFailReason attemptReason)
{
    if (failCode == FOCUS_FAIL_FORCE_ABORT && attemptReason != FOCUS_FAIL_NONE)
        return attemptReason;
    return failCode;
}

double FocusDiagnosis::suggestedExposure(double exposure)
{
    return std::min(MAX_SUGGESTED_EXPOSURE, std::max(1.0, exposure * 2));
}

int FocusDiagnosis::suggestedStepSize(const FocusFacts &facts)
{
    if (facts.stepSize <= 0)
        return 0;
    int step = facts.stepSize * 2;
    if (facts.maxSingleStep > 0)
        step = std::min(step, facts.maxSingleStep);
    return step > facts.stepSize ? step : 0;
}

double FocusDiagnosis::suggestedMaxTravel(const FocusFacts &facts)
{
    if (!hasRange(facts) || facts.maxTravel <= 0)
        return 0;
    const double travel = std::min(facts.maxTravel * 2, facts.focuserMax - facts.focuserMin);
    return travel > facts.maxTravel ? travel : 0;
}

Report FocusDiagnosis::diagnose(const FocusFacts &facts, const QStringList &tried, const QStringList &dismissed)
{
    Report report;
    report.module = QStringLiteral("focus");
    report.train = facts.train;
    report.state = facts.state;
    report.reason = static_cast<int>(facts.reason);
    report.reasonKey = focusFailReasonKey(facts.reason);
    report.tried = tried;

    describeCause(facts, report);
    addEvidence(facts, report);
    addRuledOut(facts, report);

    const QStringList skip = tried + dismissed;

    QList<Item> items;
    deviceItems(facts, items);
    if (facts.reason == FOCUS_FAIL_NO_STARS)
        noStarsItems(facts, skip, items, report);
    else if (isCurveFailure(facts.reason))
        curveItems(facts, skip, items, report);

    // Never offer the same item twice, nor one already tried or dismissed
    QStringList seen;
    for (auto item : items)
    {
        if (seen.contains(item.id) || skip.contains(item.id))
            continue;
        seen << item.id;
        // A sequence or the scheduler decides what happens after a failed autofocus
        if (facts.inSequence)
            item.autoRetry = false;
        report.items << item;
    }

    if (report.items.isEmpty())
    {
        auto item = makeItem("focus_check_log", Kind::Info, Tier::Tuning, Likelihood::Check, i18n("Check the Focus log"),
                             i18n("No specific cause was found."));
        item.blocker = false;
        item.steps << i18n("Check that the focus frame shows stars, and that they get smaller and larger as the focuser moves.")
                   << i18n("Enable Focus Logging in Setup Tab -> Logs to get details on the failure.");
        report.items << item;
    }

    order(report.items);
    return report;
}

Report FocusDiagnosis::checkup(const FocusFacts &facts, const QStringList &dismissed)
{
    Report report;
    report.module = QStringLiteral("focus");
    report.train = facts.train;
    report.state = facts.state;
    report.reason = static_cast<int>(FOCUS_FAIL_NONE);
    report.reasonKey = checkupReasonKey();

    QList<Item> items;
    if (!facts.hasCamera)
        items << selectCameraItem();
    else if (!facts.cameraConnected)
        items << connectCameraItem(facts);
    else
        report.ruledOut << i18n("The camera %1 is connected.", facts.cameraName);

    if (!facts.hasFocuser)
        items << selectFocuserItem();
    else if (!facts.focuserConnected)
        items << connectFocuserItem(facts);
    else
        report.ruledOut << i18n("The focuser %1 is connected.", facts.focuserName);

    if (facts.hasFilterWheel && !facts.filterWheelConnected)
        items << connectFilterWheelItem();

    if (pulseTooShort(facts))
        items << pulseStepItem(facts);

    QStringList seen;
    for (auto item : items)
    {
        if (seen.contains(item.id) || dismissed.contains(item.id))
            continue;
        seen << item.id;
        // Nothing failed: applying a suggestion never starts autofocus by itself
        item.autoRetry = false;
        report.items << item;
    }
    order(report.items);

    if (report.items.isEmpty())
    {
        report.title = i18n("Focus looks ready");
        report.summary = i18n("No problems were found in the Focus setup.");
    }
    else
    {
        report.title = i18np("%1 thing to check", "%1 things to check", report.items.size());
        report.summary = i18n("Start with the first suggestion, it is the most likely to matter.");
    }
    return report;
}

}
