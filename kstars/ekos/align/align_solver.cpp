/*
    SPDX-FileCopyrightText: 2013-2021 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2018-2020 Robert Lancaster <rlancaste@gmail.com>
    SPDX-FileCopyrightText: 2019-2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "align.h"
#include "align_p.h"
#include "alignadaptor.h"
#include "alignview.h"
#include "fov.h"
#include "manualrotator.h"
#include "Options.h"
#include "polaralignmentassistant.h"
#include "remoteastrometryparser.h"
#include "indi/indirotator.h"

#include "ekos/auxiliary/darkprocessor.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/rotatorutils.h"
#include "ekos/auxiliary/solverutils.h"
#include "ekos/manager.h"
#include "fitsviewer/fitsdata.h"
#include "fitsviewer/fitsview.h"
#include "indi/clientmanager.h"
#include "indi/driverinfo.h"
#include "ksnotification.h"
#include "kspaths.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymapcomposite.h"

#include <basedevice.h>
#include <indicom.h>

#include <ekos_align_debug.h>

#define ALIGN_CAPTURE_TIMEOUT_THRESHOLD 30000

namespace Ekos
{

using PAA = PolarAlignmentAssistant;

bool Align::captureAndSolve(bool initialCall)
{
    // Set target to current telescope position, if no object is selected yet.
    if (m_TargetCoord.ra().degree() < 0) // see default constructor skypoint()
    {
        if (m_TelescopeCoord.isValid() == false)
        {
            appendLogText(i18n("Mount coordinates are invalid. Check mount connection and try again."));
            KSNotification::event(QLatin1String("AlignFailed"), i18n("Astrometry alignment failed"), KSNotification::Align,
                                  KSNotification::Alert);
            return false;
        }

        m_TargetCoord = m_TelescopeCoord;
        appendLogText(i18n("Setting target to RA:%1 DEC:%2",
                           m_TargetCoord.ra().toHMSString(true), m_TargetCoord.dec().toDMSString(true)));
    }

    if (initialCall)
        m_DestinationCoord = m_TargetCoord;

    qCDebug(KSTARS_EKOS_ALIGN) << "Capture&Solve - Target RA:" <<  m_TargetCoord.ra().toHMSString(true)
                               << " DE:" << m_TargetCoord.dec().toDMSString(true);
    qCDebug(KSTARS_EKOS_ALIGN) << "Capture&Solve - Destination RA:" <<  m_DestinationCoord.ra().toHMSString(true)
                               << " DE:" << m_DestinationCoord.dec().toDMSString(true);
    m_RemoteAlignTimer.stop();
    m_CaptureTimer.stop();

    if (m_Camera == nullptr)
    {
        appendLogText(i18n("Error: No camera detected."));
        return false;
    }

    if (m_Camera->isConnected() == false)
    {
        appendLogText(i18n("Error: lost connection to camera."));
        KSNotification::event(QLatin1String("AlignFailed"), i18n("Astrometry alignment failed"), KSNotification::Align,
                              KSNotification::Alert);
        return false;
    }

    if (m_Camera->isBLOBEnabled() == false)
    {
        m_Camera->setBLOBEnabled(true);
    }

    if (m_FocalLength == -1 || m_Aperture == -1)
    {
        KSNotification::error(
            i18n("Telescope aperture and focal length are missing. Please check your optical train settings and try again."));
        return false;
    }

    if (m_CameraPixelWidth == -1 || m_CameraPixelHeight == -1)
    {
        KSNotification::error(i18n("Camera pixel size is missing. Please check your driver settings and try again."));
        return false;
    }

    if (m_FilterWheel != nullptr)
    {
        if (m_FilterWheel->isConnected() == false)
        {
            appendLogText(i18n("Error: lost connection to filter wheel."));
            return false;
        }

        int targetPosition = alignFilter->currentIndex() + 1;

        if (targetPosition > 0 && targetPosition != currentFilterPosition)
        {
            filterPositionPending    = true;
            m_FilterManager->setFilterPosition(targetPosition, FilterManager::NO_AUTOFOCUS_POLICY);
            setState(ALIGN_PROGRESS);
            return true;
        }
    }

    auto clientManager = m_Camera->getDriverInfo()->getClientManager();
    if (clientManager && clientManager->getBLOBMode(m_Camera->getDeviceName().toLatin1().constData(), "CCD1") == B_NEVER)
    {
        if (KMessageBox::warningContinueCancel(
                    nullptr, i18n("Image transfer is disabled for this camera. Would you like to enable it?")) ==
                KMessageBox::Continue)
        {
            clientManager->setBLOBMode(B_ONLY, m_Camera->getDeviceName().toLatin1().constData(), "CCD1");
            clientManager->setBLOBMode(B_ONLY, m_Camera->getDeviceName().toLatin1().constData(), "CCD2");
        }
        else
        {
            return false;
        }
    }

    // Check dustcap status before proceeding with capture
    if (m_DustCap && m_DustCap->isParked())
    {
        appendLogText(i18n("Dustcap is parked. Unparking before capture and solve..."));
        m_waitingForDustCapUnpark = true;
        m_DustCap->unpark();
        return true;
    }
    else if (m_DustCap && m_DustCap->isUnParked() == false && m_DustCap->status() != ISD::DustCap::CAP_IDLE)
    {
        appendLogText(i18n("Dustcap is not ready (status: %1). Waiting before capture and solve...",
                           ISD::DustCap::getStatusString(m_DustCap->status())));
        m_waitingForDustCapUnpark = true;
        return true;
    }

    double seqExpose = alignExposure->value();

    auto targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);

    if (m_FocusState >= FOCUS_PROGRESS)
    {
        appendLogText(i18n("Cannot capture while focus module is busy. Retrying in %1 seconds...",
                           CAPTURE_RETRY_DELAY / 1000));
        m_CaptureTimer.start(CAPTURE_RETRY_DELAY);
        return true;
    }

    if (targetChip->isCapturing())
    {
        if (matchPAHStage(PAA::PAH_REFRESH))
        {
            qCWarning(KSTARS_EKOS_ALIGN) << "PAH Refresh: capture requested while camera is still exposing – skipping retry timer.";
            return true;
        }
        appendLogText(i18n("Cannot capture while camera exposure is in progress. Retrying in %1 seconds...",
                           CAPTURE_RETRY_DELAY / 1000));
        m_CaptureTimer.start(CAPTURE_RETRY_DELAY);
        return true;
    }

    if (m_Dome && m_Dome->isMoving())
    {
        qCWarning(KSTARS_EKOS_ALIGN) << "Cannot capture while dome is in motion. Retrying in" <<  CAPTURE_RETRY_DELAY / 1000 <<
                                        "seconds...";
        m_CaptureTimer.start(CAPTURE_RETRY_DELAY);
        return true;
    }

    // Let rotate the camera BEFORE taking a capture in [Capture & Solve]
    if (!m_SolveFromFile && m_Rotator && m_Rotator->absoluteAngleState() == IPS_BUSY)
    {
        int TimeOut = CAPTURE_ROTATOR_DELAY;
        switch (m_CaptureTimeoutCounter)
        {
            case 0:
            {
                auto absAngle = 0;
                if ((absAngle = m_Rotator->getNumber("ABS_ROTATOR_ANGLE")[0].getValue()))
                {
                    RotatorUtils::Instance()->startTimeFrame(absAngle);
                    m_estimateRotatorTimeFrame = true;
                    appendLogText(i18n("Cannot capture while rotator is busy: Time delay estimate started..."));
                }
                m_CaptureTimer.start(TimeOut);
                break;
            }
            case 1:
            {
                TimeOut = m_RotatorTimeFrame * 1000;
                [[fallthrough]];
            }
            default:
            {
                TimeOut *= m_CaptureTimeoutCounter;
                TimeOut = std::min(TimeOut, 60000);
                m_estimateRotatorTimeFrame = false;
                appendLogText(i18n("Cannot capture while rotator is busy: Retrying in %1 seconds...", TimeOut / 1000));
                m_CaptureTimer.start(TimeOut);
            }
        }
        return true;
    }

    m_AlignView->setBaseSize(alignWidget->size());
    m_AlignView->setProperty("suspended", (solverModeButtonGroup->checkedId() == SOLVER_LOCAL
                                           && alignDarkFrame->isChecked()));

    // Optimization: If we have a valid PA from a previous solve and mount hasn't moved,
    // rotate before capturing instead of capture→solve→rotate→capture→solve.
    if (!m_SolveFromFile && std::isnan(m_TargetPositionAngle) == false && m_PreviousPAValid
            && Options::astrometryUseRotator())
    {
        // Only for automatic rotator (not manual rotator) for safety
        if (m_Rotator != nullptr && m_Rotator->isConnected())
        {
            double paDiffArcmin = std::abs(KSUtils::rangePA(currentRotatorPA - m_TargetPositionAngle)) * 60;

            if (paDiffArcmin > Options::astrometryRotatorThreshold())
            {
                m_PreviousPAError = std::abs(KSUtils::rangePA(currentRotatorPA - m_TargetPositionAngle));
                Q_EMIT newRotatorCommand(m_TargetPositionAngle);
                appendLogText(i18n("Setting camera position angle to %1 degrees (using cached PA from previous solve)...",
                                   m_TargetPositionAngle));
                setState(ALIGN_ROTATING);
                Q_EMIT newStatus(state);
                m_RotatorTimer.start();
                QTimer::singleShot(1000, this, &Align::checkRotatorTimeout);
                // Mark PA as no longer valid (we're rotating, will re-validate after next solve)
                m_PreviousPAValid = false;
                m_RotateBeforeSolve = true;
                return true;
            }
        }
    }

    connect(m_Camera, &ISD::Camera::newImage, this, &Ekos::Align::processData);
    connect(m_Camera, &ISD::Camera::newExposureValue, this, &Ekos::Align::checkCameraExposureProgress);

    if (solverModeButtonGroup->checkedId() == SOLVER_REMOTE && remoteParser.get() != nullptr)
    {
        if (m_RemoteParserDevice == nullptr)
        {
            appendLogText(i18n("No remote astrometry driver detected, switching to StellarSolver."));
            setSolverMode(SOLVER_LOCAL);
        }
        else
        {
            auto activeDevices = m_RemoteParserDevice->getBaseDevice().getText("ACTIVE_DEVICES");
            if (activeDevices)
            {
                auto activeCCD = activeDevices.findWidgetByName("ACTIVE_CCD");
                if (QString(activeCCD->text) != m_Camera->getDeviceName())
                {
                    activeCCD->setText(m_Camera->getDeviceName().toLatin1().constData());
                    m_RemoteParserDevice->getClientManager()->sendNewProperty(activeDevices);
                }
            }

            dynamic_cast<RemoteAstrometryParser *>(remoteParser.get())->setEnabled(true);
            dynamic_cast<RemoteAstrometryParser *>(remoteParser.get())->sendArgs(generateRemoteArgs(QSharedPointer<FITSData>()));
        }
    }

    // Remove temporary FITS files left before by the solver
    QDir dir(QDir::tempPath());
    dir.setNameFilters(QStringList() << "fits*"  << "tmp.*");
    dir.setFilter(QDir::Files);
    for (auto &dirFile : dir.entryList())
        dir.remove(dirFile);

    prepareCapture(targetChip);

    double captureExposure = seqExpose;
    if (matchPAHStage(PAA::PAH_REFRESH))
        captureExposure = m_PolarAlignmentAssistant->getPAHExposureDuration();
    targetChip->capture(captureExposure);

    // Start capture timeout: exposure + 30s threshold
    m_CaptureTimer.start(captureExposure * 1000 + ALIGN_CAPTURE_TIMEOUT_THRESHOLD);

    solveB->setEnabled(false);
    loadSlewB->setEnabled(false);
    stopB->setEnabled(true);
    pi->startAnimation();

    RotatorGOTO = false;

    setState(ALIGN_PROGRESS);
    Q_EMIT newStatus(state);
    solverFOV->setProperty("visible", true);

    if (matchPAHStage(PAA::PAH_REFRESH))
        return true;

    appendLogText(i18n("Capturing image..."));

    if (!m_Mount)
        return true;

    double ra, dec;
    m_Mount->getEqCoords(&ra, &dec);
    if (!m_SolveFromFile)
    {
        int currentRow = solutionTable->rowCount();
        solutionTable->insertRow(currentRow);
        for (int i = 4; i < 6; i++)
        {
            QTableWidgetItem *disabledBox = new QTableWidgetItem();
            disabledBox->setFlags(Qt::ItemIsSelectable);
            solutionTable->setItem(currentRow, i, disabledBox);
        }

        QTableWidgetItem *RAReport = new QTableWidgetItem();
        RAReport->setText(ScopeRAOut->text());
        RAReport->setTextAlignment(Qt::AlignHCenter);
        RAReport->setFlags(Qt::ItemIsSelectable);
        solutionTable->setItem(currentRow, 0, RAReport);

        QTableWidgetItem *DECReport = new QTableWidgetItem();
        DECReport->setText(ScopeDecOut->text());
        DECReport->setTextAlignment(Qt::AlignHCenter);
        DECReport->setFlags(Qt::ItemIsSelectable);
        solutionTable->setItem(currentRow, 1, DECReport);

        double maxrad = 1.0;
        SkyObject *so =
            KStarsData::Instance()->skyComposite()->objectNearest(new SkyPoint(dms(ra * 15), dms(dec)), maxrad);
        QString name;
        if (so)
            name = so->longname();
        else
            name = "None";
        QTableWidgetItem *ObjNameReport = new QTableWidgetItem();
        ObjNameReport->setText(name);
        ObjNameReport->setTextAlignment(Qt::AlignHCenter);
        ObjNameReport->setFlags(Qt::ItemIsSelectable);
        solutionTable->setItem(currentRow, 2, ObjNameReport);
#ifdef Q_OS_MACOS
        repaint();
#endif

        QProgressIndicator *alignIndicator = new QProgressIndicator(this);
        solutionTable->setCellWidget(currentRow, 3, alignIndicator);
        alignIndicator->startAnimation();
#ifdef Q_OS_MACOS
        repaint();
#endif
    }

    return true;
}

void Align::processData(const QSharedPointer<FITSData> &data)
{
    auto chip = data->property("chip");
    if (chip.isValid() && chip.toInt() == ISD::CameraChip::GUIDE_CCD)
        return;

    // Stop the capture timeout timer since we got the image
    m_CaptureTimer.stop();
    m_CaptureTimeoutCounter = 0;

    // disconnect() with a null sender is a safe Qt no-op, so these calls are safe
    // even if m_Camera has been set to nullptr already.
    disconnect(m_Camera, &ISD::Camera::newImage, this, &Ekos::Align::processData);
    disconnect(m_Camera, &ISD::Camera::newExposureValue, this, &Ekos::Align::checkCameraExposureProgress);

    // Guard against m_Camera being null: if the driver was removed/restarted the
    // newImage signal may have been already queued when m_Camera was set to nullptr.
    // getChip() on a null pointer would cause SIGSEGV at the primaryChip offset.
    if (!m_Camera)
        return;

    if (data)
    {
        m_AlignView->loadData(data);
        m_ImageData = data;
    }
    else
        m_ImageData.reset();

    RUN_PAH(setImageData(m_ImageData));

    if (matchPAHStage(PAA::PAH_REFRESH))
    {
        setCaptureComplete();
        return;
    }
    else
        appendLogText(i18n("Image received."));

    if (solverModeButtonGroup->checkedId() == SOLVER_LOCAL)
    {
        if (alignDarkFrame->isChecked())
        {
            int x, y, w, h, binx = 1, biny = 1;
            ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
            targetChip->getFrame(&x, &y, &w, &h);
            targetChip->getBinning(&binx, &biny);

            uint16_t offsetX = x / binx;
            uint16_t offsetY = y / biny;

            m_DarkProcessor->denoise(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()),
                                     targetChip, m_ImageData, alignExposure->value(), offsetX, offsetY);
            return;
        }

        setCaptureComplete();
    }
}

void Align::prepareCapture(ISD::CameraChip *targetChip)
{
    if (m_Camera->getUploadMode() == ISD::Camera::UPLOAD_REMOTE)
    {
        rememberUploadMode = ISD::Camera::UPLOAD_REMOTE;
        m_Camera->setUploadMode(ISD::Camera::UPLOAD_CLIENT);
    }

    if (m_Camera->isFastExposureEnabled())
    {
        m_RememberCameraFastExposure = true;
        m_Camera->setFastExposureEnabled(false);
    }

    m_Camera->setEncodingFormat("FITS");
    targetChip->resetFrame();
    targetChip->setBatchMode(false);
    targetChip->setCaptureMode(FITS_ALIGN);
    targetChip->setFrameType(FRAME_LIGHT);

    int bin = alignBinning->currentIndex() + 1;
    targetChip->setBinning(bin, bin);

    if (m_Camera->hasGain() && alignGain->isEnabled() && alignGain->value() > alignGainSpecialValue)
        m_Camera->setGain(alignGain->value());
    if (alignISO->currentIndex() >= 0)
        targetChip->setISOIndex(alignISO->currentIndex());
}

void Align::setCaptureComplete()
{
    if (matchPAHStage(PAA::PAH_REFRESH))
    {
        Q_EMIT newFrame(m_AlignView);
        m_PolarAlignmentAssistant->processPAHRefresh();
        return;
    }

    Q_EMIT newImage(m_AlignView);

    solverFOV->setImage(m_AlignView->getDisplayImage());

    if (Options::saveAlignImages())
    {
        QDir dir;
        QDateTime now = KStarsData::Instance()->lt();
        QString path = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("align/" +
                       now.toString("yyyy-MM-dd"));
        dir.mkpath(path);
        QString name     = "align_frame_" + now.toString("HH-mm-ss") + ".fits";
        QString filename = path + QStringLiteral("/") + name;
        if (m_ImageData)
            m_ImageData->saveImage(filename);
    }

    startSolving();
}

void Align::setSolverAction(int mode)
{
    if (gotoModeButtonGroup->button(mode))
        gotoModeButtonGroup->button(mode)->setChecked(true);
    m_CurrentGotoMode = static_cast<GotoMode>(mode);
}

void Align::startSolving()
{
    if (m_Solver.get() && m_Solver->isRunning())
        m_Solver->abort(true);

    QStringList astrometryDataDirs = KSUtils::getAstrometryDataDirs();
    disconnect(m_AlignView.get(), &FITSView::loaded, this, &Align::startSolving);

    m_UsedScale = false;
    m_UsedPosition = false;
    m_ScaleUsed = 0;
    m_RAUsed = 0;
    m_DECUsed = 0;

    if (solverModeButtonGroup->checkedId() == SOLVER_LOCAL)
    {
        QString filenameToUse = "";

        if(Options::solverType() != SSolver::SOLVER_ASTAP
                && Options::solverType() != SSolver::SOLVER_WATNEYASTROMETRY)
        {
            bool foundAnIndex = false;
            for(auto &dataDir : astrometryDataDirs)
            {
                QDir dir = QDir(dataDir);
                if(dir.exists())
                {
                    dir.setNameFilters(QStringList() << "*.fits");
                    QStringList indexList = dir.entryList();
                    if(indexList.count() > 0)
                        foundAnIndex = true;
                }
            }
            if(!foundAnIndex)
            {
                appendLogText(
                    i18n("No index files were found on your system in the specified index file directories."
                     "Please download some index files or add the correct directory to the list."));
                KConfigDialog * alignSettings = KConfigDialog::exists("alignsettings");
                if(alignSettings && m_IndexFilesPage)
                {
                    alignSettings->setCurrentPage(m_IndexFilesPage);
                    alignSettings->show();
                }
            }
        }

        if (!m_ImageData)
            m_ImageData = m_AlignView->imageData();

        SSolver::Parameters params;
        try
        {
            params = m_StellarSolverProfiles.at(Options::solveOptionsProfile());
        }
        catch (std::out_of_range const &)
        {
            params = m_StellarSolverProfiles[0];
        }

        params.partition = Options::stellarSolverPartition();
        params.threshold_bg_multiple = m_dynamicThreshold;
        if (m_dynamicThreshold == 1 && Options::astrometryDynamicThreshold())
            params.convFilterType = CONV_DEFAULT;

        m_Solver.reset(new SolverUtils(params, Options::astrometryTimeout()));

        const SSolver::SolverType type = static_cast<SSolver::SolverType>(Options::solverType());
        if(type == SSolver::SOLVER_LOCALASTROMETRY || type == SSolver::SOLVER_ASTAP || type == SSolver::SOLVER_WATNEYASTROMETRY)
        {
            QString filename = QDir::tempPath() + QString("/solver%1.fits").arg(QUuid::createUuid().toString().remove(
                                   QRegularExpression("[-{}]")));
            m_AlignView->saveImage(filename);
            filenameToUse = filename;
        }

        if(type == SSolver::SOLVER_ONLINEASTROMETRY )
        {
            QString filename = QDir::tempPath() + QString("/solver%1.fits").arg(QUuid::createUuid().toString().remove(
                                   QRegularExpression("[-{}]")));
            m_AlignView->saveImage(filename);
            filenameToUse = filename;
        }

        bool useImageScale = Options::astrometryUseImageScale();
        if (useBlindScale == BLIND_ENGAGNED)
        {
            useImageScale = false;
            useBlindScale = BLIND_USED;
            appendLogText(i18n("Solving with blind image scale..."));
        }

        bool useImagePosition = Options::astrometryUsePosition();
        if (useBlindPosition == BLIND_ENGAGNED)
        {
            useImagePosition = false;
            useBlindPosition = BLIND_USED;
            appendLogText(i18n("Solving with blind image position..."));
        }

        if (useBlindDynamicThreshold == BLIND_ENGAGNED)
        {
            useBlindDynamicThreshold = BLIND_USED;
            appendLogText(i18n("Solving with blind dynamic threshold..."));
        }

        if (m_SolveFromFile)
        {
            FITSImage::Solution solution;
            m_ImageData->parseSolution(solution);

            if (useImageScale && solution.pixscale > 0)
            {
                m_UsedScale = true;
                m_ScaleUsed = solution.pixscale;
                m_Solver->useScale(true, solution.pixscale, solution.pixscale);
            }
            else
                m_Solver->useScale(false, 0, 0);

            if (useImagePosition && solution.ra > 0)
            {
                m_UsedPosition = true;
                m_RAUsed = solution.ra;
                m_DECUsed = solution.dec;
                m_Solver->usePosition(true, solution.ra, solution.dec);
            }
            else
                m_Solver->usePosition(false, 0, 0);

            QVariant value = "";
            if (!m_ImageData->getRecordValue("PIERSIDE", value))
            {
                appendLogText(i18n("Loaded image does not have pierside information"));
                m_TargetPierside = ISD::Mount::PIER_UNKNOWN;
            }
            else
            {
                appendLogText(i18n("Loaded image was taken on pierside %1", value.toString()));
                (value == "WEST") ? m_TargetPierside = ISD::Mount::PIER_WEST : m_TargetPierside = ISD::Mount::PIER_EAST;
            }
            RotatorUtils::Instance()->Instance()->setImagePierside(m_TargetPierside);
        }
        else
        {
            if (useImageScale)
            {
                m_UsedScale = true;
                m_ScaleUsed = Options::astrometryImageScaleLow();

                SSolver::ScaleUnits units = static_cast<SSolver::ScaleUnits>(Options::astrometryImageScaleUnits());
                m_Solver->useScale(true, Options::astrometryImageScaleLow(), Options::astrometryImageScaleHigh(), units);
            }
            else
                m_Solver->useScale(false, 0, 0);

            if(useImagePosition)
            {
                m_UsedPosition = true;
                m_RAUsed = m_TelescopeCoord.ra().Degrees();
                m_DECUsed = m_TelescopeCoord.dec().Degrees();
                m_Solver->usePosition(true, m_TelescopeCoord.ra().Degrees(), m_TelescopeCoord.dec().Degrees());
            }
            else
                m_Solver->usePosition(false, 0, 0);
        }

        connect(m_Solver.get(), &SolverUtils::done, this, &Align::solverDone, Qt::UniqueConnection);

        if (filenameToUse.isEmpty())
            m_Solver->runSolver(m_ImageData);
        else
            m_Solver->runSolver(filenameToUse);
    }
    else
    {
        if (m_ImageData.isNull())
            m_ImageData = m_AlignView->imageData();
        m_RemoteElapsedTimer.start();
        m_RemoteAlignTimer.start();
        remoteParser->startSolver(m_ImageData->filename(), generateRemoteArgs(m_ImageData), false);
    }

    if (matchPAHStage(PAA::PAH_FIRST_CAPTURE) ||
            matchPAHStage(PAA::PAH_SECOND_CAPTURE) ||
            matchPAHStage(PAA::PAH_THIRD_CAPTURE) ||
            matchPAHStage(PAA::PAH_FIRST_SOLVE) ||
            matchPAHStage(PAA::PAH_SECOND_SOLVE) ||
            matchPAHStage(PAA::PAH_THIRD_SOLVE) ||
            syncR->isChecked())
        errOut->clear();

    setState(ALIGN_PROGRESS);
    Q_EMIT newStatus(state);
}

void Align::solverDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds)
{
    disconnect(m_Solver.get(), &SolverUtils::done, this, &Align::solverDone);
    if (timedOut || !success)
    {
        if (elapsedSeconds > 0)
        {
            if (timedOut)
                appendLogText(i18n("Solver timed out after %1 seconds.",
                                   QString::number(elapsedSeconds, 'f', 2)));
            else
                appendLogText(i18n("Solver failed after %1 seconds.",
                                   QString::number(elapsedSeconds, 'f', 2)));
        }

        if (matchPAHStage(PAA::PAH_FIRST_CAPTURE) ||
                matchPAHStage(PAA::PAH_SECOND_CAPTURE) ||
                matchPAHStage(PAA::PAH_THIRD_CAPTURE) ||
                matchPAHStage(PAA::PAH_FIRST_SOLVE) ||
                matchPAHStage(PAA::PAH_SECOND_SOLVE) ||
                matchPAHStage(PAA::PAH_THIRD_SOLVE))
        {
            if (CHECK_PAH(processSolverFailure()))
                return;
            else
                setState(ALIGN_ABORTED);
        }
        solverFailed();
        return;
    }
    else
    {
        if (elapsedSeconds > 0)
            appendLogText(i18n("Solver completed after %1 seconds.", QString::number(elapsedSeconds, 'f', 2)));
        const bool eastToTheRight = solution.parity == FITSImage::POSITIVE ? false : true;
        solverFinished(solution.orientation, solution.ra, solution.dec, solution.pixscale, eastToTheRight);
    }
}

void Align::solverFinished(double orientation, double ra, double dec, double pixscale, bool eastToTheRight)
{
    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);

    sOrientation = orientation;
    sRA          = ra;
    sDEC         = dec;

    m_RemoteAlignTimer.stop();
    if (solverModeButtonGroup->checkedId() == SOLVER_REMOTE && m_RemoteParserDevice && remoteParser.get())
    {
        const double elapsed = m_RemoteElapsedTimer.elapsed() / 1000.0;
        if (elapsed > 0)
            appendLogText(i18n("Remote solver completed after %1 seconds.", QString::number(elapsed, 'f', 2)));

        dynamic_cast<RemoteAstrometryParser *>(remoteParser.get())->setEnabled(false);
    }

    int binx, biny;
    ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);
    targetChip->getBinning(&binx, &biny);

    if (Options::alignmentLogging())
    {
        QString parityString = eastToTheRight ? "neg" : "pos";
        appendLogText(i18n("Solver RA (%1) DEC (%2) Orientation (%3) Pixel Scale (%4) Parity (%5)", QString::number(ra, 'f', 5),
                           QString::number(dec, 'f', 5), QString::number(orientation, 'f', 5),
                           QString::number(pixscale, 'f', 5), parityString));
    }

    if (!m_SolveFromFile &&
            (isEqual(m_FOVWidth, 0) || m_EffectiveFOVPending || std::abs(pixscale - m_FOVPixelScale) > 0.005) &&
            pixscale > 0)
    {
        double newFOVW = m_CameraWidth * pixscale / binx / 60.0;
        double newFOVH = m_CameraHeight * pixscale / biny / 60.0;

        calculateEffectiveFocalLength(newFOVW);
        saveNewEffectiveFOV(newFOVW, newFOVH);

        m_EffectiveFOVPending = false;
    }

    m_AlignCoord.setRA0(ra / 15.0);
    m_AlignCoord.setDec0(dec);

    m_AlignCoord.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());
    m_AlignCoord.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

    if (!m_SolveFromFile)
    {
        pixScaleOut->setText(QString::number(pixscale, 'f', 2));
        calculateAlignTargetDiff();
    }

    double solverPA = KSUtils::rotationToPositionAngle(orientation);
    solverFOV->setCenter(m_AlignCoord);
    solverFOV->setPA(solverPA);
    solverFOV->setImageDisplay(Options::astrometrySolverOverlay());
    sensorFOV->setPA(solverPA);

    // Mark that we have a valid PA from this solve that can be used for rotate-first optimization
    m_PreviousPAValid = true;
    m_PAValidPierSide = m_Mount ? m_Mount->pierSide() : ISD::Mount::PIER_UNKNOWN;

    PAOut->setText(QString::number(solverPA, 'f', 2));

    QString ra_dms, dec_dms;
    getFormattedCoords(m_AlignCoord.ra().Hours(), m_AlignCoord.dec().Degrees(), ra_dms, dec_dms);

    SolverRAOut->setText(ra_dms);
    SolverDecOut->setText(dec_dms);

    if (Options::astrometrySolverWCS())
    {
        auto ccdRotation = m_Camera->getNumber("CCD_ROTATION");
        if (ccdRotation)
        {
            auto rotation = ccdRotation.findWidgetByName("CCD_ROTATION_VALUE");
            if (rotation)
            {
                auto clientManager = m_Camera->getDriverInfo()->getClientManager();
                if (clientManager)
                {
                    rotation->setValue(orientation);
                    clientManager->sendNewProperty(ccdRotation);
                }

                if (clientManager && m_wcsSynced == false)
                {
                    appendLogText(
                        i18n("WCS information updated. Images captured from this point forward shall have valid WCS."));

                    auto telescopeInfo = m_Mount->getNumber("TELESCOPE_INFO");
                    if (telescopeInfo)
                        clientManager->sendNewProperty(telescopeInfo);

                    m_wcsSynced = true;
                }
            }
        }
    }

    m_CaptureErrorCounter = 0;
    m_SlewErrorCounter = 0;
    m_CaptureTimeoutCounter = 0;

    appendLogText(
        i18n("Solution coordinates: RA (%1) DEC (%2) Telescope Coordinates: RA (%3) DEC (%4) Target Coordinates: RA (%5) DEC (%6)",
             m_AlignCoord.ra().toHMSString(),
             m_AlignCoord.dec().toDMSString(),
             m_TelescopeCoord.ra().toHMSString(),
             m_TelescopeCoord.dec().toDMSString(),
             m_TargetCoord.ra().toHMSString(),
             m_TargetCoord.dec().toDMSString()));

    if (!m_SolveFromFile && m_CurrentGotoMode == GOTO_SLEW)
    {
        dms diffDeg(m_TargetDiffTotal / 3600.0);
        appendLogText(i18n("Target is within %1 degrees of solution coordinates.", diffDeg.toDMSString()));
    }

    if (rememberUploadMode != m_Camera->getUploadMode())
        m_Camera->setUploadMode(rememberUploadMode);

    if (m_RememberCameraFastExposure)
    {
        m_RememberCameraFastExposure = false;
        m_Camera->setFastExposureEnabled(true);
    }

    std::unique_ptr<QTableWidgetItem> statusReport(new QTableWidgetItem());
    int currentRow = solutionTable->rowCount() - 1;
    if (!m_SolveFromFile)
    {
        stopProgressAnimation();
        solutionTable->setCellWidget(currentRow, 3, new QWidget());
        statusReport->setFlags(Qt::ItemIsSelectable);
        if (m_Rotator != nullptr && m_Rotator->isConnected())
        {
            if (auto absAngle = m_Rotator->getNumber("ABS_ROTATOR_ANGLE"))
            {
                sRawAngle = absAngle[0].getValue();
                double OffsetAngle = RotatorUtils::Instance()->calcOffsetAngle(sRawAngle, solverPA);
                RotatorUtils::Instance()->updateOffset(OffsetAngle);
                auto reverseStatus = "Unknown";
                auto reverseProperty = m_Rotator->getSwitch("ROTATOR_REVERSE");
                if (reverseProperty)
                {
                    if (reverseProperty[0].getState() == ISS_ON)
                        reverseStatus = "Reversed Direction";
                    else
                        reverseStatus = "Normal Direction";
                }
                qCDebug(KSTARS_EKOS_ALIGN) << "Raw Rotator Angle:" << sRawAngle << "Rotator PA:" << solverPA
                                           << "Rotator Offset:" << OffsetAngle << "Direction:" << reverseStatus;

                if (std::isnan(m_TargetPositionAngle) == false && m_PreviousPAError >= 0)
                {
                    double newPAError = std::abs(KSUtils::rangePA(solverPA - m_TargetPositionAngle));
                    if (newPAError > m_PreviousPAError + 0.5)
                    {
                        // First wrong-direction detection: try auto-reversing the rotator
                        if (!m_RotatorAutoReversed)
                        {
                            auto reverseProperty = m_Rotator->getSwitch("ROTATOR_REVERSE");
                            bool isReversed = reverseProperty ? (reverseProperty[0].getState() == ISS_ON) : false;
                            bool newReversed = !isReversed;

                            appendLogText(i18n("Rotator is moving in the wrong direction. "
                                               "Automatically reversing rotator direction and retrying..."));
                            qCDebug(KSTARS_EKOS_ALIGN) << "Rotator wrong direction detected."
                                                       << "Previous PA error:" << m_PreviousPAError
                                                       << "New PA error:" << newPAError
                                                       << "Auto-reversing direction to:"
                                                       << (newReversed ? "reversed" : "normal");

                            m_Rotator->setReversed(newReversed);
                            RotatorUtils::Instance()->setReversed(newReversed);
                            m_RotatorAutoReversed = true;
                            m_PreviousPAError = -1;
                            // Re-issue the rotation command with the reversed direction
                            checkIfRotationRequired();
                            return;
                        }

                        // Auto-reverse already attempted — abort
                        appendLogText(i18n("Rotator is moving in the wrong direction. "
                                           "The position angle error increased from %1° to %2° after rotation. "
                                           "Auto-reverse was already tried. Please check rotator configuration.",
                                           QString::number(m_PreviousPAError, 'f', 2),
                                           QString::number(newPAError, 'f', 2)));
                        qCDebug(KSTARS_EKOS_ALIGN) << "Rotator wrong direction detected after auto-reverse."
                                                   << "Previous PA error:" << m_PreviousPAError
                                                   << "New PA error:" << newPAError;
                        m_TargetPositionAngle = std::numeric_limits<double>::quiet_NaN();
                        m_PreviousPAError = -1;
                        m_RotatorAutoReversed = false;
                        setState(ALIGN_FAILED);
                        emit newStatus(state);
                        solveB->setEnabled(true);
                        loadSlewB->setEnabled(true);
                        return;
                    }
                    m_PreviousPAError = -1;
                    m_RotatorAutoReversed = false;
                }

                Q_EMIT newSolverResults(solverPA, ra, dec, pixscale);
                appendLogText(i18n("Camera position angle is %1 degrees.", RotatorUtils::Instance()->calcCameraAngle(sRawAngle, false)));
            }
        }
    }

    QJsonObject solution =
    {
        {"camera", m_Camera->getDeviceName()},
        {"ra", SolverRAOut->text()},
        {"ra.Hours", m_AlignCoord.ra().Hours()},
        {"de.Degrees", m_AlignCoord.dec().Degrees()},
        {"de", SolverDecOut->text()},
        {"dRA", m_TargetDiffRA},
        {"dDE", m_TargetDiffDE},
        {"dAZ", m_TargetDiffAZ},
        {"dAL", m_TargetDiffAL},
        {"targetDiff", m_TargetDiffTotal},
        {"pix", pixscale},
        {"PA", solverPA},
        {"fov", FOVOut->text()},
    };
    Q_EMIT newSolution(solution.toVariantMap());

    if (Options::astrometryDynamicThreshold())
    {
        int numStarsFound = m_Solver.get() ? m_Solver->getNumStarsFound() : 0;
        auto newThreshold = std::min(std::max(1.0,
                                              numStarsFound < DYNAMIC_THRESHOLD_STARS ? m_dynamicThreshold / 2.0 : m_dynamicThreshold * 2.0),
                                     64.0);
        if (newThreshold != m_dynamicThreshold)
        {
            m_dynamicThreshold = newThreshold;
            appendLogText(i18n("Dynamic threshold adjusted to %1 (stars found: %2).",
                               QString::number(m_dynamicThreshold, 'f', 2), numStarsFound));
        }
    }

    setState(ALIGN_SUCCESSFUL);
    Q_EMIT newStatus(state);
    solverIterations = 0;
    KSNotification::event(QLatin1String("AlignSuccessful"), i18n("Astrometry alignment completed successfully"),
                          KSNotification::Align);

    switch (m_CurrentGotoMode)
    {
        case GOTO_SYNC:
            executeGOTO();
            if (!m_SolveFromFile)
            {
                stopProgressAnimation();
                statusReport->setIcon(QIcon(":/icons/AlignSuccess.svg"));
                solutionTable->setItem(currentRow, 3, statusReport.release());
            }
            return;

        case GOTO_SLEW:
            if (m_SolveFromFile || m_TargetDiffTotal > static_cast<double>(alignAccuracyThreshold->value()))
            {
                if (!m_SolveFromFile && ++solverIterations == MAXIMUM_SOLVER_ITERATIONS)
                {
                    appendLogText(i18n("Maximum number of iterations reached. Solver failed."));
                    if (!m_SolveFromFile)
                    {
                        statusReport->setIcon(QIcon(":/icons/AlignFailure.svg"));
                        solutionTable->setItem(currentRow, 3, statusReport.release());
                    }
                    solverFailed();
                    return;
                }

                targetAccuracyNotMet = true;

                if (!m_SolveFromFile)
                {
                    stopProgressAnimation();
                    statusReport->setIcon(QIcon(":/icons/AlignWarning.svg"));
                    solutionTable->setItem(currentRow, 3, statusReport.release());
                }

                executeGOTO();
                return;
            }

            stopProgressAnimation();
            statusReport->setIcon(QIcon(":/icons/AlignSuccess.svg"));
            solutionTable->setItem(currentRow, 3, statusReport.release());

            appendLogText(i18n("Target is within acceptable range."));
            break;

        case GOTO_NOTHING:
            if (!m_SolveFromFile)
            {
                stopProgressAnimation();
                statusReport->setIcon(QIcon(":/icons/AlignSuccess.svg"));
                solutionTable->setItem(currentRow, 3, statusReport.release());
            }
            break;
    }

    solverFOV->setProperty("visible", true);

    if (!matchPAHStage(PAA::PAH_IDLE))
    {
        int indexUsed, healpixUsed;
        m_Solver->getSolutionHealpix(&indexUsed, &healpixUsed);
        m_PolarAlignmentAssistant->processPAHStage(orientation, ra, dec, pixscale, eastToTheRight, indexUsed, healpixUsed);
    }
    else
    {
        if (checkIfRotationRequired())
        {
            solveB->setEnabled(false);
            loadSlewB->setEnabled(false);
            return;
        }

        setState(ALIGN_COMPLETE);
        Q_EMIT newStatus(state);

        solveB->setEnabled(true);
        loadSlewB->setEnabled(true);
    }
}

void Align::solverFailed()
{
    if (Options::saveFailedAlignImages())
    {
        QDir dir;
        QDateTime now = KStarsData::Instance()->lt();
        QString path = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("align/failed");
        dir.mkpath(path);
        QString extraFilenameInfo;
        if (m_UsedScale)
            extraFilenameInfo.append(QString("_s%1u%2").arg(m_ScaleUsed, 0, 'f', 3)
                                     .arg(Options::astrometryImageScaleUnits()));
        if (m_UsedPosition)
            extraFilenameInfo.append(QString("_r%1_d%2").arg(m_RAUsed, 0, 'f', 5).arg(m_DECUsed, 0, 'f', 5));

        QString name     = "failed_align_frame_" + now.toString("yyyy-MM-dd-HH-mm-ss") + extraFilenameInfo + ".fits";
        QString filename = path + QStringLiteral("/") + name;
        if (m_ImageData)
        {
            m_ImageData->saveImage(filename);
            appendLogText(i18n("Saving failed solver image to %1", filename));
        }
    }

    if (Options::astrometryDynamicThreshold())
    {
        int numStarsFound = m_Solver->getNumStarsFound();
        auto newThreshold = std::min(std::max(1.0,
                                              numStarsFound < DYNAMIC_THRESHOLD_STARS ? m_dynamicThreshold / 2.0 : m_dynamicThreshold * 2.0),
                                     64.0);

        if (newThreshold != m_dynamicThreshold)
        {
            m_dynamicThreshold = newThreshold;
            appendLogText(i18n("Dynamic threshold adjusted to %1 (stars found: %2).",
                               QString::number(m_dynamicThreshold, 'f', 2), numStarsFound));

            if (useBlindDynamicThreshold == BLIND_IDLE)
            {
                appendLogText(i18n("Solver failed. Retrying with blind dynamic threshold."));
                useBlindDynamicThreshold = BLIND_ENGAGNED;
                setAlignTableResult(ALIGN_RESULT_FAILED);
                captureAndSolve(false);
                return;
            }
        }
    }

    if (state != ALIGN_ABORTED)
    {
        if (Options::astrometryUseImageScale() && useBlindScale == BLIND_IDLE)
        {
            appendLogText(i18n("Solver failed. Retrying without scale constraint."));
            useBlindScale = BLIND_ENGAGNED;
            setAlignTableResult(ALIGN_RESULT_FAILED);
            captureAndSolve(false);
            return;
        }

        if (Options::astrometryUsePosition() && useBlindPosition == BLIND_IDLE)
        {
            appendLogText(i18n("Solver failed. Retrying without position constraint."));
            useBlindPosition = BLIND_ENGAGNED;
            setAlignTableResult(ALIGN_RESULT_FAILED);
            captureAndSolve(false);
            return;
        }

        appendLogText(i18n("Solver Failed."));
        if(!Options::alignmentLogging())
            appendLogText(
                i18n("Please check you have sufficient stars in the image, the indicated FOV is correct, and the necessary index files are installed. Enable Alignment Logging in Setup Tab -> Logs to get detailed information on the failure."));

        KSNotification::event(QLatin1String("AlignFailed"), i18n("Astrometry alignment failed"),
                              KSNotification::Align, KSNotification::Alert);
    }

    pi->stopAnimation();
    stopB->setEnabled(false);
    solveB->setEnabled(true);
    loadSlewB->setEnabled(true);

    m_RemoteAlignTimer.stop();

    m_SolveFromFile = false;
    solverIterations = 0;
    m_CaptureErrorCounter = 0;
    m_CaptureTimeoutCounter = 0;
    m_SlewErrorCounter = 0;
    useBlindDynamicThreshold = BLIND_IDLE;

    setState(ALIGN_FAILED);
    Q_EMIT newStatus(state);

    solverFOV->setProperty("visible", false);

    setAlignTableResult(ALIGN_RESULT_FAILED);
}

uint8_t Align::getSolverDownsample(uint16_t binnedW)
{
    uint8_t downsample = Options::astrometryDownsample();

    if (!Options::astrometryAutoDownsample())
        return downsample;

    while (downsample < 8)
    {
        if (binnedW / downsample <= 1024)
            break;

        downsample += 2;
    }

    return downsample;
}

void Align::processCaptureTimeout()
{
    m_CaptureTimeoutCounter++;

    if (m_Camera == nullptr)
        return;

    if (m_DeviceRestartCounter >= 3)
    {
        m_CaptureTimeoutCounter = 0;
        m_DeviceRestartCounter = 0;
        appendLogText(i18n("Exposure timeout. Aborting..."));
        m_CaptureTimer.stop();
        abort();
        return;
    }

    if (m_CaptureTimeoutCounter > 3)
    {
        appendLogText(i18n("Exposure timeout. Too many. Restarting driver."));
        QString camera = m_Camera->getDeviceName();
        m_CaptureTimer.stop();
        Q_EMIT driverTimedout(camera);
        QTimer::singleShot(5000, this, [guard = QPointer<Align>(this), camera]()
        {
            if (!guard)
                return;
            guard->m_DeviceRestartCounter++;
            guard->reconnectDriver(camera);
        });
        return;
    }

    ISD::CameraChip *targetChip = m_Camera->getChip(useGuideHead ? ISD::CameraChip::GUIDE_CCD : ISD::CameraChip::PRIMARY_CCD);

    // If the camera is still exposing, this is a genuine capture timeout.
    // Abort the exposure and retry with the original capture timeout.
    if (targetChip->isCapturing())
    {
        appendLogText(i18n("Exposure timeout. Restarting exposure..."));
        targetChip->abortExposure();
        m_CaptureTimer.start(alignExposure->value() * 1000 + ALIGN_CAPTURE_TIMEOUT_THRESHOLD);
    }
    else
    {
        // Camera is not capturing — restart the whole capture-and-solve cycle.
        setAlignTableResult(ALIGN_RESULT_FAILED);
        if (m_resetCaptureTimeoutCounter)
        {
            m_resetCaptureTimeoutCounter = false;
            m_CaptureTimeoutCounter = 0;
        }
        captureAndSolve(false);
    }
}

void Align::reconnectDriver(const QString &camera)
{
    if (m_Camera && m_Camera->getDeviceName() == camera)
    {
        // Set state to IDLE so that checkCamera is processed
        checkCamera();

        // Reset the counters and restart capture & solve
        m_CaptureTimeoutCounter = 0;
        captureAndSolve(false);
    }
}

void Align::resetDynamicThreshold()
{
    SSolver::Parameters params;
    try
    {
        params = m_StellarSolverProfiles.at(Options::solveOptionsProfile());
    }
    catch (std::out_of_range const &)
    {
        params = m_StellarSolverProfiles[0];
    }
    auto newThreshold = params.threshold_bg_multiple;
    if (newThreshold != m_dynamicThreshold)
    {
        m_dynamicThreshold = newThreshold;
        qCInfo(KSTARS_EKOS_ALIGN) << "Dynamic threshold reset to" << QString::number(m_dynamicThreshold, 'f', 2);
    }
}

}
