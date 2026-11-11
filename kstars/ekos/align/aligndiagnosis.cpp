/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "aligndiagnosis.h"
#include "indexfiles.h"

#include <KLocalizedString>

#include <algorithm>
#include <cmath>

namespace Ekos
{

using namespace Diagnosis;

namespace
{
// Same values as Align::GotoMode
constexpr int GOTO_SYNC = 0;
constexpr int GOTO_SLEW = 1;

Item makeItem(const QString &id, Kind kind, Tier tier, const QString &title, const QString &why)
{
    Item item;
    item.id = id;
    item.kind = kind;
    item.tier = tier;
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

bool isSolveFailure(AlignFailReason reason)
{
    return reason == AlignFailReason::SolverFailed || reason == AlignFailReason::SolverTimeout ||
           reason == AlignFailReason::PahSolverFailed || reason == AlignFailReason::NoIndexFiles;
}

bool isPointingFailure(AlignFailReason reason)
{
    return isSolveFailure(reason) || reason == AlignFailReason::SyncAlert || reason == AlignFailReason::SlewAlert ||
           reason == AlignFailReason::AccuracyNotMet;
}

// Failures that happen after the camera, optics and pixel size checks passed
bool passedSetupChecks(AlignFailReason reason)
{
    switch (reason)
    {
        case AlignFailReason::None:
        case AlignFailReason::NoMountCoords:
        case AlignFailReason::NoCamera:
        case AlignFailReason::CameraDisconnected:
        case AlignFailReason::MissingOptics:
        case AlignFailReason::MissingPixelSize:
        case AlignFailReason::FovOutOfBounds:
        case AlignFailReason::UserAbort:
            return false;
        default:
            return true;
    }
}

QString sizeString(int size)
{
    return QString("%1").arg(size, 2, 10, QLatin1Char('0'));
}

QString seriesList(const QList<int> &sizes)
{
    QStringList names;
    for (int size : sizes)
        names << QString::number(IndexFiles::installSeries(size));
    return names.join(QLatin1String(", "));
}

int knownStars(const AlignFacts &facts)
{
    if (facts.image)
        return facts.image->stars;
    if (!facts.attempts.isEmpty())
        return facts.attempts.last().starsFound;
    return -1;
}

// Items shared by the failure rules and the check-up

Item selectCameraItem()
{
    auto item = makeItem("train_select_camera", Kind::Open, Tier::Hardware, i18n("Select a camera in the optical train"),
                         i18n("The optical train of Align has no camera."));
    item.likelihood = Likelihood::Likely;
    item.openTarget = QStringLiteral("optical_train");
    return item;
}

Item connectCameraItem(const AlignFacts &facts)
{
    auto item = makeItem("camera_connect", Kind::Auto, Tier::Hardware, i18n("Connect the camera"),
                         i18n("The camera %1 is disconnected.", facts.cameraName));
    item.likelihood = Likelihood::Likely;
    item.verify = makeVerify("device_state", "connected", "==", 1);
    return item;
}

Item connectMountItem()
{
    auto item = makeItem("mount_connect", Kind::Auto, Tier::Hardware, i18n("Connect the mount"),
                         i18n("The mount is disconnected, so its position is unknown."));
    item.likelihood = Likelihood::Likely;
    item.verify = makeVerify("device_state", "connected", "==", 1);
    return item;
}

Item mountCoordsItem()
{
    auto item = makeItem("mount_check_coords", Kind::Manual, Tier::Hardware, i18n("Check the mount connection"),
                         i18n("The mount is connected but sends no valid coordinates."));
    item.likelihood = Likelihood::Likely;
    item.steps << i18n("Open the mount control panel and check that it shows RA and DEC.")
               << i18n("If it does not, disconnect and reconnect the mount.");
    item.verify = makeVerify("retry");
    return item;
}

Item opticsItem(const AlignFacts &facts, Likelihood likelihood)
{
    auto item = makeItem("train_set_optics", Kind::Open, Tier::Configuration,
                         i18n("Set the telescope focal length and aperture"),
                         i18n("Focal length %1 mm, aperture %2 mm.", QString::number(facts.focalLength, 'f', 0),
                              QString::number(facts.aperture, 'f', 0)));
    item.likelihood = likelihood;
    item.cost = ChangesSettings;
    item.openTarget = QStringLiteral("optical_train");
    item.verify = makeVerify("retry");
    return item;
}

Item pixelSizeItem(const AlignFacts &facts, Likelihood likelihood)
{
    auto item = makeItem("camera_set_pixel_size", Kind::Open, Tier::Configuration, i18n("Set the camera pixel size"),
                         i18n("The camera driver reports a pixel size of %1 x %2 um.", facts.pixelW, facts.pixelH));
    item.likelihood = likelihood;
    item.cost = ChangesSettings;
    item.openTarget = QString("indi_panel:%1:Image Info").arg(facts.cameraName);
    item.verify = makeVerify("retry");
    return item;
}

Item externalSolverItem()
{
    auto item = makeItem("solver_check_external", Kind::Open, Tier::Configuration, i18n("Install or locate the solver program"),
                         i18n("The selected external solver program was not found."));
    item.likelihood = Likelihood::Likely;
    item.openTarget = QStringLiteral("align_settings:solver");
    return item;
}

// Index files: every size the field needs, as the Index Files page marks them. A field can hold quads of
// the larger sizes and still need the smaller ones, especially when it has few stars.
void indexItems(const AlignFacts &facts, QList<Item> &items, Report &report)
{
    if (!facts.indexSolver || facts.requiredSizes.isEmpty())
        return;

    QList<int> missing;
    for (int size : facts.requiredSizes)
    {
        if (!facts.installedSizes.contains(size))
            missing << size;
    }

    if (missing.isEmpty())
    {
        report.ruledOut << i18n("Index files for this field of view are installed.");
        return;
    }

    for (int size : missing)
    {
        const int series = IndexFiles::installSeries(size);
        auto item = makeItem(QString("index_install_%1").arg(sizeString(size)), Kind::Auto, Tier::Data,
                             // A series is a name, not a number to format with digit grouping
                             i18n("Install index files %1", QString::number(series)),
                             i18n("Your %1 x %2 arcmin field needs index files %3; %4 are missing or incomplete.",
                                  QString::number(facts.fovW, 'f', 1), QString::number(facts.fovH, 'f', 1),
                                  seriesList(facts.requiredSizes), seriesList(missing)));
        // Without the index files of its field, the solver cannot solve it
        item.likelihood = Likelihood::Likely;
        item.cost = NeedsInternet;
        item.downloadBytes = IndexFiles::estimatedDownloadBytes(size);
        item.verify = makeVerify("index_scan", "series_installed", "==", 1);
        items << item;
    }
}

bool canRaiseExposure(const AlignFacts &facts)
{
    return facts.exposure < AlignDiagnosis::SUGGESTED_EXPOSURE;
}

bool canBinMore(const AlignFacts &facts)
{
    return facts.canBin && facts.binIndex == 0 && facts.binCount > 1;
}

/**
 * @param stars stars found in the failed frame, or -1 if unknown. A known low count makes it a likely cause.
 */
Item exposureItem(const AlignFacts &facts, int stars)
{
    auto item = makeItem("align_exposure_5s", Kind::Auto, Tier::Data,
                         i18n("Raise the Align exposure to %1 seconds", AlignDiagnosis::SUGGESTED_EXPOSURE),
                         stars >= 0 ? i18n("Only %1 stars were found in a %2 second exposure.", stars, facts.exposure)
                         : i18n("A %1 second exposure may be too short to show enough stars.", facts.exposure));
    item.likelihood = stars >= 0 ? Likelihood::Likely : Likelihood::Possible;
    item.cost = ChangesSettings;
    item.verify = makeVerify("retry");
    return item;
}

/** @param starsKnown the failed frame had too few stars */
Item binItem(const AlignFacts &facts, bool starsKnown)
{
    auto item = makeItem("align_bin_max", Kind::Auto, Tier::Data, i18n("Bin the Align camera %1x%1", facts.binCount),
                         i18n("Binning collects more light per pixel and speeds up solving."));
    item.likelihood = starsKnown ? Likelihood::Likely : Likelihood::Possible;
    item.cost = ChangesSettings;
    item.verify = makeVerify("retry");
    return item;
}

bool customProfile(const AlignFacts &facts)
{
    return !facts.profile.isEmpty() && facts.profile != AlignDiagnosis::defaultProfileName();
}

// The default profile sizes its search from the field of view, so it suits every field
Item profileItem(const AlignFacts &facts)
{
    auto item = makeItem("solver_profile_default", Kind::Auto, Tier::Tuning, i18n("Use the default solver profile"),
                         i18n("The solver profile %1 is selected. The default profile adapts its search to your field of view.",
                              facts.profile));
    item.likelihood = Likelihood::Possible;
    item.cost = ChangesSettings;
    item.verify = makeVerify("retry");
    return item;
}

bool narrowField(const AlignFacts &facts)
{
    return facts.fovW > 0 && facts.fovH > 0 && std::min(facts.fovW, facts.fovH) < AlignDiagnosis::NARROW_FOV_ARCMIN;
}

Item narrowFovItem(const AlignFacts &facts)
{
    auto item = makeItem("fov_narrow", Kind::Info, Tier::Tuning, i18n("The field of view is narrow"),
                         i18n("A %1 x %2 arcmin field holds few stars. Use binning and longer exposures, or solve with a wider camera.",
                              QString::number(facts.fovW, 'f', 1), QString::number(facts.fovH, 'f', 1)));
    item.likelihood = Likelihood::Check;
    item.blocker = false;
    return item;
}

bool unusualReducer(const AlignFacts &facts)
{
    return facts.reducer > 0 && (facts.reducer < 0.5 || facts.reducer > 2);
}

Item reducerItem(const AlignFacts &facts)
{
    auto item = makeItem("train_check_reducer", Kind::Open, Tier::Tuning, i18n("Check the reducer setting"),
                         i18n("The optical train uses a reducer of %1x. A wrong value gives the solver a wrong field of view.",
                              QString::number(facts.reducer, 'f', 2)));
    item.likelihood = Likelihood::Possible;
    item.blocker = false;
    item.openTarget = QStringLiteral("optical_train");
    return item;
}

bool timeSourceIsKStars(const AlignFacts &facts)
{
    return facts.timeUpdate && facts.timeSource == QLatin1String("KStars");
}

/**
 * The KStars clock against the client device (or this computer), then the mount clock.
 * Needs a connected mount.
 */
void clockChecks(const AlignFacts &facts, QList<Item> &items, Report &report)
{
    // The client time is compared only, never applied (Q3).
    const QDateTime reference = facts.clientUtc ? *facts.clientUtc : facts.systemUtc;
    if (reference.isValid() && facts.kstarsUtc.isValid())
    {
        const double kstarsDiff = std::abs(facts.kstarsUtc.secsTo(reference));
        if (kstarsDiff > AlignDiagnosis::MAX_CLOCK_DIFF_SEC)
        {
            auto item = makeItem("kstars_clock_wrong", Kind::Manual, Tier::Configuration, i18n("Check the KStars clock"),
                                 facts.clientUtc ?
                                 i18n("The KStars time differs from your device by %1 seconds.", QString::number(kstarsDiff, 'f', 0)) :
                                 i18n("The KStars time differs from this computer by %1 seconds.", QString::number(kstarsDiff, 'f', 0)));
            item.likelihood = Likelihood::Possible;
            item.steps << i18n("Check the date, time and time zone of the computer that runs KStars.")
                       << i18n("Check the location and time zone set in KStars.");
            // KStars takes its time from a device: a wrong device clock makes the KStars clock wrong
            if (facts.timeUpdate && !timeSourceIsKStars(facts))
                item.steps <<
                           i18n("KStars takes its time from %1. Check the clock of that device, or set the time source to KStars in the INDI settings.",
                                facts.timeSource);
            item.verify = makeVerify("retry");
            items << item;
            // Do not send a wrong KStars time to the mount: the KStars clock comes first
            return;
        }
    }

    if (!facts.mountUtc || !facts.mountUtcReceived.isValid())
    {
        report.notChecked << i18n("Mount clock: the mount does not report its time.");
        return;
    }

    // Most drivers report their time once, e.g. when KStars sends it: compare the reported time with when it
    // arrived, not with the current time
    const double mountDiff = std::abs(facts.mountUtc->secsTo(facts.mountUtcReceived));
    if (mountDiff <= AlignDiagnosis::MAX_CLOCK_DIFF_SEC)
    {
        report.ruledOut << i18n("The mount clock matches this computer.");
        return;
    }

    if (timeSourceIsKStars(facts))
    {
        auto item = makeItem("mount_time_sync", Kind::Auto, Tier::Configuration, i18n("Send the KStars time to the mount"),
                             i18n("The mount clock was %1 seconds off when it last reported its time.",
                                  QString::number(mountDiff, 'f', 0)));
        item.likelihood = Likelihood::Possible;
        item.verify = makeVerify("retry");
        items << item;
    }
    else
    {
        // KStars does not keep the mount time: send it once and keep sending it from now on
        auto item = makeItem("mount_time_source_kstars", Kind::Auto, Tier::Configuration,
                             i18n("Let KStars keep the mount time"),
                             i18n("The mount clock was %1 seconds off when it last reported its time, and KStars does not send its time to the mount.",
                                  QString::number(mountDiff, 'f', 0)));
        item.likelihood = Likelihood::Possible;
        item.cost = ChangesSettings;
        item.verify = makeVerify("retry");
        items << item;
    }
}

void describeCause(const AlignFacts &facts, Report &report)
{
    const QString gotoAction = facts.gotoMode == GOTO_SLEW ? i18n("slew to the target") : i18n("sync");
    switch (facts.reason)
    {
        case AlignFailReason::NoMountCoords:
            report.title = i18n("The mount position is unknown");
            report.summary = i18n("Align needs the mount position to set its target, but the mount reports no valid coordinates.");
            break;
        case AlignFailReason::NoCamera:
            report.title = i18n("No camera is selected");
            report.summary = i18n("The optical train has no camera for Align to capture with.");
            break;
        case AlignFailReason::CameraDisconnected:
            report.title = i18n("The camera is disconnected");
            report.summary = i18n("Align lost the connection to the camera %1.", facts.cameraName);
            break;
        case AlignFailReason::MissingOptics:
            report.title = i18n("The focal length or aperture is missing");
            report.summary = i18n("The optical train has no focal length or aperture, so the field of view is unknown.");
            break;
        case AlignFailReason::MissingPixelSize:
            report.title = i18n("The camera pixel size is missing");
            report.summary = i18n("The camera driver reports no pixel size, so the field of view is unknown.");
            break;
        case AlignFailReason::FovOutOfBounds:
            report.title = i18n("The field of view is out of bounds");
            report.summary = i18n("The calculated field of view is %1 x %2 arcmin.",
                                  QString::number(facts.fovW, 'f', 1), QString::number(facts.fovH, 'f', 1));
            break;
        case AlignFailReason::FilterWheelDisconnected:
            report.title = i18n("The filter wheel is disconnected");
            report.summary = i18n("Align could not select its filter.");
            break;
        case AlignFailReason::FilterChangeFailed:
            report.title = i18n("The filter change failed");
            report.summary = i18n("The filter wheel did not reach the Align filter.");
            break;
        case AlignFailReason::BlobDisabled:
            report.title = i18n("Image transfer is disabled");
            report.summary = i18n("The camera does not send its images to KStars.");
            break;
        case AlignFailReason::DustCapError:
            report.title = i18n("The dust cap reports an error");
            report.summary = i18n("Align cannot capture until the dust cap is open.");
            break;
        case AlignFailReason::CaptureError:
            report.title = i18n("The camera failed to capture");
            report.summary = i18n("The exposure failed %1 times.", facts.captureErrors);
            break;
        case AlignFailReason::CaptureTimeout:
            report.title = i18n("The camera stopped responding");
            report.summary = i18n("Exposures timed out after the camera driver was restarted %1 times.", facts.driverRestarts);
            break;
        case AlignFailReason::BusyTimeout:
            report.title = i18n("Another device stayed busy");
            report.summary = i18n("Align waited too long for another device to finish.");
            break;
        case AlignFailReason::NoIndexFiles:
            report.title = i18n("No index files are installed");
            report.summary = i18n("The solver needs index files for this field of view.");
            break;
        case AlignFailReason::SolverFailed:
            report.title = i18n("The image could not be solved");
            report.summary = i18np("The solver found no solution after %1 attempt.",
                                   "The solver found no solution after %1 attempts.", std::max<int>(1, facts.attempts.size()));
            break;
        case AlignFailReason::SolverTimeout:
            report.title = i18n("The solver timed out");
            report.summary = i18n("The solver did not finish within %1 seconds.", QString::number(facts.timeoutSec, 'f', 0));
            break;
        case AlignFailReason::PahSolverFailed:
            report.title = i18n("Polar alignment could not solve the image");
            report.summary = i18n("The polar alignment assistant needs a solved image at each step.");
            break;
        case AlignFailReason::RemoteSolverTimeout:
            report.title = i18n("The remote solver timed out");
            report.summary = i18n("The remote astrometry driver sent no solution.");
            break;
        case AlignFailReason::SyncRejected:
            report.title = i18n("The mount rejected the sync");
            report.summary = i18n("The image was solved, but the mount did not accept the solved position.");
            break;
        case AlignFailReason::SyncAlert:
            report.title = i18n("The mount failed to sync");
            report.summary = i18n("The image was solved, but the mount reported an error %1 times while syncing.",
                                  facts.slewErrors);
            break;
        case AlignFailReason::SlewRejected:
            report.title = i18n("The slew was rejected");
            report.summary = i18n("The image was solved, but KStars refused to slew to the target.");
            break;
        case AlignFailReason::SlewAlert:
            report.title = i18n("The mount failed to slew");
            report.summary = i18n("The image was solved, but the mount reported an error %1 times while slewing.",
                                  facts.slewErrors);
            break;
        case AlignFailReason::MountParked:
            report.title = i18n("The mount is parked");
            report.summary = i18nc("%1 is the action, e.g. sync", "Align was asked to %1, but the mount is parked.", gotoAction);
            break;
        case AlignFailReason::RotatorAlert:
            report.title = i18n("The rotator reports an error");
            report.summary = i18n("The rotator stopped with an error while turning the camera.");
            break;
        case AlignFailReason::RotatorNotReached:
            report.title = i18n("The rotator did not reach the position angle");
            report.summary = i18n("The camera position angle is outside the rotator threshold of %1 arcmin.",
                                  QString::number(facts.rotatorThresholdArcmin, 'f', 1));
            break;
        case AlignFailReason::RotatorTimeout:
            report.title = i18n("The rotator timed out");
            report.summary = i18n("The rotator did not finish turning in time.");
            break;
        case AlignFailReason::RotatorWrongDirection:
            report.title = i18n("The rotator turns the wrong way");
            report.summary = i18n("The position angle error grew after the rotation, also with the direction reversed.");
            break;
        case AlignFailReason::AccuracyNotMet:
            report.title = i18n("The target accuracy was not reached");
            report.summary = i18n("Solving works, but the mount did not get close enough to the target.");
            break;
        case AlignFailReason::SlewEndedIdle:
            report.title = i18n("The mount stopped without tracking");
            report.summary = i18n("The image was solved, but the mount stopped after the slew and did not start tracking.");
            break;
        case AlignFailReason::None:
        case AlignFailReason::UserAbort:
            report.title = i18n("Align failed");
            break;
    }
}

void addEvidence(const AlignFacts &facts, Report &report)
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
            reason == AlignFailReason::CameraDisconnected ? Evidence::Bad : Evidence::Info);

