#include "calibrationprocess.h"

#include "gmath.h"
#include "ekos_guide_debug.h"
#include "gmath.h"
#include "guidelog.h"

#include "Options.h"

namespace Ekos
{

QString stageString(Ekos::CalibrationProcess::CalibrationStage stage)
{
    switch(stage)
    {
        case Ekos::CalibrationProcess::CAL_IDLE:
            return ("CAL_IDLE");
        case Ekos::CalibrationProcess::CAL_ERROR:
            return("CAL_ERROR");
        case Ekos::CalibrationProcess::CAL_CAPTURE_IMAGE:
            return("CAL_CAPTURE_IMAGE");
        case Ekos::CalibrationProcess::CAL_SELECT_STAR:
            return("CAL_SELECT_STAR");
        case Ekos::CalibrationProcess::CAL_START:
            return("CAL_START");
        case Ekos::CalibrationProcess::CAL_RA_INC:
            return("CAL_RA_INC");
        case Ekos::CalibrationProcess::CAL_RA_DEC:
            return("CAL_RA_DEC");
        case Ekos::CalibrationProcess::CAL_DEC_INC:
            return("CAL_DEC_INC");
        case Ekos::CalibrationProcess::CAL_DEC_DEC:
            return("CAL_DEC_DEC");
        case Ekos::CalibrationProcess::CAL_BACKLASH:
            return("CAL_BACKLASH");
        default:
            return("???");
    };
}

CalibrationProcess::CalibrationProcess(double startX, double startY, bool raOnlyEnabled)
{
    calibrationStage = CAL_START;
    raOnly = raOnlyEnabled;
    start_x1 = startX;
    start_y1 = startY;
}

void CalibrationProcess::useCalibration(Calibration *calibrationPtr)
{
    calibration = calibrationPtr;
    tempCalibration = *calibration;
}

void CalibrationProcess::startup()
{
    calibrationStage = CAL_START;
}

void CalibrationProcess::setGuideLog(GuideLog *guideLogPtr)
{
    guideLog = guideLogPtr;
}

bool CalibrationProcess::inProgress() const
{
    return calibrationStage > CAL_START;
}

void CalibrationProcess::addCalibrationUpdate(
    GuideInterface::CalibrationUpdateType type,
    QString message, double x, double y)
{
    updateType = type;
    calibrationUpdate = message;
    updateX = x;
    updateY = y;
}

void CalibrationProcess::getCalibrationUpdate(
    GuideInterface::CalibrationUpdateType *type,
    QString *message, double *x, double *y) const
{
    *type = updateType;
    *message = calibrationUpdate;
    *x = updateX;
    *y = updateY;
}

QString CalibrationProcess::getLogStatus() const
{
    return logString;
}

void CalibrationProcess::addLogStatus(const QString &message)
{
    logString = message;
}

void CalibrationProcess::addPulse(GuideDirection dir, int msecs)
{
    pulseDirection = dir;
    pulseMsecs = msecs;
}

void CalibrationProcess::getPulse(GuideDirection *dir, int *msecs) const
{
    *dir = pulseDirection;
    *msecs = pulseMsecs;
}

void CalibrationProcess::addStatus(Ekos::GuideState s)
{
    status = s;
}

Ekos::GuideState CalibrationProcess::getStatus() const
{
    return status;
}

void CalibrationProcess::initializeIteration()
{
    axisCalibrationComplete = false;

    logString.clear();

    calibrationUpdate.clear();
    updateType = GuideInterface::CALIBRATION_MESSAGE_ONLY;
    updateX = 0;
    updateY = 0;

    addStatus(Ekos::GUIDE_CALIBRATING);

    pulseDirection = NO_DIR;
    pulseMsecs = 0;
}

void CalibrationProcess::iterate(double x, double y)
{
    initializeIteration();
    switch (calibrationStage)
    {
        case CAL_START:
            startState();
            break;
        case CAL_RA_INC:
            raOutState(x, y);
            break;
        case CAL_RA_DEC:
            raInState(x, y);
            break;
        case CAL_BACKLASH:
            decBacklashState(x, y);
            break;
        case CAL_DEC_INC:
            decOutState(x, y);
            break;
        case CAL_DEC_DEC:
            decInState(x, y);
            break;
        default:
            break;
    }
}

void CalibrationProcess::startState()
{
    maximumSteps = Options::autoModeIterations();
    turn_back_time = maximumSteps * 7;

    ra_iterations = 0;
    dec_iterations = 0;
    backlash_iterations = 0;
    ra_total_pulse = de_total_pulse = 0;

    addLogStatus(i18n("RA drifting forward..."));

    last_pulse = Options::calibrationPulseDuration();

    addCalibrationUpdate(GuideInterface::RA_OUT, i18n("Guide Star found."), 0, 0);

    qCDebug(KSTARS_EKOS_GUIDE) << "Auto Iteration #" << maximumSteps << "Default pulse:" << last_pulse;
    qCDebug(KSTARS_EKOS_GUIDE) << "Start X1 " << start_x1 << " Start Y1 " << start_y1;

    last_x = start_x1;
    last_y = start_x2;

    addPulse(RA_INC_DIR, last_pulse);

    ra_iterations++;

    calibrationStage = CAL_RA_INC;
    if (guideLog)
        guideLog->addCalibrationData(RA_INC_DIR, start_x1, start_y1, start_x1, start_y1);
}


void CalibrationProcess::raOutState(double cur_x, double cur_y)
{
    QString Info = QString("RA+ (%1, %2)").arg((cur_x - start_x1), 0, 'f', 1).arg((cur_y - start_y1), 0, 'f', 1);
    addCalibrationUpdate(GuideInterface::RA_OUT, Info, cur_x - start_x1, cur_y - start_y1);

    qCDebug(KSTARS_EKOS_GUIDE) << "Iteration #" << ra_iterations << ": STAR " << cur_x << "," << cur_y;
    qCDebug(KSTARS_EKOS_GUIDE) << "Iteration " << ra_iterations << " Direction: RA_INC_DIR" << " Duration: "
                               << last_pulse << " ms.";

    if (guideLog)
        guideLog->addCalibrationData(RA_INC_DIR, cur_x, cur_y, start_x1, start_y1);

    // Must pass at least 1.5 pixels to move on to the next stage.
    // If we've moved 15 pixels, we can cut short the requested number of iterations.
    const double xDrift = cur_x - start_x1;
    const double yDrift = cur_y - start_y1;
    if (((ra_iterations >= maximumSteps) ||
            (std::hypot(xDrift, yDrift) > Options::calibrationMaxMove()))
            && (fabs(xDrift) > 1.5 || fabs(yDrift) > 1.5))
    {
        ra_total_pulse += last_pulse;
        calibrationStage = CAL_RA_DEC;

        end_x1 = cur_x;
        end_y1 = cur_y;

        last_x = cur_x;
        last_y = cur_y;

        qCDebug(KSTARS_EKOS_GUIDE) << "End X1 " << end_x1 << " End Y1 " << end_y1;

        // This temporary calibration is just used to help find our way back to the origin.
        // total_pulse is not used, but valid.
        tempCalibration.calculate1D(start_x1, start_y1, end_x1, end_y1, ra_total_pulse);

        ra_distance = 0;
        backlash = 0;

        addCalibrationUpdate(GuideInterface::RA_OUT_OK, Info, cur_x - start_x1, cur_y - start_y1);

        addPulse(RA_DEC_DIR, last_pulse);
        ra_iterations++;

        addLogStatus(i18n("RA drifting reverse..."));
        if (guideLog)
            guideLog->endCalibrationSection(RA_INC_DIR, tempCalibration.getAngle());
    }
    else if (ra_iterations > turn_back_time)
    {
        addLogStatus(i18n("Calibration rejected. Star drift is too short. Check for mount, cable, or backlash problems."));
        calibrationStage = CAL_ERROR;
        addStatus(Ekos::GUIDE_CALIBRATION_ERROR);
        addCalibrationUpdate(GuideInterface::CALIBRATION_MESSAGE_ONLY, i18n("Calibration Failed: Drift too short."));
        if (guideLog)
            guideLog->endCalibration(0, 0);
    }
    else
    {
        // Aggressive pulse in case we're going slow
        if (fabs(cur_x - last_x) < 0.5 && fabs(cur_y - last_y) < 0.5)
        {
            // 200%
            last_pulse = Options::calibrationPulseDuration() * 2;
        }
        else
        {
            ra_total_pulse += last_pulse;
            last_pulse = Options::calibrationPulseDuration();
        }

        last_x = cur_x;
        last_y = cur_y;

        addPulse(RA_INC_DIR, last_pulse);

        ra_iterations++;
    }
}

void CalibrationProcess::raInState(double cur_x, double cur_y)
{
    QString Info = QString("RA-  (%1, %2)").arg((cur_x - start_x1), 0, 'f', 1).arg((cur_y - start_y1), 0, 'f', 1);
    addCalibrationUpdate(GuideInterface::RA_IN, Info, cur_x - start_x1, cur_y - start_y1);

    qCDebug(KSTARS_EKOS_GUIDE) << "Iteration #" << ra_iterations << ": STAR " << cur_x << "," << cur_y;
    qCDebug(KSTARS_EKOS_GUIDE) << "Iteration " << ra_iterations << " Direction: RA_DEC_DIR" << " Duration: "
                               << last_pulse << " ms.";

    double driftRA, driftDEC;
    tempCalibration.computeDrift(GuiderUtils::Vector(cur_x, cur_y, 0), GuiderUtils::Vector(start_x1, start_y1, 0),
                                 &driftRA, &driftDEC);

    qCDebug(KSTARS_EKOS_GUIDE) << "Star x pos is " << driftRA << " from original point.";

    if (ra_distance == 0.0)
        ra_distance = driftRA;

    if (guideLog)
        guideLog->addCalibrationData(RA_DEC_DIR, cur_x, cur_y, start_x1, start_y1);

    // start point reached... so exit
    if (driftRA < 1.5)
    {
        last_pulse = Options::calibrationPulseDuration();
        axisCalibrationComplete = true;
    }
    // If we'not moving much, try increasing pulse to 200% to clear any backlash
    // Also increase pulse width if we are going FARTHER and not back to our original position
    else if ( (fabs(cur_x - last_x) < 0.5 && fabs(cur_y - last_y) < 0.5)
              || driftRA > ra_distance)
    {
        backlash++;

        // Increase pulse to 200% after we tried to fight against backlash 2 times at least
        if (backlash > 2)
            last_pulse = Options::calibrationPulseDuration() * 2;
        else
            last_pulse = Options::calibrationPulseDuration();
    }
    else
    {
        //ra_total_pulse += last_pulse;
        last_pulse = Options::calibrationPulseDuration();
        backlash = 0;
    }
    last_x = cur_x;
    last_y = cur_y;

    if (axisCalibrationComplete == false)
    {
        if (ra_iterations < turn_back_time)
        {
            addPulse(RA_DEC_DIR, last_pulse);
            ra_iterations++;
            return;
        }

        calibrationStage = CAL_ERROR;
        addStatus(Ekos::GUIDE_CALIBRATION_ERROR);
        addCalibrationUpdate(GuideInterface::CALIBRATION_MESSAGE_ONLY, i18n("Calibration Failed: couldn't reach start."));
        addLogStatus(i18np("Guide RA: Scope cannot reach the start point after %1 iteration. Possible mount or "
                           "backlash problems...",
                           "GUIDE_RA: Scope cannot reach the start point after %1 iterations. Possible mount or "
                           "backlash problems...",
                           ra_iterations));
        return;
    }

    if (raOnly == false)
    {
        if (Options::guideCalibrationBacklash())
        {
            calibrationStage = CAL_BACKLASH;
            last_x = cur_x;
            last_y = cur_y;
            start_backlash_x = cur_x;
            start_backlash_y = cur_y;
            addPulse(DEC_INC_DIR, Options::calibrationPulseDuration());
            backlash_iterations++;
            addLogStatus(i18n("DEC backlash..."));
        }
        else
        {
            calibrationStage = CAL_DEC_INC;
            start_x2         = cur_x;
            start_y2         = cur_y;
            last_x = cur_x;
            last_y = cur_y;

            qCDebug(KSTARS_EKOS_GUIDE) << "Start X2 " << start_x2 << " start Y2 " << start_y2;
            addPulse(DEC_INC_DIR, Options::calibrationPulseDuration());
            dec_iterations++;
            addLogStatus(i18n("DEC drifting forward..."));
        }
        return;
    }
    // calc orientation
    if (calibration->calculate1D(start_x1, start_y1, end_x1, end_y1, ra_total_pulse))
    {
        calibration->save();
        calibrationStage = CAL_IDLE;
        addStatus(Ekos::GUIDE_CALIBRATION_SUCCESS);
        // Below converts from ms/arcsecond to arcseconds/second.
        if (guideLog)
            guideLog->endCalibration(1000.0 / calibration->raPulseMillisecondsPerArcsecond(), 0);
    }
    else
    {
        addLogStatus(i18n("Calibration rejected. Star drift is too short. Check for mount, cable, or backlash problems."));
        calibrationStage = CAL_ERROR;
        addStatus(Ekos::GUIDE_CALIBRATION_ERROR);
        addCalibrationUpdate(GuideInterface::CALIBRATION_MESSAGE_ONLY, i18n("Calibration Failed: drift too short."));
        if (guideLog)
            guideLog->endCalibration(0, 0);
    }
}

void CalibrationProcess::decBacklashState(double cur_x, double cur_y)
{
    double driftRA, driftDEC;
    tempCalibration.computeDrift(
        GuiderUtils::Vector(cur_x, cur_y, 0),
        GuiderUtils::Vector(start_backlash_x, start_backlash_y, 0),
        &driftRA, &driftDEC);

    // Exit the backlash phase either after 5 pulses, or after we've moved sufficiently in the
    // DEC direction.
    constexpr int MIN_DEC_BACKLASH_MOVE_PIXELS = 3;
    if ((++backlash_iterations >= 5) ||
            (fabs(driftDEC) > MIN_DEC_BACKLASH_MOVE_PIXELS))
    {
        addCalibrationUpdate(GuideInterface::BACKLASH, i18n("Calibrating DEC Backlash"),
                             cur_x - start_x1, cur_y - start_y1);
        qCDebug(KSTARS_EKOS_GUIDE) << QString("Stopping dec backlash caibration after %1 iterations, offset %2")
                                   .arg(backlash_iterations - 1)
                                   .arg(driftDEC, 4, 'f', 2);
        calibrationStage = CAL_DEC_INC;
        start_x2         = cur_x;
        start_y2         = cur_y;
        last_x = cur_x;
        last_y = cur_y;

        qCDebug(KSTARS_EKOS_GUIDE) << "Start X2 " << start_x2 << " start Y2 " << start_y2;
        addPulse(DEC_INC_DIR, Options::calibrationPulseDuration());
        dec_iterations++;
        addLogStatus(i18n("DEC drifting forward..."));
        return;
    }
    addCalibrationUpdate(GuideInterface::BACKLASH, i18n("Calibrating DEC Backlash"),
                         cur_x - start_x1, cur_y - start_y1);
    qCDebug(KSTARS_EKOS_GUIDE) << "Backlash iter" << backlash_iterations << "position" << cur_x << cur_y;
    addPulse(DEC_INC_DIR, Options::calibrationPulseDuration());
}


void CalibrationProcess::decOutState(double cur_x, double cur_y)
{
    QString Info = QString("DE+  (%1, %2)").arg((cur_x - start_x1), 0, 'f', 1).arg((cur_y - start_y1), 0, 'f', 1);
    addCalibrationUpdate(GuideInterface::DEC_OUT, Info, cur_x - start_x1, cur_y - start_y1);

    qCDebug(KSTARS_EKOS_GUIDE) << "Iteration #" << dec_iterations << ": STAR " << cur_x << "," << cur_y;
    qCDebug(KSTARS_EKOS_GUIDE) << "Iteration " << dec_iterations << " Direction: DEC_INC_DIR" <<
                               " Duration: " << last_pulse << " ms.";

    // Don't yet know how to tell NORTH vs SOUTH
    if (guideLog)
        guideLog->addCalibrationData(DEC_INC_DIR, cur_x, cur_y,
                                     start_x2, start_y2);
    const double xDrift = cur_x - start_x2;
    const double yDrift = cur_y - start_y2;
    if (((dec_iterations >= maximumSteps) ||
            (std::hypot(xDrift, yDrift) > Options::calibrationMaxMove()))
            && (fabs(xDrift) > 1.5 || fabs(yDrift) > 1.5))
    {
        calibrationStage = CAL_DEC_DEC;

        de_total_pulse += last_pulse;

        end_x2 = cur_x;
        end_y2 = cur_y;

        last_x = cur_x;
        last_y = cur_y;

        axisCalibrationComplete = false;

        qCDebug(KSTARS_EKOS_GUIDE) << "End X2 " << end_x2 << " End Y2 " << end_y2;

        tempCalibration.calculate1D(start_x2, start_y2, end_x2, end_y2, de_total_pulse);

        de_distance = 0;

        addCalibrationUpdate(GuideInterface::DEC_OUT_OK, Info, cur_x - start_x1, cur_y - start_y1);

        addPulse(DEC_DEC_DIR, last_pulse);
        addLogStatus(i18n("DEC drifting reverse..."));
        dec_iterations++;
        if (guideLog)
            guideLog->endCalibrationSection(DEC_INC_DIR, tempCalibration.getAngle());
    }
    else if (dec_iterations > turn_back_time)
    {
        calibrationStage = CAL_ERROR;

        addStatus(Ekos::GUIDE_CALIBRATION_ERROR);
        addCalibrationUpdate(GuideInterface::CALIBRATION_MESSAGE_ONLY, i18n("Calibration Failed: couldn't reach start point."));
        addLogStatus(i18np("Guide DEC: Scope cannot reach the start point after %1 iteration.\nPossible mount "
                           "or backlash problems...",
                           "GUIDE DEC: Scope cannot reach the start point after %1 iterations.\nPossible mount "
                           "or backlash problems...",
                           dec_iterations));

        if (guideLog)
            guideLog->endCalibration(0, 0);
    }
    else
    {
        if (fabs(cur_x - last_x) < 0.5 && fabs(cur_y - last_y) < 0.5)
        {
            // Increase pulse by 200%
            last_pulse = Options::calibrationPulseDuration() * 2;
        }
        else
        {
            de_total_pulse += last_pulse;
            last_pulse = Options::calibrationPulseDuration();
        }
        last_x = cur_x;
        last_y = cur_y;

        addPulse(DEC_INC_DIR, last_pulse);

        dec_iterations++;
    }
}

void CalibrationProcess::decInState(double cur_x, double cur_y)
{
    QString Info = QString("DE-  (%1, %2)").arg((cur_x - start_x1), 0, 'f', 1).arg((cur_y - start_y1), 0, 'f', 1);
    addCalibrationUpdate(GuideInterface::DEC_IN, Info, cur_x - start_x1, cur_y - start_y1);

    // Star position resulting from LAST guiding pulse to mount
    qCDebug(KSTARS_EKOS_GUIDE) << "Iteration #" << dec_iterations << ": STAR " << cur_x << "," << cur_y;
    qCDebug(KSTARS_EKOS_GUIDE) << "Iteration " << dec_iterations << " Direction: DEC_DEC_DIR" <<
                               " Duration: " << last_pulse << " ms.";

    // Note: the way this temp calibration was set up above, with the DEC drifts, the ra axis is really dec.
    // This will help the dec find its way home. Could convert to a full RA/DEC calibration.
    double driftRA, driftDEC;
    tempCalibration.computeDrift(
        GuiderUtils::Vector(cur_x, cur_y, 0),
        GuiderUtils::Vector(start_x1, start_y1, 0),
        &driftRA, &driftDEC);

    qCDebug(KSTARS_EKOS_GUIDE) << "Currently " << driftRA << driftDEC << " from original point.";

    // Keep track of distance
    if (de_distance == 0.0)
        de_distance = driftRA;

    if (guideLog)
        guideLog->addCalibrationData(DEC_DEC_DIR, cur_x, cur_y, start_x2, start_y2);

    // start point reached... so exit
    if (driftRA < 1.5)
    {
        last_pulse = Options::calibrationPulseDuration();
        axisCalibrationComplete = true;
    }
    // Increase pulse if we're not moving much or if we are moving _away_ from target.
    else if ( (fabs(cur_x - last_x) < 0.5 && fabs(cur_y - last_y) < 0.5)
              || driftRA > de_distance)
    {
        // Increase pulse by 200%
        last_pulse = Options::calibrationPulseDuration() * 2;
    }
    else
    {
        last_pulse = Options::calibrationPulseDuration();
    }

    if (axisCalibrationComplete == false)
    {
        if (dec_iterations < turn_back_time)
        {
            addPulse(DEC_DEC_DIR, last_pulse);
            dec_iterations++;
            return;
        }

        calibrationStage = CAL_ERROR;

        addStatus(Ekos::GUIDE_CALIBRATION_ERROR);
        addCalibrationUpdate(GuideInterface::CALIBRATION_MESSAGE_ONLY, i18n("Calibration Failed: couldn't reach start point."));

        addLogStatus(i18np("Guide DEC: Scope cannot reach the start point after %1 iteration.\nPossible mount "
                           "or backlash problems...",
                           "Guide DEC: Scope cannot reach the start point after %1 iterations.\nPossible mount "
                           "or backlash problems...",
                           dec_iterations));
        return;
    }

    bool reverse_dec_dir = false;
    // calc orientation
    if (calibration->calculate2D(start_x1, start_y1, end_x1, end_y1, start_x2, start_y2, end_x2, end_y2,
                                 &reverse_dec_dir, ra_total_pulse, de_total_pulse))
    {
        double rotation = calibration->getAngle();
        QString success = QString("Camera Rotation = %1").arg(rotation, 0, 'f', 1);
        addCalibrationUpdate(GuideInterface::CALIBRATION_MESSAGE_ONLY, i18n(success.toLatin1().data()));

        calibration->save();
        calibrationStage = CAL_IDLE;
        if (reverse_dec_dir)
            addLogStatus(i18n("DEC direction reversed (RA-DEC is now standard CCW-system)."));

        addStatus(Ekos::GUIDE_CALIBRATION_SUCCESS);

        // Below converts from ms/arcsecond to arcseconds/second.
        if (guideLog)
            guideLog->endCalibration(
                1000.0 / calibration->raPulseMillisecondsPerArcsecond(),
                1000.0 / calibration->decPulseMillisecondsPerArcsecond());
        return;
    }
    else
    {
        addLogStatus(i18n("Calibration rejected. Star drift is too short. Check for mount, cable, or backlash problems."));
        addCalibrationUpdate(GuideInterface::CALIBRATION_MESSAGE_ONLY, i18n("Calibration Failed: drift too short."));
        addStatus(Ekos::GUIDE_CALIBRATION_ERROR);
        calibrationStage = CAL_ERROR;
        if (guideLog)
            guideLog->endCalibration(0, 0);
        return;
    }
}

}  // namespace Ekos
