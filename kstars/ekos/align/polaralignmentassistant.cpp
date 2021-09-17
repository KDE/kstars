/*  Ekos Polar Alignment Assistant Tool
    SPDX-FileCopyrightText: 2018-2021 Jasem Mutlaq
    SPDX-FileCopyrightText: 2020-2021 Hy Murveit

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "polaralignmentassistant.h"

#include "align.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksnotification.h"
#include "ksmessagebox.h"
#include "ekos/auxiliary/stellarsolverprofile.h"

// Options
#include "Options.h"


#include "QProgressIndicator.h"

#include <ekos_align_debug.h>

#define PAA_VERSION "v2.3"

namespace Ekos
{

const QMap<PolarAlignmentAssistant::PAHStage, QString> PolarAlignmentAssistant::PAHStages =
{
    {PAH_IDLE, I18N_NOOP("Idle")},
    {PAH_FIRST_CAPTURE, I18N_NOOP("First Capture"}),
    {PAH_FIND_CP, I18N_NOOP("Finding CP"}),
    {PAH_FIRST_ROTATE, I18N_NOOP("First Rotation"}),
    {PAH_SECOND_CAPTURE, I18N_NOOP("Second Capture"}),
    {PAH_SECOND_ROTATE, I18N_NOOP("Second Rotation"}),
    {PAH_THIRD_CAPTURE, I18N_NOOP("Third Capture"}),
    {PAH_STAR_SELECT, I18N_NOOP("Select Star"}),
    {PAH_PRE_REFRESH, I18N_NOOP("Select Refresh"}),
    {PAH_REFRESH, I18N_NOOP("Refreshing"}),
    {PAH_POST_REFRESH, I18N_NOOP("Refresh Complete"}),
    {PAH_ERROR, I18N_NOOP("Error")},
};

PolarAlignmentAssistant::PolarAlignmentAssistant(Align *parent, AlignView *view) : QWidget(parent)
{
    setupUi(this);

    m_AlignInstance = parent;
    alignView = view;

    connect(PAHSlewRateCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [&](int index)
    {
        Options::setPAHMountSpeedIndex(index);
    });

    PAHUpdatedErrorLine->setVisible(Options::pAHRefreshUpdateError());
    PAHRefreshUpdateError->setChecked(Options::pAHRefreshUpdateError());
    connect(PAHRefreshUpdateError, &QCheckBox::toggled, [this](bool toggled)
    {
        Options::setPAHRefreshUpdateError(toggled);
        PAHUpdatedErrorLine->setVisible(toggled);
    });

    // PAH Connections
    PAHWidgets->setCurrentWidget(PAHIntroPage);
    connect(this, &PolarAlignmentAssistant::PAHEnabled, [&](bool enabled)
    {
        PAHStartB->setEnabled(enabled);
        directionLabel->setEnabled(enabled);
        PAHDirectionCombo->setEnabled(enabled);
        PAHRotationSpin->setEnabled(enabled);
        PAHSlewRateCombo->setEnabled(enabled);
        PAHManual->setEnabled(enabled);
    });
    connect(PAHStartB, &QPushButton::clicked, this, &Ekos::PolarAlignmentAssistant::startPAHProcess);
    // PAH StopB is just a shortcut for the regular stop
    connect(PAHStopB, &QPushButton::clicked, this, &PolarAlignmentAssistant::stopPAHProcess);
    connect(PAHCorrectionsNextB, &QPushButton::clicked, this,
            &Ekos::PolarAlignmentAssistant::setPAHCorrectionSelectionComplete);
    connect(PAHRefreshB, &QPushButton::clicked, this, &Ekos::PolarAlignmentAssistant::startPAHRefreshProcess);
    connect(PAHDoneB, &QPushButton::clicked, this, &Ekos::PolarAlignmentAssistant::setPAHRefreshComplete);
    // done button for manual slewing during polar alignment:
    connect(PAHManualDone, &QPushButton::clicked, this, &Ekos::PolarAlignmentAssistant::setPAHSlewDone);

    hemisphere = KStarsData::Instance()->geo()->lat()->Degrees() > 0 ? NORTH_HEMISPHERE : SOUTH_HEMISPHERE;

}

PolarAlignmentAssistant::~PolarAlignmentAssistant()
{

}

void PolarAlignmentAssistant::syncMountSpeed()
{
    PAHSlewRateCombo->blockSignals(true);
    PAHSlewRateCombo->clear();
    PAHSlewRateCombo->addItems(m_CurrentTelescope->slewRates());
    const uint16_t configMountSpeed = Options::pAHMountSpeedIndex();
    if (configMountSpeed < PAHSlewRateCombo->count())
        PAHSlewRateCombo->setCurrentIndex(configMountSpeed);
    else
    {
        int currentSlewRateIndex = m_CurrentTelescope->getSlewRate();
        if (currentSlewRateIndex >= 0)
        {
            PAHSlewRateCombo->setCurrentIndex(currentSlewRateIndex);
            Options::setPAHMountSpeedIndex(currentSlewRateIndex);
        }
    }
    PAHSlewRateCombo->blockSignals(false);
}

void PolarAlignmentAssistant::setEnabled(bool enabled)
{
    QWidget::setEnabled(enabled);

    emit PAHEnabled(enabled);
    if (enabled)
    {
        PAHWidgets->setToolTip(QString());
        FOVDisabledLabel->hide();
    }
    else
    {
        PAHWidgets->setToolTip(i18n(
                                   "<p>Polar Alignment Helper tool requires the following:</p><p>1. German Equatorial Mount</p><p>2. FOV &gt;"
                                   " 0.5 degrees</p><p>For small FOVs, use the Legacy Polar Alignment Tool.</p>"));
        FOVDisabledLabel->show();
    }

}

void PolarAlignmentAssistant::syncStage()
{
    if (m_PAHStage == PAH_FIRST_CAPTURE)
        PAHWidgets->setCurrentWidget(PAHFirstCapturePage);
    else if (m_PAHStage == PAH_SECOND_CAPTURE)
        PAHWidgets->setCurrentWidget(PAHSecondCapturePage);
    else if (m_PAHStage == PAH_THIRD_CAPTURE)
        PAHWidgets->setCurrentWidget(PAHThirdCapturePage);

}

bool PolarAlignmentAssistant::detectStarsPAHRefresh(QList<Edge> *stars, int num, int x, int y, int *starIndex)
{
    stars->clear();
    *starIndex = -1;

    // Use the solver settings from the align tab for for "polar-align refresh" star detection.
    QVariantMap settings;
    settings["optionsProfileIndex"] = Options::solveOptionsProfile();
    settings["optionsProfileGroup"] = static_cast<int>(Ekos::AlignProfiles);
    m_ImageData->setSourceExtractorSettings(settings);

    QElapsedTimer timer;
    m_ImageData->findStars(ALGORITHM_SEP).waitForFinished();

    QString debugString = QString("PAA Refresh: Detected %1 stars (%2s)")
                          .arg(m_ImageData->getStarCenters().size()).arg(timer.elapsed() / 1000.0, 5, 'f', 3);
    qCDebug(KSTARS_EKOS_ALIGN) << debugString;

    QList<Edge *> detectedStars = m_ImageData->getStarCenters();
    // Let's sort detectedStars by flux, starting with widest
    std::sort(detectedStars.begin(), detectedStars.end(), [](const Edge * edge1, const Edge * edge2) -> bool { return edge1->sum > edge2->sum;});

    // Find the closest star to the x,y position, which is where the user clicked on the alignView.
    double bestDist = 1e9;
    int bestIndex = -1;
    for (int i = 0; i < detectedStars.size(); i++)
    {
        double dx = detectedStars[i]->x - x;
        double dy = detectedStars[i]->y - y;
        double dist = dx * dx + dy * dy;
        if (dist < bestDist)
        {
            bestDist = dist;
            bestIndex = i;
        }
    }

    int starCount = qMin(num, detectedStars.count());
    for (int i = 0; i < starCount; i++)
        stars->append(*(detectedStars[i]));
    if (bestIndex >= starCount)
    {
        // If we found the star, but requested 'num' stars, and the user's star
        // is lower down in the list, add it and return num+1 stars.
        stars->append(*(detectedStars[bestIndex]));
        *starIndex = starCount;
    }
    else
    {
        *starIndex = bestIndex;
    }
    debugString = QString("PAA Refresh: User's star(%1,%2) is index %3").arg(x).arg(y).arg(*starIndex);
    qCDebug(KSTARS_EKOS_ALIGN) << debugString;

    detectedStars.clear();

    return stars->count();
}

void PolarAlignmentAssistant::processPAHRefresh()
{
    alignView->setStarCircle();
    PAHUpdatedErrorTotal->clear();
    PAHUpdatedErrorAlt->clear();
    PAHUpdatedErrorAz->clear();
    QString debugString;
    // Always run on the initial iteration to setup the user's star,
    // so that if it is enabled later the star could be tracked.
    // Flaw here is that if enough stars are not detected, iteration is not incremented,
    // so it may repeat.
    if (Options::pAHRefreshUpdateError() || (refreshIteration == 0))
    {
        constexpr int MIN_PAH_REFRESH_STARS = 10;

        QList<Edge> stars;
        // Index of user's star in the detected stars list. In the first iteration
        // the stars haven't moved and we can just use the location of the click.
        // Later we'll need to find the star with starCorrespondence.
        int clickedStarIndex;
        detectStarsPAHRefresh(&stars, 100, correctionFrom.x(), correctionFrom.y(), &clickedStarIndex);
        if (clickedStarIndex < 0)
        {
            debugString = QString("PAA Refresh(%1): Didn't find the clicked star near %2,%3")
                          .arg(refreshIteration).arg(correctionFrom.x()).arg(correctionFrom.y());
            qCDebug(KSTARS_EKOS_ALIGN) << debugString;

            emit newAlignTableResult(Align::ALIGN_RESULT_FAILED);
            emit captureAndSolve();
            return;
        }

        debugString = QString("PAA Refresh(%1): Refresh star(%2,%3) is index %4 with offset %5 %6")
                      .arg(refreshIteration + 1).arg(correctionFrom.x(), 4, 'f', 0)
                      .arg(correctionFrom.y(), 4, 'f', 0).arg(clickedStarIndex)
                      .arg(stars[clickedStarIndex].x - correctionFrom.x(), 4, 'f', 0)
                      .arg(stars[clickedStarIndex].y - correctionFrom.y(), 4, 'f', 0);
        qCDebug(KSTARS_EKOS_ALIGN) << debugString;

        if (stars.size() > MIN_PAH_REFRESH_STARS)
        {
            int dx = 0;
            int dy = 0;
            int starIndex = -1;

            if (refreshIteration++ == 0)
            {
                // First iteration. Setup starCorrespondence so we can find the user's star.
                // clickedStarIndex should be the index of a detected star near where the user clicked.
                starCorrespondencePAH.initialize(stars, clickedStarIndex);
                if (clickedStarIndex >= 0)
                {
                    setupCorrectionGraphics(QPointF(stars[clickedStarIndex].x, stars[clickedStarIndex].y));
                    emit newCorrectionVector(QLineF(correctionFrom, correctionTo));
                    emit newFrame(alignView);
                }
            }
            else
            {
                // Or, in other iterations find the movement of the "user's star".
                // The 0.40 means it's OK if star correspondence only finds 40% of the
                // reference stars (as we'd have more issues near the image edge otherwise).
                QVector<int> starMap;
                starCorrespondencePAH.find(stars, 200.0, &starMap, false, 0.40);

                // Go through the starMap, and find the user's star, and compare its position
                // to its initial position.
                for (int i = 0; i < starMap.size(); ++i)
                {
                    if (starMap[i] == starCorrespondencePAH.guideStar())
                    {
                        dx = stars[i].x - correctionFrom.x();
                        dy = stars[i].y - correctionFrom.y();
                        starIndex = i;
                        break;
                    }
                }
                if (starIndex == -1)
                {
                    bool allOnes = true;
                    for (int i = 0; i < starMap.size(); ++i)
                    {
                        if (starMap[i] != -1)
                            allOnes = false;
                    }
                    debugString = QString("PAA Refresh(%1): starMap %2").arg(refreshIteration).arg(allOnes ? "ALL -1's" : "not all -1's");
                    qCDebug(KSTARS_EKOS_ALIGN) << debugString;
                }
            }

            if (starIndex >= 0)
            {
                // Annotate the user's star on the alignview.
                alignView->setStarCircle(QPointF(stars[starIndex].x, stars[starIndex].y));
                debugString = QString("PAA Refresh(%1): User's star is now at %2,%3, with movement = %4,%5").arg(refreshIteration)
                              .arg(stars[starIndex].x, 4, 'f', 0).arg(stars[starIndex].y, 4, 'f', 0).arg(dx).arg(dy);
                qCDebug(KSTARS_EKOS_ALIGN) << debugString;

                double azE, altE;
                if (polarAlign.pixelError(alignView->keptImage(), QPointF(stars[starIndex].x, stars[starIndex].y),
                                          correctionTo, &azE, &altE))
                {
                    const double errDegrees = hypot(azE, altE);
                    dms totalError(errDegrees), azError(azE), altError(altE);
                    PAHUpdatedErrorTotal->setText(totalError.toDMSString());
                    PAHUpdatedErrorAlt->setText(altError.toDMSString());
                    PAHUpdatedErrorAz->setText(azError.toDMSString());
                    constexpr double oneArcMin = 1.0 / 60.0;
                    PAHUpdatedErrorTotal->setStyleSheet(
                        errDegrees < oneArcMin ? "color:green" : (errDegrees < 2 * oneArcMin ? "color:yellow" : "color:red"));
                    PAHUpdatedErrorAlt->setStyleSheet(
                        fabs(altE) < oneArcMin ? "color:green" : (fabs(altE) < 2 * oneArcMin ? "color:yellow" : "color:red"));
                    PAHUpdatedErrorAz->setStyleSheet(
                        fabs(azE) < oneArcMin ? "color:green" : (fabs(azE) < 2 * oneArcMin ? "color:yellow" : "color:red"));

                    debugString = QString("PAA Refresh(%1): %2,%3 --> %4,%5 @ %6,%7. Corrected az: %8 (%9) alt: %10 (%11) total: %12 (%13)")
                                  .arg(refreshIteration).arg(correctionFrom.x(), 4, 'f', 0).arg(correctionFrom.y(), 4, 'f', 0)
                                  .arg(correctionTo.x(), 4, 'f', 0).arg(correctionTo.y(), 4, 'f', 0)
                                  .arg(stars[starIndex].x, 4, 'f', 0).arg(stars[starIndex].y, 4, 'f', 0)
                                  .arg(azError.toDMSString()).arg(azE, 5, 'f', 3)
                                  .arg(altError.toDMSString()).arg(altE, 6, 'f', 3)
                                  .arg(totalError.toDMSString()).arg(hypot(azE, altE), 6, 'f', 3);
                    qCDebug(KSTARS_EKOS_ALIGN) << debugString;
                }
                else
                {
                    debugString = QString("PAA Refresh(%1): pixelError failed to estimate the remaining correction").arg(refreshIteration);
                    qCDebug(KSTARS_EKOS_ALIGN) << debugString;
                }
            }
            else
            {
                if (refreshIteration > 1)
                {
                    debugString = QString("PAA Refresh(%1): Didn't find the user's star").arg(refreshIteration);
                    qCDebug(KSTARS_EKOS_ALIGN) << debugString;
                }
            }
        }
        else
        {
            debugString = QString("PAA Refresh(%1): Too few stars detected (%2)").arg(refreshIteration).arg(stars.size());
            qCDebug(KSTARS_EKOS_ALIGN) << debugString;
        }
    }
    // Finally start the next capture
    emit captureAndSolve();
}

bool PolarAlignmentAssistant::processSolverFailure()
{
    if ((m_PAHStage == PAH_FIRST_CAPTURE || m_PAHStage == PAH_SECOND_CAPTURE || m_PAHStage == PAH_THIRD_CAPTURE)
            && ++m_PAHRetrySolveCounter < 4)
    {
        emit captureAndSolve();
        return true;
    }

    if (m_PAHStage != PAH_IDLE)
    {
        emit newLog(i18n("PAA: Stopping, solver failed too many times."));
        stopPAHProcess();
    }

    return false;
}

void PolarAlignmentAssistant::setPAHStage(PAHStage stage)
{
    if (stage != m_PAHStage)
    {
        m_PAHStage = stage;
        emit newPAHStage(m_PAHStage);
    }
}

void PolarAlignmentAssistant::processMountRotation(const dms &ra, double settleDuration)
{
    double deltaAngle = fabs(ra.deltaAngle(targetPAH.ra()).Degrees());

    if (m_PAHStage == PAH_FIRST_ROTATE)
    {
        // only wait for telescope to slew to new position if manual slewing is switched off
        if(!PAHManual->isChecked())
        {
            qCDebug(KSTARS_EKOS_ALIGN) << "First mount rotation remaining degrees:" << deltaAngle;
            if (deltaAngle <= PAH_ROTATION_THRESHOLD)
            {
                m_CurrentTelescope->StopWE();
                emit newLog(i18n("Mount first rotation is complete."));

                m_PAHStage = PAH_SECOND_CAPTURE;
                emit newPAHStage(m_PAHStage);

                PAHWidgets->setCurrentWidget(PAHSecondCapturePage);
                emit newPAHMessage(secondCaptureText->text());

                if (settleDuration >= 0)
                {
                    PAHWidgets->setCurrentWidget(PAHFirstSettlePage);
                    emit newLog(i18n("Settling..."));
                    QTimer::singleShot(settleDuration, [this]()
                    {
                        PAHWidgets->setCurrentWidget(PAHSecondCapturePage);
                        emit newPAHMessage(secondCaptureText->text());
                    });
                }

                emit settleStarted(settleDuration);
            }
            // If for some reason we didn't stop, let's stop if we get too far
            else if (deltaAngle > PAHRotationSpin->value() * 1.25)
            {
                m_CurrentTelescope->Abort();
                emit newLog(i18n("Mount aborted. Please restart the process and reduce the speed."));
                stopPAHProcess();
            }
            return;
        } // endif not manual slew
    }
    else if (m_PAHStage == PAH_SECOND_ROTATE)
    {
        // only wait for telescope to slew to new position if manual slewing is switched off
        if(!PAHManual->isChecked())
        {

            qCDebug(KSTARS_EKOS_ALIGN) << "Second mount rotation remaining degrees:" << deltaAngle;
            if (deltaAngle <= PAH_ROTATION_THRESHOLD)
            {
                m_CurrentTelescope->StopWE();
                emit newLog(i18n("Mount second rotation is complete."));

                m_PAHStage = PAH_THIRD_CAPTURE;
                emit newPAHStage(m_PAHStage);


                PAHWidgets->setCurrentWidget(PAHThirdCapturePage);
                emit newPAHMessage(thirdCaptureText->text());

                if (settleDuration >= 0)
                {
                    PAHWidgets->setCurrentWidget(PAHSecondSettlePage);
                    emit newLog(i18n("Settling..."));
                    QTimer::singleShot(settleDuration, [this]()
                    {
                        PAHWidgets->setCurrentWidget(PAHThirdCapturePage);
                        emit newPAHMessage(thirdCaptureText->text());
                    });
                }

                emit settleStarted(settleDuration);
            }
            // If for some reason we didn't stop, let's stop if we get too far
            else if (deltaAngle > PAHRotationSpin->value() * 1.25)
            {
                m_CurrentTelescope->Abort();
                emit newLog(i18n("Mount aborted. Please restart the process and reduce the speed."));
                stopPAHProcess();
            }
            return;
        } // endif not manual slew
    }
}

bool PolarAlignmentAssistant::checkPAHForMeridianCrossing()
{
    // Make sure using -180 to 180 for hourAngle and DEC. (Yes dec should be between -90 and 90).
    double hourAngle = m_CurrentTelescope->hourAngle().Degrees();
    while (hourAngle < -180)
        hourAngle += 360;
    while (hourAngle > 180)
        hourAngle -= 360;
    double ra = 0, dec = 0;
    m_CurrentTelescope->getEqCoords(&ra, &dec);
    while (dec < -180)
        dec += 360;
    while (dec > 180)
        dec -= 360;

    // Don't do this check within 2 degrees of the poles.
    bool nearThePole = fabs(dec) > 88;
    if (nearThePole)
        return false;

    double degreesPerSlew = PAHRotationSpin->value();
    bool closeToMeridian = fabs(hourAngle) < 2.0 * degreesPerSlew;
    bool goingWest = PAHDirectionCombo->currentIndex() == 0;

    // If the pier is on the east side (pointing west) and will slew west and is within 2 slews of the HA=0,
    // or on the west side (pointing east) and will slew east, and is within 2 slews of HA=0
    // then warn and give the user a chance to cancel.
    bool wouldCrossMeridian =
        ((m_CurrentTelescope->pierSide() == ISD::Telescope::PIER_EAST && !goingWest && closeToMeridian) ||
         (m_CurrentTelescope->pierSide() == ISD::Telescope::PIER_WEST && goingWest && closeToMeridian) ||
         (m_CurrentTelescope->pierSide() == ISD::Telescope::PIER_UNKNOWN && closeToMeridian));

    return wouldCrossMeridian;
}

void PolarAlignmentAssistant::startPAHProcess()
{
    qCInfo(KSTARS_EKOS_ALIGN) << QString("Starting Polar Alignment Assistant process %1 ...").arg(PAA_VERSION);

    auto executePAH = [ this ]()
    {
        m_PAHStage = PAH_FIRST_CAPTURE;
        emit newPAHStage(m_PAHStage);

        if (Options::limitedResourcesMode())
            emit newLog(i18n("Warning: Equatorial Grid Lines will not be drawn due to limited resources mode."));

        if (m_CurrentTelescope->hasAlignmentModel())
        {
            emit newLog(i18n("Clearing mount Alignment Model..."));
            m_CurrentTelescope->clearAlignmentModel();
        }

        // Unpark
        m_CurrentTelescope->UnPark();

        // Set tracking ON if not already
        if (m_CurrentTelescope->canControlTrack() && m_CurrentTelescope->isTracking() == false)
            m_CurrentTelescope->setTrackEnabled(true);

        PAHStartB->setEnabled(false);
        PAHStopB->setEnabled(true);

        PAHUpdatedErrorTotal->clear();
        PAHUpdatedErrorAlt->clear();
        PAHUpdatedErrorAz->clear();
        PAHOrigErrorTotal->clear();
        PAHOrigErrorAlt->clear();
        PAHOrigErrorAz->clear();

        PAHWidgets->setCurrentWidget(PAHFirstCapturePage);
        emit newPAHMessage(firstCaptureText->text());

        m_PAHRetrySolveCounter = 0;
        emit captureAndSolve();
    };

    // Right off the bat, check if this alignment might cause a pier crash.
    // If we're crossing the meridian, warn unless within 5-degrees from the pole.
    if (checkPAHForMeridianCrossing())
    {
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, executePAH]()
        {
            KSMessageBox::Instance()->disconnect(this);
            executePAH();
        });

        emit newLog(i18n("Warning, This could cause the telescope to cross the meridian. Check your direction."));
    }
    else
        executePAH();
}

void PolarAlignmentAssistant::stopPAHProcess()
{
    if (m_PAHStage == PAH_IDLE)
        return;

    qCInfo(KSTARS_EKOS_ALIGN) << "Stopping Polar Alignment Assistant process...";

    // Only display dialog if user explicitly restarts
    if ((static_cast<QPushButton *>(sender()) == PAHStopB) && KMessageBox::questionYesNo(KStars::Instance(),
            i18n("Are you sure you want to stop the polar alignment process?"),
            i18n("Polar Alignment Assistant"), KStandardGuiItem::yes(), KStandardGuiItem::no(),
            "restart_PAA_process_dialog") == KMessageBox::No)
        return;

    if (m_CurrentTelescope && m_CurrentTelescope->isInMotion())
        m_CurrentTelescope->Abort();

    m_PAHStage = PAH_IDLE;
    emit newPAHStage(m_PAHStage);

    PAHStartB->setEnabled(true);
    PAHStopB->setEnabled(false);
    PAHRefreshB->setEnabled(true);
    PAHWidgets->setCurrentWidget(PAHIntroPage);
    emit newPAHMessage(introText->text());

    alignView->reset();
    alignView->setRefreshEnabled(false);

    emit newFrame(alignView);
    disconnect(alignView, &AlignView::trackingStarSelected, this, &Ekos::PolarAlignmentAssistant::setPAHCorrectionOffset);
    disconnect(alignView, &AlignView::newCorrectionVector, this, &Ekos::PolarAlignmentAssistant::newCorrectionVector);

    if (Options::pAHAutoPark())
    {
        m_CurrentTelescope->Park();
        emit newLog(i18n("Parking the mount..."));
    }
}

void PolarAlignmentAssistant::rotatePAH()
{
    double TargetDiffRA = PAHRotationSpin->value();
    bool westMeridian = PAHDirectionCombo->currentIndex() == 0;

    // West
    if (westMeridian)
        TargetDiffRA *= -1;
    // East
    else
        TargetDiffRA *= 1;

    // JM 2018-05-03: Hemispheres shouldn't affect rotation direction in RA

    // if Manual slewing is selected, don't move the mount
    if (PAHManual->isChecked())
    {
        return;
    }

    const SkyPoint telescopeCoord = m_CurrentTelescope->currentCoordinates();

    // TargetDiffRA is in degrees
    dms newTelescopeRA = (telescopeCoord.ra() + dms(TargetDiffRA)).reduce();

    targetPAH.setRA(newTelescopeRA);
    targetPAH.setDec(telescopeCoord.dec());

    //m_CurrentTelescope->Slew(&targetPAH);
    // Set Selected Speed
    if (PAHSlewRateCombo->currentIndex() >= 0)
        m_CurrentTelescope->setSlewRate(PAHSlewRateCombo->currentIndex());
    // Go to direction
    m_CurrentTelescope->MoveWE(westMeridian ? ISD::Telescope::MOTION_WEST : ISD::Telescope::MOTION_EAST,
                               ISD::Telescope::MOTION_START);

    emit newLog(i18n("Please wait until mount completes rotating to RA (%1) DE (%2)", targetPAH.ra().toHMSString(),
                     targetPAH.dec().toDMSString()));
}

void PolarAlignmentAssistant::setupCorrectionGraphics(const QPointF &pixel)
{
    // We use the previously stored image (the 3rd PAA image)
    // so we can continue to estimage the correction even after
    // capturing new images during the refresh stage.
    const QSharedPointer<FITSData> &imageData = alignView->keptImage();

    // Just the altitude correction
    if (!polarAlign.findCorrectedPixel(imageData, pixel, &correctionAltTo, true))
    {
        qCInfo(KSTARS_EKOS_ALIGN) << QString(i18n("PAA: Failed to findCorrectedPixel."));
        return;
    }
    // The whole correction.
    if (!polarAlign.findCorrectedPixel(imageData, pixel, &correctionTo))
    {
        qCInfo(KSTARS_EKOS_ALIGN) << QString(i18n("PAA: Failed to findCorrectedPixel."));
        return;
    }
    QString debugString = QString("PAA: Correction: %1,%2 --> %3,%4 (alt only %5,%6)")
                          .arg(pixel.x(), 4, 'f', 0).arg(pixel.y(), 4, 'f', 0)
                          .arg(correctionTo.x(), 4, 'f', 0).arg(correctionTo.y(), 4, 'f', 0)
                          .arg(correctionAltTo.x(), 4, 'f', 0).arg(correctionAltTo.y(), 4, 'f', 0);
    qCDebug(KSTARS_EKOS_ALIGN) << debugString;
    correctionFrom = pixel;
    alignView->setCorrectionParams(correctionFrom, correctionTo, correctionAltTo);

    return;
}

bool PolarAlignmentAssistant::calculatePAHError()
{
    // Hold on to the imageData so we can use it during the refresh phase.
    alignView->holdOnToImage();

    if (!polarAlign.findAxis())
    {
        emit newLog(i18n("PAA: Failed to find RA Axis center."));
        stopPAHProcess();
        return false;
    }

    double azimuthError, altitudeError;
    polarAlign.calculateAzAltError(&azimuthError, &altitudeError);
    dms polarError(hypot(altitudeError, azimuthError));
    dms azError(azimuthError), altError(altitudeError);

    if (alignView->isEQGridShown() == false && !Options::limitedResourcesMode())
        alignView->toggleEQGrid();

    QString msg = QString("%1. Azimuth: %2  Altitude: %3")
                  .arg(polarError.toDMSString()).arg(azError.toDMSString())
                  .arg(altError.toDMSString());
    emit newLog(QString("Polar Alignment Error: %1").arg(msg));
    PAHErrorLabel->setText(msg);

    // These are viewed during the refresh phase.
    PAHOrigErrorTotal->setText(polarError.toDMSString());
    PAHOrigErrorAlt->setText(altError.toDMSString());
    PAHOrigErrorAz->setText(azError.toDMSString());

    setupCorrectionGraphics(QPointF(m_ImageData->width() / 2, m_ImageData->height() / 2));

    // Find Celestial pole location and mount's RA axis
    SkyPoint CP(0, (hemisphere == NORTH_HEMISPHERE) ? 90 : -90);
    QPointF imagePoint, celestialPolePoint;
    m_ImageData->wcsToPixel(CP, celestialPolePoint, imagePoint);
    if (m_ImageData->contains(celestialPolePoint))
    {
        alignView->setCelestialPole(celestialPolePoint);
        QPointF raAxis;
        if (polarAlign.findCorrectedPixel(m_ImageData, celestialPolePoint, &raAxis))
            alignView->setRaAxis(raAxis);
    }

    connect(alignView, &AlignView::trackingStarSelected, this, &Ekos::PolarAlignmentAssistant::setPAHCorrectionOffset);
    emit polarResultUpdated(QLineF(correctionFrom, correctionTo), polarError.Degrees(), azError.Degrees(), altError.Degrees());

    connect(alignView, &AlignView::newCorrectionVector, this, &Ekos::PolarAlignmentAssistant::newCorrectionVector,
            Qt::UniqueConnection);
    syncCorrectionVector();
    emit newFrame(alignView);

    return true;
}

void PolarAlignmentAssistant::syncCorrectionVector()
{
    emit newCorrectionVector(QLineF(correctionFrom, correctionTo));
    alignView->setCorrectionParams(correctionFrom, correctionTo, correctionAltTo);
}

void PolarAlignmentAssistant::setPAHCorrectionOffsetPercentage(double dx, double dy)
{
    double x = dx * alignView->zoomedWidth();
    double y = dy * alignView->zoomedHeight();
    setPAHCorrectionOffset(static_cast<int>(round(x)), static_cast<int>(round(y)));
}

void PolarAlignmentAssistant::setPAHCorrectionOffset(int x, int y)
{
    if (m_PAHStage == PAH_REFRESH || m_PAHStage == PAH_PRE_REFRESH)
    {
        emit newLog(i18n("Polar-alignment star cannot be updated during refresh phase as it might affect error measurements."));
    }
    else
    {
        setupCorrectionGraphics(QPointF(x, y));
        emit newCorrectionVector(QLineF(correctionFrom, correctionTo));
        emit newFrame(alignView);
    }
}

void PolarAlignmentAssistant::setPAHCorrectionSelectionComplete()
{
    m_PAHStage = PAH_PRE_REFRESH;
    emit newPAHStage(m_PAHStage);
    PAHWidgets->setCurrentWidget(PAHRefreshPage);
    emit newPAHMessage(refreshText->text());
}

void PolarAlignmentAssistant::setPAHSlewDone()
{
    emit newPAHMessage("Manual slew done.");
    switch(m_PAHStage)
    {
        case PAH_FIRST_ROTATE :
            m_PAHStage = PAH_SECOND_CAPTURE;
            emit newPAHStage(m_PAHStage);
            PAHWidgets->setCurrentWidget(PAHSecondCapturePage);
            emit newLog(i18n("First manual rotation done."));
            break;
        case PAH_SECOND_ROTATE :
            m_PAHStage = PAH_THIRD_CAPTURE;
            emit newPAHStage(m_PAHStage);
            PAHWidgets->setCurrentWidget(PAHThirdCapturePage);
            emit newLog(i18n("Second manual rotation done."));
            break;
        default :
            return; // no other stage should be able to trigger this event
    }
}



void PolarAlignmentAssistant::startPAHRefreshProcess()
{
    qCInfo(KSTARS_EKOS_ALIGN) << "Starting Polar Alignment Assistant refreshing...";

    refreshIteration = 0;

    m_PAHStage = PAH_REFRESH;
    emit newPAHStage(m_PAHStage);

    PAHRefreshB->setEnabled(false);

    // Hide EQ Grids if shown
    if (alignView->isEQGridShown())
        alignView->toggleEQGrid();

    alignView->setRefreshEnabled(true);

    Options::setAstrometrySolverWCS(false);
    Options::setAutoWCS(false);

    // We for refresh, just capture really
    emit captureAndSolve();
}

void PolarAlignmentAssistant::setPAHRefreshComplete()
{
    refreshIteration = 0;
    m_PAHStage = PAH_POST_REFRESH;
    emit newPAHStage(m_PAHStage);
    stopPAHProcess();
}

void PolarAlignmentAssistant::processPAHStage(double orientation, double ra, double dec, double pixscale,
        bool eastToTheRight)
{
    if (m_PAHStage == PAH_FIND_CP)
    {
        emit newLog(
            i18n("Mount is synced to celestial pole. You can now continue Polar Alignment Assistant procedure."));
        m_PAHStage = PAH_FIRST_CAPTURE;
        emit newPAHStage(m_PAHStage);
        return;
    }

    if (m_PAHStage == PAH_FIRST_CAPTURE || m_PAHStage == PAH_SECOND_CAPTURE || m_PAHStage == PAH_THIRD_CAPTURE)
    {
        bool doWcs = (m_PAHStage == PAH_THIRD_CAPTURE) || !Options::limitedResourcesMode();
        if (doWcs)
        {
            emit newLog(i18n("Please wait while WCS data is processed..."));
            PAHWidgets->setCurrentWidget(
                m_PAHStage == PAH_FIRST_CAPTURE
                ? PAHFirstWcsPage
                : (m_PAHStage == PAH_SECOND_CAPTURE ? PAHSecondWcsPage
                   : PAHThirdWcsPage));
        }
        connect(alignView, &AlignView::wcsToggled, this, &Ekos::PolarAlignmentAssistant::setWCSToggled, Qt::UniqueConnection);
        alignView->injectWCS(orientation, ra, dec, pixscale, eastToTheRight, doWcs);
        return;
    }
}

QJsonObject PolarAlignmentAssistant::getPAHSettings() const
{
    QJsonObject settings;

    settings.insert("mountDirection", PAHDirectionCombo->currentIndex());
    settings.insert("mountSpeed", PAHSlewRateCombo->currentIndex());
    settings.insert("mountRotation", PAHRotationSpin->value());
    settings.insert("refresh", PAHExposure->value());
    settings.insert("manualslew", PAHManual->isChecked());

    return settings;
}

void PolarAlignmentAssistant::setPAHSettings(const QJsonObject &settings)
{
    PAHDirectionCombo->setCurrentIndex(settings["mountDirection"].toInt(0));
    PAHRotationSpin->setValue(settings["mountRotation"].toInt(30));
    PAHExposure->setValue(settings["refresh"].toDouble(1));
    if (settings.contains("mountSpeed"))
        PAHSlewRateCombo->setCurrentIndex(settings["mountSpeed"].toInt(0));
    PAHManual->setChecked(settings["manualslew"].toBool(false));
}

void PolarAlignmentAssistant::setWCSToggled(bool result)
{
    emit newLog(i18n("WCS data processing is complete."));

    disconnect(alignView, &AlignView::wcsToggled, this, &Ekos::PolarAlignmentAssistant::setWCSToggled);

    if (m_PAHStage == PAH_FIRST_CAPTURE)
    {
        // We need WCS to be synced first
        if (result == false && m_AlignInstance->wcsSynced() == true)
        {
            emit newLog(i18n("WCS info is now valid. Capturing next frame..."));
            emit captureAndSolve();
            return;
        }

        polarAlign.reset();
        polarAlign.addPoint(m_ImageData);

        m_PAHStage = PAH_FIRST_ROTATE;
        emit newPAHStage(m_PAHStage);

        if (PAHManual->isChecked())
        {
            QString msg = QString("Please rotate your mount about %1 deg in RA")
                          .arg(PAHRotationSpin->value());
            manualRotateText->setText(msg);
            emit newLog(msg);
            PAHWidgets->setCurrentWidget(PAHManualRotatePage);
            emit newPAHMessage(manualRotateText->text());
        }
        else
        {
            PAHWidgets->setCurrentWidget(PAHFirstRotatePage);
            emit newPAHMessage(firstRotateText->text());
        }

        rotatePAH();
    }
    else if (m_PAHStage == PAH_SECOND_CAPTURE)
    {
        m_PAHStage = PAH_SECOND_ROTATE;
        emit newPAHStage(m_PAHStage);

        if (PAHManual->isChecked())
        {
            QString msg = QString("Please rotate your mount about %1 deg in RA")
                          .arg(PAHRotationSpin->value());
            manualRotateText->setText(msg);
            emit newLog(msg);
            PAHWidgets->setCurrentWidget(PAHManualRotatePage);
            emit newPAHMessage(manualRotateText->text());
        }
        else
        {
            PAHWidgets->setCurrentWidget(PAHSecondRotatePage);
            emit newPAHMessage(secondRotateText->text());
        }

        polarAlign.addPoint(m_ImageData);

        rotatePAH();
    }
    else if (m_PAHStage == PAH_THIRD_CAPTURE)
    {
        // Critical error
        if (result == false)
        {
            emit newLog(i18n("Failed to process World Coordinate System: %1. Try again.", m_ImageData->getLastError()));
            return;
        }

        polarAlign.addPoint(m_ImageData);

        // We have 3 points which uniquely defines a circle with its center representing the RA Axis
        // We have celestial pole location. So correction vector is just the vector between these two points
        if (calculatePAHError())
        {
            m_PAHStage = PAH_STAR_SELECT;
            emit newPAHStage(m_PAHStage);
            PAHWidgets->setCurrentWidget(PAHCorrectionPage);
            emit newPAHMessage(correctionText->text());
        }
    }
}

void PolarAlignmentAssistant::setMountStatus(ISD::Telescope::Status newState)
{
    switch (newState)
    {
        case ISD::Telescope::MOUNT_PARKING:
        case ISD::Telescope::MOUNT_SLEWING:
        case ISD::Telescope::MOUNT_MOVING:
            PAHStartB->setEnabled(false);
            break;

        default:
            if (m_PAHStage == PAH_IDLE)
                PAHStartB->setEnabled(true);
            break;
    }
}

QString PolarAlignmentAssistant::getPAHMessage() const
{
    switch (m_PAHStage)
    {
        case PAH_IDLE:
        case PAH_FIND_CP:
            return introText->text();
        case PAH_FIRST_CAPTURE:
            return firstCaptureText->text();
        case PAH_FIRST_ROTATE:
            return firstRotateText->text();
        case PAH_SECOND_CAPTURE:
            return secondCaptureText->text();
        case PAH_SECOND_ROTATE:
            return secondRotateText->text();
        case PAH_THIRD_CAPTURE:
            return thirdCaptureText->text();
        case PAH_STAR_SELECT:
            return correctionText->text();
        case PAH_PRE_REFRESH:
        case PAH_POST_REFRESH:
        case PAH_REFRESH:
            return refreshText->text();
        case PAH_ERROR:
            return PAHErrorDescriptionLabel->text();
    }

    return QString();
}


}