    if (reason == AlignFailReason::MountParked)
        add("park_status", i18n("Mount"), QStringLiteral("parked"), QString(), Evidence::Bad);

    if (facts.fovW > 0 && facts.fovH > 0)
        add("fov", i18n("Field of view"), QString("%1 x %2").arg(facts.fovW, 0, 'f', 1).arg(facts.fovH, 0, 'f', 1),
            QStringLiteral("arcmin"), reason == AlignFailReason::FovOutOfBounds ? Evidence::Bad : Evidence::Info);

    if (reason == AlignFailReason::MissingOptics || reason == AlignFailReason::FovOutOfBounds)
    {
        add("focal_length", i18n("Focal length"), facts.focalLength, QStringLiteral("mm"),
            facts.focalLength > 0 ? Evidence::Ok : Evidence::Bad);
        add("aperture", i18n("Aperture"), facts.aperture, QStringLiteral("mm"),
            facts.aperture > 0 ? Evidence::Ok : Evidence::Bad);
    }

    if (reason == AlignFailReason::MissingPixelSize || reason == AlignFailReason::FovOutOfBounds)
        add("pixel_size", i18n("Pixel size"), QString("%1 x %2").arg(facts.pixelW).arg(facts.pixelH), QStringLiteral("um"),
            facts.pixelW > 0 && facts.pixelH > 0 ? Evidence::Ok : Evidence::Bad);

