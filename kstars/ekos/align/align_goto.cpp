/*
    SPDX-FileCopyrightText: 2013-2021 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2018-2020 Robert Lancaster <rlancaste@gmail.com>
    SPDX-FileCopyrightText: 2019-2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "align.h"
#include "align_p.h"
#include "alignview.h"
#include "fov.h"
#include "manualrotator.h"
#include "mountmodel.h"
#include "opsastrometry.h"
#include "polaralignmentassistant.h"
#include "remoteastrometryparser.h"
#include "Options.h"
#include "indi/indirotator.h"

#include "ekos/auxiliary/rotatorutils.h"
#include "ekos/manager.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsviewer.h"
#include "indi/indidome.h"
#include "ksnotification.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymapcomposite.h"

#include <ekos_align_debug.h>

#include <QFileDialog>

namespace Ekos
{

using PAA = PolarAlignmentAssistant;

void Align::stop(Ekos::AlignState mode)
{
    m_CaptureTimer.stop();
    m_RotatorTimer.invalidate();
    if (solverModeButtonGroup->checkedId() == SOLVER_LOCAL)
    {
        if (m_Solver.get())
            m_Solver->abort();
    }
    else if (solverModeButtonGroup->checkedId() == SOLVER_REMOTE && remoteParser)
        remoteParser->stopSolver();

    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);
    loadSlewB->setEnabled(true);

    m_SolveFromFile = false;
    solverIterations = 0;
    m_CaptureErrorCounter = 0;
    m_CaptureTimeoutCounter = 0;
    m_SlewErrorCounter = 0;
    m_RemoteAlignTimer.stop();

    disconnect(m_Camera, &ISD::Camera::newImage, this, &Ekos::Align::processData);
    disconnect(m_Camera, &ISD::Camera::newExposureValue, this, &Ekos::Align::checkCameraExposureProgress);

    if (rememberUploadMode != m_Camera->getUploadMode())
        m_Camera->setUploadMode(rememberUploadMode);

    // Remember to reset fast exposure
    if (m_RememberCameraFastExposure)
    {
        m_RememberCameraFastExposure = false;
        m_Camera->setFastExposureEnabled(true);
    }

    auto targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);

    // If capture is still in progress, let's stop that.
    if (matchPAHStage(PAA::PAH_POST_REFRESH))
    {
        if (targetChip->isCapturing())
            targetChip->abortExposure();

        appendLogText(i18n("Refresh is complete."));
    }
    else
    {
        if (targetChip->isCapturing())
        {
            targetChip->abortExposure();
            appendLogText(i18n("Capture aborted."));
        }
        else
            appendLogText(i18n("Solver aborted."));
    }

    setState(mode);
    Q_EMIT newStatus(state);

    setAlignTableResult(ALIGN_RESULT_FAILED);
}

void Align::updateProperty(INDI::Property prop)
{
    if (prop.isNameMatch("EQUATORIAL_EOD_COORD") || prop.isNameMatch("EQUATORIAL_COORD"))
    {
        auto nvp = prop.getNumber();
        QString ra_dms, dec_dms;

        getFormattedCoords(m_TelescopeCoord.ra().Hours(), m_TelescopeCoord.dec().Degrees(), ra_dms, dec_dms);

        ScopeRAOut->setText(ra_dms);
        ScopeDecOut->setText(dec_dms);

        switch (nvp->s)
        {
            // Idle --> Mount not tracking or slewing
            case IPS_IDLE:
                m_wasSlewStarted = false;
                break;

            // Ok --> Mount Tracking. If m_wasSlewStarted is true
            // then it just finished slewing
            case IPS_OK:
            {
                // Update the boxes as the mount just finished slewing
                if (m_wasSlewStarted && Options::astrometryAutoUpdatePosition())
                {
                    opsAstrometry->estRA->setText(ra_dms);
                    opsAstrometry->estDec->setText(dec_dms);

                    Options::setAstrometryPositionRA(nvp->np[0].value * 15);
                    Options::setAstrometryPositionDE(nvp->np[1].value);
                }

                // If we are looking for celestial pole
                if (m_wasSlewStarted && matchPAHStage(PAA::PAH_FIND_CP))
                {
                    m_wasSlewStarted = false;
                    appendLogText(i18n("Mount completed slewing near celestial pole. Capture again to verify."));
                    setSolverAction(GOTO_NOTHING);

                    m_PolarAlignmentAssistant->setPAHStage(PAA::PAH_FIRST_CAPTURE);
                    return;
                }

                switch (state)
                {
                    case ALIGN_PROGRESS:
                        break;

                    case ALIGN_SYNCING:
                    {
                        m_wasSlewStarted = false;
                        if (m_CurrentGotoMode == GOTO_SLEW)
                        {
                            Slew();
                            return;
                        }
                        else
                        {
                            appendLogText(i18n("Mount is synced to solution coordinates."));

                            KSNotification::event(QLatin1String("AlignSuccessful"),
                                                  i18n("Astrometry alignment completed successfully"), KSNotification::Align);
                            setState(ALIGN_COMPLETE);
                            Q_EMIT newStatus(state);
                            solverIterations = 0;
                            loadSlewB->setEnabled(true);
                        }
                    }
                    break;

                    case ALIGN_SLEWING:

                        if (!didSlewStart())
                        {
                            qCDebug(KSTARS_EKOS_ALIGN) << "Mount slew planned, but not started slewing yet...";
                            break;
                        }

                        m_wasSlewStarted = false;
                        if (m_SolveFromFile)
                        {
                            m_SolveFromFile = false;
                            m_TargetPositionAngle = solverFOV->PA();
                            qCDebug(KSTARS_EKOS_ALIGN) << "Solving from file: Setting target PA to" << m_TargetPositionAngle;

                            setState(ALIGN_PROGRESS);
                            Q_EMIT newStatus(state);

                            if (alignSettlingTime->value() >= DELAY_THRESHOLD_NOTIFY)
                                appendLogText(i18n("Settling..."));
                            m_resetCaptureTimeoutCounter = true;
                            m_CaptureTimer.start(alignSettlingTime->value());
                            return;
                        }
                        else if (m_CurrentGotoMode == GOTO_SLEW || (m_MountModel && m_MountModel->isRunning()))
                        {
                            if (targetAccuracyNotMet)
                                appendLogText(i18n("Slew complete. Target accuracy is not met, running solver again..."));
                            else
                                appendLogText(i18n("Slew complete. Solving Alignment Point. . ."));

                            targetAccuracyNotMet = false;

                            setState(ALIGN_PROGRESS);
                            Q_EMIT newStatus(state);

                            if (alignSettlingTime->value() >= DELAY_THRESHOLD_NOTIFY)
                                appendLogText(i18n("Settling..."));
                            m_resetCaptureTimeoutCounter = true;
                            m_CaptureTimer.start(alignSettlingTime->value());
                        }
                        break;

                    default:
                    {
                        m_wasSlewStarted = false;
                    }
                    break;
                }
            }
            break;

            // Busy --> Mount Slewing or Moving (NSWE buttons)
            case IPS_BUSY:
            {
                m_wasSlewStarted = true;
                handleMountMotion();
            }
            break;

            // Alert --> Mount has problem moving or communicating.
            case IPS_ALERT:
            {
                m_wasSlewStarted = false;

                if (state == ALIGN_SYNCING || state == ALIGN_SLEWING)
                {
                    if (state == ALIGN_SYNCING)
                        appendLogText(i18n("Syncing failed."));
                    else
                        appendLogText(i18n("Slewing failed."));

                    if (++m_SlewErrorCounter == 3)
                    {
                        abort();
                        return;
                    }
                    else
                    {
                        if (m_CurrentGotoMode == GOTO_SLEW)
                            Slew();
                        else
                            Sync();
                    }
                }

                return;
            }
        }

        RUN_PAH(processMountRotation(m_TelescopeCoord.ra(), alignSettlingTime->value()));
    }
    else if (prop.isNameMatch("ABS_ROTATOR_ANGLE"))
    {
        auto nvp = prop.getNumber();
        double RAngle = nvp->np[0].value;
        currentRotatorPA = RotatorUtils::Instance()->calcCameraAngle(RAngle, false);

        // Check for rotator error state
        if (std::isnan(m_TargetPositionAngle) == false && state == ALIGN_ROTATING && nvp->s == IPS_ALERT)
        {
            appendLogText(i18n("Rotator error detected. Aborting alignment."));
            m_RotatorTimer.invalidate();
            setState(ALIGN_FAILED);
            Q_EMIT newStatus(state);
            solveB->setEnabled(true);
            loadSlewB->setEnabled(true);
            return;
        }

        // loadSlewTarget defined if activation through [Load & Slew] and rotator just reached position
        if (std::isnan(m_TargetPositionAngle) == false && state == ALIGN_ROTATING && nvp->s == IPS_OK)
        {
            auto diff = std::abs(RotatorUtils::Instance()->DiffPA(currentRotatorPA - m_TargetPositionAngle)) * 60;
            qCDebug(KSTARS_EKOS_ALIGN) << "Raw Rotator Angle:" << RAngle << "Current PA:" << currentRotatorPA
                                       << "Target PA:" << m_TargetPositionAngle << "Diff (arcmin):" << diff << "Offset:"
                                       << Options::pAOffset();

            if (diff <= Options::astrometryRotatorThreshold())
            {
                appendLogText(i18n("Rotator reached camera position angle."));
                RotatorGOTO = true; // Flag for SlewToTarget()
                executeGOTO();
            }
            else
            {
                // Sometimes update of "nvp->s" is a bit slow, so check state again, if it's really ok
                QTimer::singleShot(300, [ = ]
                {
                    if (nvp->s == IPS_OK)
                    {
                        appendLogText(i18n("Rotator failed to arrive at the requested position angle (Deviation %1 arcmin).", diff));
                        setState(ALIGN_FAILED);
                        Q_EMIT newStatus(state);
                        solveB->setEnabled(true);
                        loadSlewB->setEnabled(true);
                    }
                });
            }
        }
        else if (m_estimateRotatorTimeFrame)
        {
            m_RotatorTimeFrame = std::min(RotatorUtils::Instance()->calcTimeFrame(RAngle), 60);
        }
    }
    else if (prop.isNameMatch("TELESCOPE_MOTION_NS") || prop.isNameMatch("TELESCOPE_MOTION_WE"))
    {
        switch (prop.getState())
        {
            case IPS_BUSY:
                handleMountMotion();
                m_wasSlewStarted = true;
                break;
            default:
                qCDebug(KSTARS_EKOS_ALIGN) << "Mount motion finished.";
                handleMountStatus();
                break;
        }
    }
}

void Align::handleMountMotion()
{
    if (state == ALIGN_PROGRESS)
    {
        if (matchPAHStage(PAA::PAH_IDLE))
        {
            // whoops, mount slews during alignment
            appendLogText(i18n("Slew detected, suspend solving..."));
            suspend();
        }

        setState(ALIGN_SLEWING);
    }
}

void Align::handleMountStatus()
{
    auto nvp = m_Mount->getNumber(m_Mount->isJ2000() ? "EQUATORIAL_COORD" :
                                  "EQUATORIAL_EOD_COORD");

    if (nvp)
        updateProperty(nvp);
}

void Align::executeGOTO()
{
    if (m_SolveFromFile)
    {
        m_DestinationCoord = m_AlignCoord;
        m_TargetCoord = m_AlignCoord;

        qCDebug(KSTARS_EKOS_ALIGN) << "Solving from file. Setting Target Coordinates align coords. RA:"
                                   << m_TargetCoord.ra().toHMSString()
                                   << "DE:" << m_TargetCoord.dec().toDMSString();

        SlewToTarget();
    }
    else if (m_CurrentGotoMode == GOTO_SYNC)
        Sync();
    else if (m_CurrentGotoMode == GOTO_SLEW)
        SlewToTarget();
}

void Align::Sync()
{
    setState(ALIGN_SYNCING);

    if (m_Mount->Sync(&m_AlignCoord))
    {
        Q_EMIT newStatus(state);
        appendLogText(
            i18n("Syncing to RA (%1) DEC (%2)", m_AlignCoord.ra().toHMSString(), m_AlignCoord.dec().toDMSString()));
    }
    else
    {
        setState(ALIGN_IDLE);
        Q_EMIT newStatus(state);
        appendLogText(i18n("Syncing failed."));
    }
}

void Align::Slew()
{
    setState(ALIGN_SLEWING);
    Q_EMIT newStatus(state);

    if (m_Mount->Slew(&m_TargetCoord))
    {
        slewStartTimer.start();
        appendLogText(i18n("Slewing to target coordinates: RA (%1) DEC (%2).", m_TargetCoord.ra().toHMSString(),
                           m_TargetCoord.dec().toDMSString()));
    }
    else
    {
        appendLogText(i18n("Slewing to target coordinates: RA (%1) DEC (%2) is rejected. (see notification)",
                           m_TargetCoord.ra().toHMSString(),
                           m_TargetCoord.dec().toDMSString()));
        setState(ALIGN_FAILED);
        Q_EMIT newStatus(state);
        solveB->setEnabled(true);
        loadSlewB->setEnabled(true);
    }
}

void Align::SlewToTarget()
{
    if (canSync && !m_SolveFromFile)
    {
        if (Options::astrometryDifferentialSlewing())
        {
            if (!RotatorGOTO)
            {
                m_TargetCoord.setRA(m_TargetCoord.ra() - m_AlignCoord.ra().deltaAngle(m_DestinationCoord.ra()));
                m_TargetCoord.setDec(m_TargetCoord.dec() - m_AlignCoord.dec().deltaAngle(m_DestinationCoord.dec()));
                qCDebug(KSTARS_EKOS_ALIGN) << "Differential slew - Target RA:" << m_TargetCoord.ra().toHMSString()
                                           << " DE:" << m_TargetCoord.dec().toDMSString();
            }
            Slew();
        }
        else
            Sync();
        return;
    }
    Slew();
}

void Align::getFormattedCoords(double ra, double dec, QString &ra_str, QString &dec_str)
{
    dms ra_s, dec_s;
    ra_s.setH(ra);
    dec_s.setD(dec);

    ra_str = QString("%1:%2:%3")
             .arg(ra_s.hour(), 2, 10, QChar('0'))
             .arg(ra_s.minute(), 2, 10, QChar('0'))
             .arg(ra_s.second(), 2, 10, QChar('0'));
    if (dec_s.Degrees() < 0)
        dec_str = QString("-%1:%2:%3")
                  .arg(std::abs(dec_s.degree()), 2, 10, QChar('0'))
                  .arg(std::abs(dec_s.arcmin()), 2, 10, QChar('0'))
                  .arg(dec_s.arcsec(), 2, 10, QChar('0'));
    else
        dec_str = QString("%1:%2:%3")
                  .arg(dec_s.degree(), 2, 10, QChar('0'))
                  .arg(dec_s.arcmin(), 2, 10, QChar('0'))
                  .arg(dec_s.arcsec(), 2, 10, QChar('0'));
}

bool Align::loadAndSlew(QString fileURL)
{
    if (fileURL.isEmpty())
        fileURL = QFileDialog::getOpenFileName(Ekos::Manager::Instance(), i18nc("@title:window", "Load Image"), dirPath,
                                               "Images (*.fits *.fits.fz *.fits.gz *.fit *.fts *.xisf *.xisf.gz "
                                               "*.jpg *.jpeg *.png *.gif *.bmp "
                                               "*.cr2 *.cr3 *.crw *.nef *.raf *.dng *.arw *.orf)");

    if (fileURL.isEmpty())
        return false;

    QFileInfo fileInfo(fileURL);
    if (fileInfo.exists() == false)
        return false;

    dirPath = fileInfo.absolutePath();

    RotatorGOTO = false;

    m_SolveFromFile = true;

    if (m_PolarAlignmentAssistant)
        m_PolarAlignmentAssistant->stopPAHProcess();

    slewR->setChecked(true);
    m_CurrentGotoMode = GOTO_SLEW;

    solveB->setEnabled(false);
    loadSlewB->setEnabled(false);
    stopB->setEnabled(true);
    pi->startAnimation();

    if (solverModeButtonGroup->checkedId() == SOLVER_REMOTE && m_RemoteParserDevice == nullptr)
    {
        appendLogText(i18n("No remote astrometry driver detected, switching to StellarSolver."));
        setSolverMode(SOLVER_LOCAL);
    }

    m_ImageData.clear();

    m_AlignView->loadFile(fileURL);
    connect(m_AlignView.get(), &FITSView::loaded, this, &Align::startSolving);

    return true;
}

bool Align::loadAndSlew(const QByteArray &image, const QString &extension)
{
    RotatorGOTO = false;
    m_SolveFromFile = true;
    RUN_PAH(stopPAHProcess());
    slewR->setChecked(true);
    m_CurrentGotoMode = GOTO_SLEW;
    solveB->setEnabled(false);
    loadSlewB->setEnabled(false);
    stopB->setEnabled(true);
    pi->startAnimation();

    // Must clear image data so we are forced to read the
    // image data again from align view when solving begins.
    m_ImageData.clear();
    QSharedPointer<FITSData> data;
    data.reset(new FITSData(), &QObject::deleteLater);
    data->setExtension(extension);
    data->loadFromBuffer(image);
    m_AlignView->loadData(data);
    startSolving();
    return true;
}

bool Align::checkIfRotationRequired()
{
    // Check if we need to perform any rotations.
    if (Options::astrometryUseRotator())
    {
        if (m_SolveFromFile)
        {
            m_TargetPositionAngle = solverFOV->PA();
            qCDebug(KSTARS_EKOS_ALIGN) << "Solving from file: Setting target PA to:" << m_TargetPositionAngle;
        }
        else // [Capture & Solve]
        {
            currentRotatorPA = solverFOV->PA();
            if (std::isnan(m_TargetPositionAngle) == false)
            {
                // If image pierside versus mount pierside is different and policy is lenient ...
                if (RotatorUtils::Instance()->Instance()->checkImageFlip() && (Options::astrometryFlipRotationAllowed()))
                {
                    // ... calculate "flipped" PA ...
                    sRawAngle = RotatorUtils::Instance()->calcRotatorAngle(m_TargetPositionAngle);
                    m_TargetPositionAngle = RotatorUtils::Instance()->calcCameraAngle(sRawAngle, true);
                    RotatorUtils::Instance()->setImagePierside(ISD::Mount::PIER_UNKNOWN); // ... once!
                }
                else if (Options::astrometryFlipRotationAllowed())
                {
                    double paDiff = std::abs(KSUtils::rangePA(currentRotatorPA - m_TargetPositionAngle));
                    if (std::abs(paDiff - 180.0) < ROTATOR_FLIP_TOLERANCE)
                    {
                        appendLogText(i18n("PA difference ~%1°, image is flipped. Preserving rotator angle.",
                                           QString::number(paDiff, 'f', 1)));
                        m_TargetPositionAngle = std::numeric_limits<double>::quiet_NaN();
                        return false;
                    }
                }
                // Match the position angle with rotator
                if  (m_Rotator != nullptr && m_Rotator->isConnected())
                {
                    if(std::abs(KSUtils::rangePA(currentRotatorPA - m_TargetPositionAngle)) * 60 >
                            Options::astrometryRotatorThreshold())
                    {
                        m_PreviousPAError = std::abs(KSUtils::rangePA(currentRotatorPA - m_TargetPositionAngle));
                        Q_EMIT newSolverResults(m_TargetPositionAngle, 0, 0, 0);
                        appendLogText(i18n("Setting camera position angle to %1 degrees ...", m_TargetPositionAngle));
                        setState(ALIGN_ROTATING);
                        Q_EMIT newStatus(state);
                        m_RotatorTimer.start();
                        QTimer::singleShot(1000, this, &Align::checkRotatorTimeout);
                        return true;
                    }
                    else
                    {
                        appendLogText(i18n("Camera position angle is within acceptable range."));
                        m_TargetPositionAngle = std::numeric_limits<double>::quiet_NaN();
                    }
                }
                else
                {
                    double current = currentRotatorPA;
                    double target = m_TargetPositionAngle;

                    double diff = KSUtils::rangePA(current - target);
                    double threshold = Options::astrometryRotatorThreshold() / 60.0;

                    appendLogText(i18n("Current PA is %1; Target PA is %2; diff: %3", current, target, diff));

                    Q_EMIT manualRotatorChanged(current, target, threshold);

                    m_ManualRotator->setRotatorDiff(current, target, diff);
                    if (std::abs(diff) > threshold)
                    {
                        targetAccuracyNotMet = true;
                        m_ManualRotator->show();
                        m_ManualRotator->raise();
                        setState(ALIGN_ROTATING);
                        Q_EMIT newStatus(state);
                        m_RotatorTimer.start();
                        QTimer::singleShot(1000, this, &Align::checkRotatorTimeout);
                        return true;
                    }
                    else
                    {
                        m_TargetPositionAngle = std::numeric_limits<double>::quiet_NaN();
                        targetAccuracyNotMet = false;
                    }
                }
            }
        }
    }
    return false;
}

void Align::checkRotatorTimeout()
{
    // Only check timeout if we're still in ALIGN_ROTATING state
    if (state != ALIGN_ROTATING)
        return;

    // Check if timeout has been reached
    if (m_RotatorTimer.isValid() &&
            m_RotatorTimer.elapsed() >= Options::captureOperationsTimeout() * 1000)
    {
        appendLogText(i18n("Rotator timeout after %1 seconds. Aborting alignment.",
                           Options::captureOperationsTimeout()));
        m_RotatorTimer.invalidate();
        setState(ALIGN_FAILED);
        Q_EMIT newStatus(state);
        solveB->setEnabled(true);
        loadSlewB->setEnabled(true);
    }
    else
    {
        // Check again in 1 second
        QTimer::singleShot(1000, this, &Align::checkRotatorTimeout);
    }
}

// m_wasSlewStarted can't be false for more than 10s after a slew starts.
bool Align::didSlewStart()
{
    if (m_wasSlewStarted)
        return true;
    if (slewStartTimer.isValid() && slewStartTimer.elapsed() > MAX_WAIT_FOR_SLEW_START_MSEC)
    {
        qCDebug(KSTARS_EKOS_ALIGN) << "Slew timed out...waited > 10s, it must have started already.";
        return true;
    }
    return false;
}

void Align::setTargetCoords(double ra0, double de0)
{
    SkyPoint target;
    target.setRA0(ra0);
    target.setDec0(de0);
    target.updateCoordsNow(KStarsData::Instance()->updateNum());
    setTarget(target);
}

void Align::setTarget(const SkyPoint &targetCoord)
{
    m_TargetCoord = targetCoord;
    qCInfo(KSTARS_EKOS_ALIGN) << "Target coordinates updated to JNow RA:" << m_TargetCoord.ra().toHMSString()
                              << "DE:" << m_TargetCoord.dec().toDMSString();
}

QList<double> Align::getSolutionResult()
{
    return QList<double>() << sOrientation << sRA << sDEC;
}

QList<double> Align::getTargetCoords()
{
    return QList<double>() << m_TargetCoord.ra0().Hours() << m_TargetCoord.dec0().Degrees();
}

void Align::setTargetPositionAngle(double value)
{
    m_TargetPositionAngle =  value;
    qCDebug(KSTARS_EKOS_ALIGN) << "Target PA updated to: " << m_TargetPositionAngle;
}

}