    if (isSolveFailure(reason))
    {
        add("exposure", i18n("Exposure"), facts.exposure, QStringLiteral("s"),
            facts.exposure < AlignDiagnosis::SUGGESTED_EXPOSURE ? Evidence::Bad : Evidence::Info);
        add("binning", i18n("Binning"), QString("%1x%1").arg(facts.binIndex + 1), QString(), Evidence::Info);

        const int stars = knownStars(facts);
        if (stars >= 0)
            add("stars", i18n("Stars found"), stars, QString(),
                stars < AlignDiagnosis::MIN_STARS ? Evidence::Bad : Evidence::Ok);

        if (facts.image && facts.image->hfr > 0)
            add("hfr", i18n("Median HFR"), QString::number(facts.image->hfr, 'f', 2), QStringLiteral("px"), Evidence::Info);
        if (facts.image && facts.image->verdict != ImageVerdict::Unknown)
        {
            const auto &image = *facts.image;
            add("image", i18n("Image"), imageVerdictKey(image.verdict), QString(),
                image.verdict == ImageVerdict::Good ? Evidence::Ok : Evidence::Bad);
            add("elongation", i18n("Star elongation"), QString::number(image.elongation, 'f', 2), QString(),
                image.verdict == ImageVerdict::Trailed ? Evidence::Bad : Evidence::Info);
            if (image.verdict == ImageVerdict::Trailed)
            {
                add("trail_length", i18n("Trail length"), QString::number(image.trailLength, 'f', 1), QStringLiteral("px"),
                    Evidence::Bad);
                add("trail_angle", i18n("Trail direction"), QString::number(image.angle, 'f', 0), QStringLiteral("deg"),
                    Evidence::Info);
            }
            if (image.verdict == ImageVerdict::DoubleImages)
                add("double_offset", i18n("Double image offset"), QString::number(image.doubleOffset, 'f', 1),
                    QStringLiteral("px"), Evidence::Bad);
            const bool outOfFocus = image.verdict == ImageVerdict::OutOfFocus;
            if (image.concentration > 0)
                add("concentration", i18n("Star concentration (R80/R20)"), QString::number(image.concentration, 'f', 2),
                    QString(), outOfFocus && image.concentration < ImageAnalysis::MIN_FOCUSED_CONCENTRATION ? Evidence::Bad : Evidence::Info);
            if (outOfFocus && image.centreDip < ImageAnalysis::MAX_FOCUSED_CENTRE_DIP)
                add("centre_dip", i18n("Star centre brightness"), QString::number(image.centreDip, 'f', 2), QString(),
                    Evidence::Bad);
        }

        if (!facts.attempts.isEmpty())
            add("attempts", i18n("Solver attempts"), facts.attempts.size(), QString(), Evidence::Info);

        if (reason == AlignFailReason::SolverTimeout)
            add("solver_timeout", i18n("Solver timeout"), facts.timeoutSec, QStringLiteral("s"), Evidence::Bad);

        if (facts.indexSolver && !facts.requiredSizes.isEmpty())
        {
            bool covered = true;
            for (int size : facts.requiredSizes)
                covered = covered && facts.installedSizes.contains(size);
            add("index_required", i18n("Index files needed"), seriesList(facts.requiredSizes), QString(),
                covered ? Evidence::Ok : Evidence::Bad);
        }
    }

    if (reason == AlignFailReason::CaptureError)
        add("capture_errors", i18n("Capture errors"), facts.captureErrors, QString(), Evidence::Bad);
    if (reason == AlignFailReason::CaptureTimeout)
        add("driver_restarts", i18n("Driver restarts"), facts.driverRestarts, QString(), Evidence::Bad);
}

void hardwareItems(const AlignFacts &facts, QList<Item> &items)
{
    // Every item here follows from the failure reason itself, so each is a likely cause
    const int first = items.size();

    switch (facts.reason)
    {
        case AlignFailReason::NoCamera:
            items << selectCameraItem();
            break;

        case AlignFailReason::CameraDisconnected:
            items << connectCameraItem(facts);
            break;

        case AlignFailReason::NoMountCoords:
        {
            if (!facts.hasMount)
            {
                auto item = makeItem("train_select_mount", Kind::Open, Tier::Hardware, i18n("Select a mount in the optical train"),
                                     i18n("Align needs a mount to know where the telescope points."));
                item.openTarget = QStringLiteral("optical_train");
                items << item;
            }
            else if (!facts.mountConnected)
                items << connectMountItem();
            else
                items << mountCoordsItem();
            break;
        }

        case AlignFailReason::MountParked:
        {
            auto item = makeItem("mount_unpark", Kind::Auto, Tier::Hardware, i18n("Unpark the mount"),
                                 i18n("The mount reports PARKED."));
            item.cost = MovesHardware;
            // A retry may slew the mount: the user starts Align again after the mount is unparked.
            item.autoRetry = false;
            item.verify = makeVerify("device_state", "park_status", "==", 0);
            items << item;
            break;
        }

        case AlignFailReason::SlewRejected:
        {
            if (facts.altLimitsEnabled && facts.targetAlt &&
                    (*facts.targetAlt < facts.altMin || *facts.targetAlt > facts.altMax))
            {
                auto item = makeItem("slew_alt_limit", Kind::Open, Tier::Hardware, i18n("Check the mount altitude limits"),
                                     i18n("The target altitude %1° is outside the limits of %2° to %3°.",
                                          QString::number(*facts.targetAlt, 'f', 1), QString::number(facts.altMin, 'f', 0),
                                          QString::number(facts.altMax, 'f', 0)));
                item.blocker = false;
                item.openTarget = QStringLiteral("mount_limits");
                items << item;
            }
            else
            {
                auto item = makeItem("slew_rejected_check", Kind::Manual, Tier::Hardware, i18n("Check the slew target"),
                                     i18n("KStars refused the slew, see the notifications for the reason."));
                item.steps << i18n("Check that the target is above the horizon.")
                           << i18n("Check the mount limits in the Mount module.");
                item.verify = makeVerify("retry");
                items << item;
            }
            break;
        }

        case AlignFailReason::SyncAlert:
        case AlignFailReason::SlewAlert:
        case AlignFailReason::SyncRejected:
        {
            auto item = makeItem("slew_rejected_by_mount", Kind::Manual, Tier::Hardware, i18n("Check the mount"),
                                 facts.reason == AlignFailReason::SlewAlert ? i18n("The mount reported an error while slewing.")
                                 : i18n("The mount reported an error while syncing."));
            item.steps << i18n("Check that the mount is unparked and tracking.")
                       << i18n("Check the slew limits set in the mount driver or its hand controller.")
                       << i18n("Check the mount cable and power.");
            item.cost = MovesHardware;
            item.verify = makeVerify("retry");
            items << item;

            if (facts.gotoMode == GOTO_SYNC && facts.hasMount && !facts.canSync)
            {
                auto info = makeItem("mount_no_sync", Kind::Info, Tier::Tuning, i18n("The mount cannot sync"),
                                     i18n("The mount driver does not support sync. Use Slew to Target or Nothing instead."));
                info.blocker = false;
                items << info;
            }
            break;
        }

        case AlignFailReason::SlewEndedIdle:
        {
            auto item = makeItem("slew_ended_idle_check", Kind::Manual, Tier::Hardware, i18n("Check why the mount stopped"),
                                 i18n("The mount stopped after the slew and did not track."));
            item.steps << i18n("Check the mount driver or hand controller for a limit error, e.g. the meridian limit.")
                       << i18n("Switch tracking on: some mounts end a slew without tracking when tracking was off before it.")
                       << i18n("Check that the slew was not stopped by hand.");
            item.cost = MovesHardware;
            item.verify = makeVerify("retry");
            items << item;
            break;
        }
        case AlignFailReason::BlobDisabled:
        {
            auto item = makeItem("camera_enable_blob", Kind::Auto, Tier::Hardware, i18n("Enable image transfer"),
                                 i18n("Image transfer is disabled for the camera %1.", facts.cameraName));
            items << item;
            break;
        }

        case AlignFailReason::DustCapError:
        {
            auto item = makeItem("remove_dust_cap", Kind::Manual, Tier::Hardware, i18n("Open the dust cap"),
                                 i18n("The dust cap reports an error."));
            item.steps << i18n("Open or remove the dust cap by hand.")
                       << i18n("Check the dust cap cable and power.");
            item.verify = makeVerify("test_frame", "stars", ">=", AlignDiagnosis::MIN_STARS);
            items << item;
            break;
        }

        case AlignFailReason::FilterWheelDisconnected:
        {
            auto item = makeItem("filter_connect", Kind::Auto, Tier::Hardware, i18n("Connect the filter wheel"),
                                 i18n("The filter wheel is disconnected."));
            item.verify = makeVerify("device_state", "connected", "==", 1);
            items << item;
            break;
        }

        case AlignFailReason::FilterChangeFailed:
        {
            auto item = makeItem("filter_test_move", Kind::Manual, Tier::Hardware, i18n("Check the filter wheel"),
                                 i18n("The filter wheel did not reach the Align filter."));
            item.steps << i18n("Change the filter from the Filter Manager and check that it arrives.")
                       << i18n("Check the filter wheel cable and power.");
            item.cost = MovesHardware;
            item.verify = makeVerify("retry");
            items << item;
            break;
        }

        case AlignFailReason::CaptureError:
        case AlignFailReason::CaptureTimeout:
        {
            auto item = makeItem("check_camera_usb_power", Kind::Manual, Tier::Hardware, i18n("Check the camera cable and power"),
                                 facts.reason == AlignFailReason::CaptureError ? i18n("The camera reported %1 exposure errors.",
                                     facts.captureErrors) : i18n("No image arrived, even after restarting the camera driver."));
            item.steps << i18n("Check the camera USB cable and use a powered hub if the camera shares one.")
                       << i18n("Check the camera power supply.")
                       << i18n("Take a test exposure from the Capture module.");
            item.verify = makeVerify("retry");
            items << item;
            break;
        }

        case AlignFailReason::RemoteSolverTimeout:
        {
            auto item = makeItem("remote_solver_check", Kind::Manual, Tier::Configuration, i18n("Check the remote solver"),
                                 i18n("The remote astrometry driver sent no solution."));
            item.steps << i18n("Check that the remote astrometry driver is running.")
                       << i18n("Check that the remote computer has index files for this field of view.")
                       << i18n("Or switch Align to the local solver.");
            item.verify = makeVerify("retry");
            items << item;
            break;
        }

        case AlignFailReason::RotatorAlert:
        case AlignFailReason::RotatorNotReached:
        case AlignFailReason::RotatorTimeout:
        case AlignFailReason::RotatorWrongDirection:
        {
            QString why;
            if (facts.reason == AlignFailReason::RotatorWrongDirection)
                why = i18n("The rotator turned away from the target position angle.");
            else if (facts.reason == AlignFailReason::RotatorTimeout)
                why = i18n("The rotator did not finish turning in time.");
            else
                why = i18n("The rotator did not reach the target position angle.");
            auto item = makeItem("rotator_check", Kind::Manual, Tier::Hardware, i18n("Check the rotator"), why);
            item.steps << i18n("Check that the rotator turns freely and its cables do not snag.")
                       << i18n("Check the reverse direction setting of the rotator driver.")
                       << i18n("Rotate and solve again.");
            item.cost = MovesHardware;
            item.verify = makeVerify("retry");
            items << item;
            break;
        }

        default:
            break;
    }

    for (int i = first; i < items.size(); i++)
        items[i].likelihood = Likelihood::Likely;
    // Unless the mount truly cannot sync, which only matters when Align syncs
    for (auto &item : items)
    {
        if (item.id == QLatin1String("mount_no_sync"))
            item.likelihood = Likelihood::Possible;
    }

    // A device that dropped out explains a failure further down the line too, e.g. a mount that disconnected
    // keeps its last position, so Align goes on and fails at the solve or the slew
    const bool needsMount = isPointingFailure(facts.reason) || facts.reason == AlignFailReason::SyncRejected
                            || facts.reason == AlignFailReason::SlewRejected || facts.reason == AlignFailReason::SlewEndedIdle;
    if (needsMount && facts.hasMount && !facts.mountConnected)
        items.prepend(connectMountItem());

    const bool needsCamera = facts.reason == AlignFailReason::CaptureError || facts.reason == AlignFailReason::CaptureTimeout;
    if (needsCamera && facts.hasCamera && !facts.cameraConnected)
        items.prepend(connectCameraItem(facts));
}

void configurationItems(const AlignFacts &facts, QList<Item> &items)
{
    const auto reason = facts.reason;
    const bool opticsWrong = facts.focalLength <= 0 || facts.aperture <= 0;
    const bool pixelWrong = facts.pixelW <= 0 || facts.pixelH <= 0;

    // A value that is missing or zero is the cause; a plausible value is only a suspect
    if (reason == AlignFailReason::MissingOptics || (reason == AlignFailReason::FovOutOfBounds && opticsWrong))
        items << opticsItem(facts, Likelihood::Likely);
    else if (reason == AlignFailReason::FovOutOfBounds && !pixelWrong)
        items << opticsItem(facts, Likelihood::Possible);

    if (reason == AlignFailReason::MissingPixelSize || (reason == AlignFailReason::FovOutOfBounds && pixelWrong))
        items << pixelSizeItem(facts, Likelihood::Likely);
    else if (reason == AlignFailReason::FovOutOfBounds && !opticsWrong)
        items << pixelSizeItem(facts, Likelihood::Possible);

    if (isSolveFailure(reason) && facts.localSolver && facts.externalSolverMissing)
        items << externalSolverItem();
}

void clockItems(const AlignFacts &facts, QList<Item> &items, Report &report)
{
    if (!isPointingFailure(facts.reason) || !facts.hasMount || !facts.mountConnected)
        return;

    clockChecks(facts, items, report);
}

void dataItems(const AlignFacts &facts, const QStringList &skip, QList<Item> &items, Report &report)
{
    const auto reason = facts.reason;
    if (!isSolveFailure(reason))
        return;

    indexItems(facts, items, report);

    const int stars = knownStars(facts);
    const bool fewStars = stars < 0 || stars < AlignDiagnosis::MIN_STARS;

    // Exposure and binning, one per retry. A frame known to hold few stars makes them likely causes.
    bool signalItemOffered = false;
    if (fewStars && canRaiseExposure(facts) && !skip.contains("align_exposure_5s"))
    {
        items << exposureItem(facts, stars);
        signalItemOffered = true;
    }

    if (fewStars && canBinMore(facts) && !skip.contains("align_bin_max"))
    {
        items << binItem(facts, stars >= 0);
        signalItemOffered = true;
    }

    // Solver settings
    const bool timedOut = reason == AlignFailReason::SolverTimeout ||
                          (!facts.attempts.isEmpty() && facts.attempts.last().timedOut);
    if (timedOut && facts.timeoutSec > 0 && facts.timeoutSec < AlignDiagnosis::MAX_SOLVER_TIMEOUT)
    {
        const double newTimeout = std::min(facts.timeoutSec * 2, AlignDiagnosis::MAX_SOLVER_TIMEOUT);
        auto item = makeItem("solver_timeout_raise", Kind::Auto, Tier::Data,
                             i18n("Raise the solver timeout to %1 seconds", QString::number(newTimeout, 'f', 0)),
                             i18n("The solver stopped after %1 seconds.", QString::number(facts.timeoutSec, 'f', 0)));
        item.likelihood = reason == AlignFailReason::SolverTimeout ? Likelihood::Likely : Likelihood::Possible;
        item.cost = ChangesSettings;
        item.verify = makeVerify("retry");
        items << item;
    }

    // Missing index files are not a profile problem
    if (reason != AlignFailReason::NoIndexFiles && customProfile(facts))
        items << profileItem(facts);

    // The failed frame: if the image itself is the problem, say what is wrong with it
    if (facts.image && facts.image->verdict != ImageVerdict::Unknown)
    {
        const auto &image = *facts.image;
        switch (image.verdict)
        {
            case ImageVerdict::Good:
                report.ruledOut << i18n("The image is good: %1 round stars with a median HFR of %2 pixels.", image.stars,
                                        QString::number(image.hfr, 'f', 1));
                break;

            case ImageVerdict::FewStars:
                if (!signalItemOffered)
                {
                    auto item = makeItem("check_sky_and_cap", Kind::Manual, Tier::Data, i18n("Check the sky and the dust cap"),
                                         i18n("Only %1 stars were found in the failed frame.", image.stars));
                    item.likelihood = Likelihood::Likely;
                    item.steps << i18n("Check that the dust cap or cover is open.")
                               << i18n("Check for clouds, dew on the optics, or an obstruction in front of the telescope.")
                               << i18n("Take a test exposure and look for stars.");
                    item.verify = makeVerify("test_frame", "stars", ">=", AlignDiagnosis::MIN_STARS);
                    items << item;
                }
                break;

            case ImageVerdict::OutOfFocus:
            {
                // Defocused stars are discs, or donuts behind a central obstruction: no bright core
                const QString text = image.centreDip < ImageAnalysis::MAX_FOCUSED_CENTRE_DIP ?
                                     i18n("The stars are rings, darker in the middle, as stars are when the telescope is out of focus. "
                     "Their median HFR is %1 pixels.", QString::number(image.hfr, 'f', 1)) :
                                     i18n("The stars are flat discs without a bright core, as stars are when the telescope is out of focus. "
                     "Their median HFR is %1 pixels.", QString::number(image.hfr, 'f', 1));
                auto item = makeItem("check_focus", Kind::Manual, Tier::Data, i18n("Focus the telescope"), text);
                item.likelihood = Likelihood::Likely;
                item.steps << i18n("Run autofocus in the Focus module, or focus by hand.");
                item.verify = makeVerify("test_frame", "concentration", ">=", ImageAnalysis::MIN_FOCUSED_CONCENTRATION);
                items << item;
                break;
            }

            case ImageVerdict::Trailed:
            {
                // Without tracking, stars drift at the sidereal rate. Shorter trails come from the mount or the wind.
                double drift = 0;
                if (facts.fovW > 0 && facts.width > 0 && facts.declination)
                {
                    const double pixelScale = facts.fovW * 60.0 * (facts.binIndex + 1) / facts.width;
                    drift = ImageAnalysis::siderealDrift(facts.exposure, *facts.declination, pixelScale);
                }
                const double ratio = drift > 0 ? image.trailLength / drift : 0;
                if (ratio >= AlignDiagnosis::MIN_UNTRACKED_RATIO && ratio <= AlignDiagnosis::MAX_UNTRACKED_RATIO)
                {
                    auto item = makeItem("mount_tracking_on", Kind::Auto, Tier::Hardware, i18n("Turn on mount tracking"),
                                         i18n("The stars trailed %1 pixels, as far as they drift in %2 seconds when the mount does not track.",
                                              QString::number(image.trailLength, 'f', 0), facts.exposure));
                    item.likelihood = Likelihood::Likely;
                    item.cost = MovesHardware;
                    item.autoRetry = false;
                    item.verify = makeVerify("device_state", "tracking", "==", 1);
                    items << item;
                }
                else
                {
                    auto item = makeItem("check_tracking", Kind::Manual, Tier::Data, i18n("Check the mount tracking"),
                                         i18n("All stars trailed %1 pixels in the same direction.",
                                              QString::number(image.trailLength, 'f', 0)));
                    item.likelihood = Likelihood::Likely;
                    item.steps << i18n("Check that the mount is tracking at the sidereal rate.")
                               << i18n("Check the balance, and that nothing touches the telescope during the exposure, e.g. wind or cables.")
                               << i18n("Use a shorter exposure.");
                    item.verify = makeVerify("test_frame", "elongation", "<", ImageAnalysis::MIN_TRAIL_ELONGATION);
                    items << item;
                }
                break;
            }

            case ImageVerdict::DoubleImages:
            {
                auto item = makeItem("check_mount_jump", Kind::Manual, Tier::Data, i18n("Check the mount for a sudden jump"),
                                     i18n("Every star appears twice, %1 pixels apart: the mount jumped during the exposure.",
                                          QString::number(image.doubleOffset, 'f', 0)));
                item.likelihood = Likelihood::Likely;
                item.steps << i18n("Check the balance of both axes, and that the clutches are tight.")
                           << i18n("Check that no cable snags or pulls on the telescope.")
                           << i18n("Check the gear backlash of the mount.");
                item.verify = makeVerify("test_frame", "double_fraction", "<", ImageAnalysis::MIN_DOUBLE_FRACTION);
                items << item;
                break;
            }

            case ImageVerdict::Unknown:
                break;
        }
    }
    else if (facts.localSolver)
        report.notChecked << i18n("Image: the star shapes in the failed frame were not measured.");

    // Field of view and reducer warnings
    if (narrowField(facts))
        items << narrowFovItem(facts);

    if (unusualReducer(facts))
        items << reducerItem(facts);
}

void addRuledOut(const AlignFacts &facts, Report &report)
{
    if (passedSetupChecks(facts.reason))
        report.ruledOut.prepend(i18n("Camera, optics and pixel size are set."));

    if (isSolveFailure(facts.reason) && facts.reason != AlignFailReason::NoIndexFiles)
        report.ruledOut << i18n("The image reached KStars.");

    bool blindScale = false, blindPosition = false;
    for (const auto &attempt : facts.attempts)
    {
        blindScale = blindScale || attempt.blindPass == "scale" || attempt.blindPass == "scale_position";
        blindPosition = blindPosition || attempt.blindPass == "position" || attempt.blindPass == "scale_position";
    }
    if (facts.reason == AlignFailReason::SolverFailed && facts.useScale && facts.usePosition && blindScale && blindPosition)
        report.ruledOut <<
                        i18n("The solver also ran without the scale and position hints, so a wrong focal length or a wrong mount position alone is not the cause.");
}

}

QString alignFailReasonKey(AlignFailReason reason)
{
    switch (reason)
    {
        case AlignFailReason::None:
            return QStringLiteral("none");
        case AlignFailReason::NoMountCoords:
            return QStringLiteral("no_mount_coords");
        case AlignFailReason::NoCamera:
            return QStringLiteral("no_camera");
        case AlignFailReason::CameraDisconnected:
            return QStringLiteral("camera_disconnected");
        case AlignFailReason::MissingOptics:
            return QStringLiteral("missing_optics");
        case AlignFailReason::MissingPixelSize:
            return QStringLiteral("missing_pixel_size");
        case AlignFailReason::FovOutOfBounds:
            return QStringLiteral("fov_out_of_bounds");
        case AlignFailReason::FilterWheelDisconnected:
            return QStringLiteral("filter_wheel_disconnected");
        case AlignFailReason::FilterChangeFailed:
            return QStringLiteral("filter_change_failed");
        case AlignFailReason::BlobDisabled:
            return QStringLiteral("blob_disabled");
        case AlignFailReason::DustCapError:
            return QStringLiteral("dust_cap_error");
        case AlignFailReason::CaptureError:
            return QStringLiteral("capture_error");
        case AlignFailReason::CaptureTimeout:
            return QStringLiteral("capture_timeout");
        case AlignFailReason::BusyTimeout:
            return QStringLiteral("busy_timeout");
        case AlignFailReason::NoIndexFiles:
            return QStringLiteral("no_index_files");
        case AlignFailReason::SolverFailed:
            return QStringLiteral("solver_failed");
        case AlignFailReason::SolverTimeout:
            return QStringLiteral("solver_timeout");
        case AlignFailReason::PahSolverFailed:
            return QStringLiteral("pah_solver_failed");
        case AlignFailReason::RemoteSolverTimeout:
            return QStringLiteral("remote_solver_timeout");
        case AlignFailReason::SyncRejected:
            return QStringLiteral("sync_rejected");
        case AlignFailReason::SyncAlert:
            return QStringLiteral("sync_alert");
        case AlignFailReason::SlewRejected:
            return QStringLiteral("slew_rejected");
        case AlignFailReason::SlewAlert:
            return QStringLiteral("slew_alert");
        case AlignFailReason::MountParked:
            return QStringLiteral("mount_parked");
        case AlignFailReason::RotatorAlert:
            return QStringLiteral("rotator_alert");
        case AlignFailReason::RotatorNotReached:
            return QStringLiteral("rotator_not_reached");
        case AlignFailReason::RotatorTimeout:
            return QStringLiteral("rotator_timeout");
        case AlignFailReason::RotatorWrongDirection:
            return QStringLiteral("rotator_wrong_direction");
        case AlignFailReason::AccuracyNotMet:
            return QStringLiteral("accuracy_not_met");
        case AlignFailReason::UserAbort:
            return QStringLiteral("user_abort");
        case AlignFailReason::SlewEndedIdle:
            return QStringLiteral("slew_ended_idle");
    }
    return QStringLiteral("none");
}

QString AlignDiagnosis::defaultProfileName()
{
    return QStringLiteral("1-Default");
}

bool AlignDiagnosis::isDiagnosable(AlignFailReason reason)
{
    return reason != AlignFailReason::None && reason != AlignFailReason::UserAbort;
}

Report AlignDiagnosis::diagnose(const AlignFacts &facts, const QStringList &tried, const QStringList &dismissed)
{
    Report report;
    report.module = QStringLiteral("align");
    report.train = facts.train;
    report.state = facts.state;
    report.reason = static_cast<int>(facts.reason);
    report.reasonKey = alignFailReasonKey(facts.reason);
    report.tried = tried;

    describeCause(facts, report);
    addEvidence(facts, report);
    addRuledOut(facts, report);

    const QStringList skip = tried + dismissed;

    QList<Item> items;
    hardwareItems(facts, items);
    configurationItems(facts, items);
    clockItems(facts, items, report);
    dataItems(facts, skip, items, report);

    // Never offer the same item twice, nor one already tried or dismissed
    QStringList seen;
    for (const auto &item : items)
    {
        if (seen.contains(item.id) || skip.contains(item.id))
            continue;
        seen << item.id;
        report.items << item;
    }

    // A hint alone (a narrow field) is not a cause: list the general checks with it
    const bool onlyHints = std::all_of(report.items.cbegin(), report.items.cend(), [](const Item & item)
    {
        return item.id == QLatin1String("fov_narrow");
    });
    if (onlyHints)
    {
        auto item = makeItem("align_check_log", Kind::Info, Tier::Tuning, i18n("Check the Align log"),
                             i18n("No specific cause was found."));
        item.likelihood = Likelihood::Check;
        item.blocker = false;
        item.steps << i18n("Check the focal length, reducer and pixel size: a wrong field of view stops the solver.")
                   << i18n("Check the time, time zone and location of KStars and the mount: a wrong position hint stops the solver.")
                   << i18n("Check that the index files for this field of view are installed.")
                   << i18n("Enable Alignment Logging in Setup Tab -> Logs to get details on the failure.");
        report.items << item;
    }

    order(report.items);
    return report;
}

Report AlignDiagnosis::checkup(const AlignFacts &facts, const QStringList &dismissed)
{
    Report report;
    report.module = QStringLiteral("align");
    report.train = facts.train;
    report.state = facts.state;
    report.reason = static_cast<int>(AlignFailReason::None);
    report.reasonKey = checkupReasonKey();

    QList<Item> items;

    // Devices
    if (!facts.hasCamera)
        items << selectCameraItem();
    else if (!facts.cameraConnected)
        items << connectCameraItem(facts);
    else
        report.ruledOut << i18n("The camera %1 is connected.", facts.cameraName);

    // Align can solve without a mount, so only a mount that is there is checked
    if (facts.hasMount)
    {
        if (!facts.mountConnected)
            items << connectMountItem();
        else if (!facts.mountCoordsValid)
            items << mountCoordsItem();
        else
            report.ruledOut << i18n("The mount is connected and reports its position.");
    }

    // Optics and pixel size give the field of view the solver searches
    const bool opticsMissing = facts.focalLength <= 0 || facts.aperture <= 0;
    const bool pixelMissing = facts.pixelW <= 0 || facts.pixelH <= 0;
    if (opticsMissing)
        items << opticsItem(facts, Likelihood::Likely);
    // The driver reports the pixel size once the camera is connected
    if (pixelMissing && facts.hasCamera && facts.cameraConnected)
        items << pixelSizeItem(facts, Likelihood::Likely);
    if (!opticsMissing && !pixelMissing)
        report.ruledOut << i18n("Focal length, aperture and pixel size are set.");

    if (facts.localSolver && facts.externalSolverMissing)
        items << externalSolverItem();
    indexItems(facts, items, report);

    if (facts.hasMount && facts.mountConnected)
        clockChecks(facts, items, report);

    // Settings that make solving hard, unless Align just solved with them
    bool solved = false;
    if (!facts.attempts.isEmpty())
    {
        const auto &last = facts.attempts.last();
        solved = last.success && std::abs(last.exposure - facts.exposure) < 1e-3 && last.binning == facts.binIndex + 1
                 && last.profile == facts.profile;
    }
    if (solved)
        report.ruledOut << i18n("Align solved with the current exposure, binning and solver profile.");
    else
    {
        if (canRaiseExposure(facts))
            items << exposureItem(facts, -1);
        if (canBinMore(facts))
            items << binItem(facts, false);
        if (customProfile(facts))
            items << profileItem(facts);
    }
    if (unusualReducer(facts))
        items << reducerItem(facts);
    if (narrowField(facts))
        items << narrowFovItem(facts);

    QStringList seen;
    for (auto item : items)
    {
        if (seen.contains(item.id) || dismissed.contains(item.id))
            continue;
        seen << item.id;
        // Nothing failed: applying a suggestion never starts Align by itself
        item.autoRetry = false;
        report.items << item;
    }
    order(report.items);

    // What the check-up looked at
    auto add = [&report](const QString & key, const QString & label, const QVariant & value, const QString & unit)
    {
        Evidence evidence;
        evidence.key = key;
        evidence.label = label;
        evidence.value = value;
        evidence.unit = unit;
        report.evidence << evidence;
    };
    if (facts.hasCamera)
        add("camera", i18n("Camera"), facts.cameraName, QString());
    if (facts.fovW > 0 && facts.fovH > 0)
        add("fov", i18n("Field of view"), QString("%1 x %2").arg(facts.fovW, 0, 'f', 1).arg(facts.fovH, 0, 'f', 1),
            QStringLiteral("arcmin"));
    add("exposure", i18n("Exposure"), facts.exposure, QStringLiteral("s"));
    add("binning", i18n("Binning"), QString("%1x%1").arg(facts.binIndex + 1), QString());

    if (report.items.isEmpty())
    {
        report.title = i18n("Align looks ready");
        report.summary = i18n("No problems were found in the Align setup.");
    }
    else
    {
        report.title = i18np("%1 thing to check", "%1 things to check", report.items.size());
        report.summary = i18n("Start with the first suggestion, it is the most likely to matter.");
    }

    return report;
}

}
