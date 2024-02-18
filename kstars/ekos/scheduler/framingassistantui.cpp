/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <indicom.h>

#include "framingassistantui.h"
#include "ui_framingassistant.h"
#include "mosaiccomponent.h"
#include "mosaictiles.h"
#include "kstars.h"
#include "Options.h"
#include "scheduler.h"
#include "skymap.h"
#include "ekos/manager.h"
#include "ekos/mount/mount.h"
#include "schedulerprocess.h"
#include "skymapcomposite.h"
#include "ksparser.h"

#include <QDBusReply>

namespace Ekos
{

FramingAssistantUI::FramingAssistantUI(): QDialog(KStars::Instance()), ui(new Ui::FramingAssistant())
{
    ui->setupUi(this);

    auto tiles = KStarsData::Instance()->skyComposite()->mosaicComponent()->tiles();

    ui->raBox->setUnits(dmsBox::HOURS);

    // Initial optics information is taken from Ekos options
    ui->focalLenSpin->setValue(Options::telescopeFocalLength());
    ui->focalReducerSpin->setValue(Options::telescopeFocalReducer());
    ui->pixelWSizeSpin->setValue(Options::cameraPixelWidth());
    ui->pixelHSizeSpin->setValue(Options::cameraPixelHeight());
    ui->cameraWSpin->setValue(Options::cameraWidth());
    ui->cameraHSpin->setValue(Options::cameraHeight());

    ui->positionAngleSpin->setValue(tiles->positionAngle());
    ui->sequenceEdit->setText(tiles->sequenceFile());
    ui->directoryEdit->setText(tiles->outputDirectory());
    ui->targetEdit->setText(tiles->targetName());
    ui->focusEvery->setValue(tiles->focusEveryN());
    ui->alignEvery->setValue(tiles->alignEveryN());
    ui->trackStepCheck->setChecked(tiles->isTrackChecked());
    ui->focusStepCheck->setChecked(tiles->isFocusChecked());
    ui->alignStepCheck->setChecked(tiles->isAlignChecked());
    ui->guideStepCheck->setChecked(tiles->isGuideChecked());
    ui->mosaicWSpin->setValue(tiles->gridSize().width());
    ui->mosaicHSpin->setValue(tiles->gridSize().height());
    ui->overlapSpin->setValue(tiles->overlap());

    ui->groupEdit->setText(tiles->group());
    QString completionVal, completionArg;
    completionVal = tiles->completionCondition(&completionArg);
    if (completionVal == "FinishSequence")
        ui->sequenceCompletionR->setChecked(true);
    else if (completionVal == "FinishRepeat")
    {
        ui->repeatCompletionR->setChecked(true);
        ui->repeatsSpin->setValue(completionArg.toInt());
    }
    else if (completionVal == "FinishLoop")
        ui->loopCompletionR->setChecked(true);

    if (tiles->operationMode() == MosaicTiles::MODE_OPERATION)
    {
        m_CenterPoint = *tiles.data();
    }
    else
    {
        // Focus only has JNow coords (in both ra0 and ra)
        // so we need to get catalog coords so it can have valid coordinates.
        m_CenterPoint = *SkyMap::Instance()->focus();
        auto J2000Coords = m_CenterPoint.catalogueCoord(KStars::Instance()->data()->ut().djd());
        m_CenterPoint.setRA0(J2000Coords.ra0());
        m_CenterPoint.setDec0(J2000Coords.dec0());
    }

    m_CenterPoint.updateCoordsNow(KStarsData::Instance()->updateNum());
    ui->raBox->show(m_CenterPoint.ra0());
    ui->decBox->show(m_CenterPoint.dec0());

    // Page Navigation
    connect(ui->backToEquipmentB, &QPushButton::clicked, this, [this]()
    {
        ui->stackedWidget->setCurrentIndex(PAGE_EQUIPMENT);
    });

    // Go and Solve
    if (Ekos::Manager::Instance()->ekosStatus() == Ekos::Success)
    {
        ui->goSolveB->setEnabled(true);
        connect(Ekos::Manager::Instance()->mountModule(), &Ekos::Mount::newStatus, this, &Ekos::FramingAssistantUI::setMountState,
                Qt::UniqueConnection);
        connect(Ekos::Manager::Instance()->alignModule(), &Ekos::Align::newStatus, this, &Ekos::FramingAssistantUI::setAlignState,
                Qt::UniqueConnection);
    }
    connect(Ekos::Manager::Instance(), &Ekos::Manager::ekosStatusChanged, this, [this](Ekos::CommunicationStatus status)
    {
        ui->goSolveB->setEnabled(status == Ekos::Success);

        // GO AND SOLVE
        if (status == Ekos::Success)
        {
            connect(Ekos::Manager::Instance()->mountModule(), &Ekos::Mount::newStatus, this, &Ekos::FramingAssistantUI::setMountState,
                    Qt::UniqueConnection);
            connect(Ekos::Manager::Instance()->alignModule(), &Ekos::Align::newStatus, this, &Ekos::FramingAssistantUI::setAlignState,
                    Qt::UniqueConnection);
        }
    });
    connect(ui->goSolveB, &QPushButton::clicked, this, &Ekos::FramingAssistantUI::goAndSolve);

    // Import
    connect(ui->importB, &QPushButton::clicked, this, &Ekos::FramingAssistantUI::selectImport);

    // Page Navigation Controls
    connect(ui->nextToAdjustGrid, &QPushButton::clicked, this, [this]()
    {
        ui->stackedWidget->setCurrentIndex(PAGE_ADJUST_GRID);
    });
    connect(ui->backToAdjustGridB, &QPushButton::clicked, this, [this]()
    {
        ui->stackedWidget->setCurrentIndex(PAGE_ADJUST_GRID);
    });
    connect(ui->nextToSelectGridB, &QPushButton::clicked, this, [this]()
    {
        ui->stackedWidget->setCurrentIndex(PAGE_SELECT_GRID);
    });
    connect(ui->backToSelectGrid, &QPushButton::clicked, this, [this]()
    {
        ui->stackedWidget->setCurrentIndex(PAGE_SELECT_GRID);
    });
    connect(ui->nextToJobsB, &QPushButton::clicked, this, [this]()
    {
        ui->stackedWidget->setCurrentIndex(PAGE_CREATE_JOBS);
        ui->createJobsB->setEnabled(!ui->targetEdit->text().isEmpty() && !ui->sequenceEdit->text().isEmpty() &&
                                    !ui->directoryEdit->text().isEmpty());
    });

    // Respond to sky map drag event that causes a shift in the ra and de coords of the center
    connect(SkyMap::Instance(), &SkyMap::mosaicCenterChanged, this, [this](dms dRA, dms dDE)
    {
        m_CenterPoint.setRA0(range24(m_CenterPoint.ra0().Hours() + dRA.Hours()));
        m_CenterPoint.setDec0(rangeDec(m_CenterPoint.dec0().Degrees() + dDE.Degrees()));
        m_CenterPoint.updateCoordsNow(KStarsData::Instance()->updateNum());
        ui->raBox->show(m_CenterPoint.ra0());
        ui->decBox->show(m_CenterPoint.dec0());
        //m_CenterPoint.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());
        m_DebounceTimer->start();
    });

    // Update target name after edit
    connect(ui->targetEdit, &QLineEdit::editingFinished, this, &FramingAssistantUI::sanitizeTarget);

    // Recenter
    connect(ui->recenterB, &QPushButton::clicked, this, [this]()
    {
        // Focus only has JNow coords (in both ra0 and ra)
        // so we need to get catalog coords so it can have valid coordinates.
        m_CenterPoint = *SkyMap::Instance()->focus();
        auto J2000Coords = m_CenterPoint.catalogueCoord(KStars::Instance()->data()->ut().djd());
        m_CenterPoint.setRA0(J2000Coords.ra0());
        m_CenterPoint.setDec0(J2000Coords.dec0());

        m_CenterPoint.updateCoordsNow(KStarsData::Instance()->updateNum());
        ui->raBox->show(m_CenterPoint.ra0());
        ui->decBox->show(m_CenterPoint.dec0());
        m_DebounceTimer->start();
    });

    // Set initial target on startup
    if (tiles->operationMode() == MosaicTiles::MODE_PLANNING && SkyMap::IsFocused())
    {
        auto sanitized = KSUtils::sanitize(SkyMap::Instance()->focusObject()->name());
        if (sanitized != i18n("unnamed"))
        {
            ui->targetEdit->setText(sanitized);

            if (m_JobsDirectory.isEmpty())
                ui->directoryEdit->setText(QDir::cleanPath(QDir::homePath() + QDir::separator() + sanitized));
            else
                ui->directoryEdit->setText(m_JobsDirectory + QDir::separator() + sanitized);
        }
    }

    // Update object name
    connect(SkyMap::Instance(), &SkyMap::objectChanged, this, [this](SkyObject * o)
    {
        QString sanitized = o->name();
        if (sanitized != i18n("unnamed"))
        {
            // Remove illegal characters that can be problematic
            sanitized = KSUtils::sanitize(sanitized);
            ui->targetEdit->setText(sanitized);

            if (m_JobsDirectory.isEmpty())
                ui->directoryEdit->setText(QDir::cleanPath(QDir::homePath() + QDir::separator() + sanitized));
            else
                ui->directoryEdit->setText(m_JobsDirectory + QDir::separator() + sanitized);
        }
    });

    // Watch for manual changes in ra box
    connect(ui->raBox, &dmsBox::editingFinished, this, [this]
    {
        m_CenterPoint.setRA0(ui->raBox->createDms());
        m_CenterPoint.updateCoordsNow(KStarsData::Instance()->updateNum());
        m_DebounceTimer->start();
    });

    // Watch for manual hanges in de box
    connect(ui->decBox, &dmsBox::editingFinished, this, [this]
    {
        m_CenterPoint.setDec0(ui->decBox->createDms());
        m_CenterPoint.updateCoordsNow(KStarsData::Instance()->updateNum());
        m_DebounceTimer->start();
    });

    connect(ui->loadSequenceB, &QPushButton::clicked, this, &FramingAssistantUI::selectSequence);
    connect(ui->selectJobsDirB, &QPushButton::clicked, this, &Ekos::FramingAssistantUI::selectDirectory);
    // Rendering options
    ui->transparencySlider->setValue(Options::mosaicTransparencyLevel());
    ui->transparencySlider->setEnabled(!Options::mosaicTransparencyAuto());
    tiles->setPainterAlpha(Options::mosaicTransparencyLevel());
    connect(ui->transparencySlider, QOverload<int>::of(&QSlider::valueChanged), this, [&](int v)
    {
        ui->transparencySlider->setToolTip(QString("%1%").arg(v));
        Options::setMosaicTransparencyLevel(v);
        auto tiles = KStarsData::Instance()->skyComposite()->mosaicComponent()->tiles();
        tiles->setPainterAlpha(v);
        m_DebounceTimer->start();
    });
    ui->transparencyAuto->setChecked(Options::mosaicTransparencyAuto());
    tiles->setPainterAlphaAuto(Options::mosaicTransparencyAuto());
    connect(ui->transparencyAuto, &QCheckBox::toggled, this, [&](bool v)
    {
        ui->transparencySlider->setEnabled(!v);
        Options::setMosaicTransparencyAuto(v);
        auto tiles = KStarsData::Instance()->skyComposite()->mosaicComponent()->tiles();
        tiles->setPainterAlphaAuto(v);
        if (v)
            m_DebounceTimer->start();
    });

    // The update timer avoids stacking updates which crash the sky map renderer
    m_DebounceTimer = new QTimer(this);
    m_DebounceTimer->setSingleShot(true);
    m_DebounceTimer->setInterval(500);
    connect(m_DebounceTimer, &QTimer::timeout, this, &Ekos::FramingAssistantUI::constructMosaic);

    // Scope optics information
    // - Changing the optics configuration changes the FOV, which changes the target field dimensions
    connect(ui->focalLenSpin, &QDoubleSpinBox::editingFinished, this, &Ekos::FramingAssistantUI::calculateFOV);
    connect(ui->focalReducerSpin, &QDoubleSpinBox::editingFinished, this, &Ekos::FramingAssistantUI::calculateFOV);
    connect(ui->cameraWSpin, &QSpinBox::editingFinished, this, &Ekos::FramingAssistantUI::calculateFOV);
    connect(ui->cameraHSpin, &QSpinBox::editingFinished, this, &Ekos::FramingAssistantUI::calculateFOV);
    connect(ui->pixelWSizeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Ekos::FramingAssistantUI::calculateFOV);
    connect(ui->pixelHSizeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Ekos::FramingAssistantUI::calculateFOV);
    connect(ui->positionAngleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Ekos::FramingAssistantUI::calculateFOV);

    // Mosaic configuration
    // - Changing the target field dimensions changes the grid dimensions
    // - Changing the overlap field changes the grid dimensions (more intuitive than changing the field)
    // - Changing the grid dimensions changes the target field dimensions
    connect(ui->targetHFOVSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Ekos::FramingAssistantUI::updateGridFromTargetFOV);
    connect(ui->targetWFOVSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Ekos::FramingAssistantUI::updateGridFromTargetFOV);
    connect(ui->overlapSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Ekos::FramingAssistantUI::updateGridFromTargetFOV);
    connect(ui->mosaicWSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &Ekos::FramingAssistantUI::updateTargetFOVFromGrid);
    connect(ui->mosaicHSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &Ekos::FramingAssistantUI::updateTargetFOVFromGrid);

    // Lazy update for s-shape
    connect(ui->reverseOddRows, &QCheckBox::toggled, this, [&]()
    {
        renderedHFOV = 0;
        m_DebounceTimer->start();
    });

    // Buttons
    connect(ui->resetB, &QPushButton::clicked, this, &Ekos::FramingAssistantUI::updateTargetFOVFromGrid);
    connect(ui->fetchB, &QPushButton::clicked, this, &FramingAssistantUI::fetchINDIInformation);
    connect(ui->createJobsB, &QPushButton::clicked, this, &FramingAssistantUI::createJobs);

    // Job options
    connect(ui->alignEvery, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::FramingAssistantUI::rewordStepEvery);
    connect(ui->focusEvery, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::FramingAssistantUI::rewordStepEvery);

    // Get INDI Information, if avaialble.
    if (tiles->operationMode() == MosaicTiles::MODE_PLANNING)
        fetchINDIInformation();

    if (isEquipmentValid())
        ui->stackedWidget->setCurrentIndex(PAGE_SELECT_GRID);

    tiles->setOperationMode(MosaicTiles::MODE_PLANNING);
}

FramingAssistantUI::~FramingAssistantUI()
{
    delete m_DebounceTimer;
}

bool FramingAssistantUI::isEquipmentValid() const
{
    return (ui->focalLenSpin->value() > 0 && ui->cameraWSpin->value() > 0 && ui->cameraHSpin->value() > 0 &&
            ui->pixelWSizeSpin->value() > 0 && ui->pixelHSizeSpin->value() > 0);
}

double FramingAssistantUI::getTargetWFOV() const
{
    double const xFOV = ui->cameraWFOVSpin->value() * (1 - ui->overlapSpin->value() / 100.0);
    return ui->cameraWFOVSpin->value() + xFOV * (ui->mosaicWSpin->value() - 1);
}

double FramingAssistantUI::getTargetHFOV() const
{
    double const yFOV = ui->cameraHFOVSpin->value() * (1 - ui->overlapSpin->value() / 100.0);
    return ui->cameraHFOVSpin->value() + yFOV * (ui->mosaicHSpin->value() - 1);
}

double FramingAssistantUI::getTargetMosaicW() const
{
    // If FOV is invalid, or target FOV is null, or target FOV is smaller than camera FOV, we get one tile
    if (!isEquipmentValid() || !ui->targetWFOVSpin->value() || ui->targetWFOVSpin->value() <= ui->cameraWFOVSpin->value())
        return 1;

    // Else we get one tile, plus as many overlapping camera FOVs in the remnant of the target FOV
    double const xFOV = ui->cameraWFOVSpin->value() * (1 - ui->overlapSpin->value() / 100.0);
    int const tiles = 1 + ceil((ui->targetWFOVSpin->value() - ui->cameraWFOVSpin->value()) / xFOV);
    //Ekos::Manager::Instance()->schedulerModule()->appendLogText(QString("[W] Target FOV %1, camera FOV %2 after overlap %3, %4 tiles.").arg(ui->targetWFOVSpin->value()).arg(ui->cameraWFOVSpin->value()).arg(xFOV).arg(tiles));
    return tiles;
}

double FramingAssistantUI::getTargetMosaicH() const
{
    // If FOV is invalid, or target FOV is null, or target FOV is smaller than camera FOV, we get one tile
    if (!isEquipmentValid() || !ui->targetHFOVSpin->value() || ui->targetHFOVSpin->value() <= ui->cameraHFOVSpin->value())
        return 1;

    // Else we get one tile, plus as many overlapping camera FOVs in the remnant of the target FOV
    double const yFOV = ui->cameraHFOVSpin->value() * (1 - ui->overlapSpin->value() / 100.0);
    int const tiles = 1 + ceil((ui->targetHFOVSpin->value() - ui->cameraHFOVSpin->value()) / yFOV);
    //Ekos::Manager::Instance()->schedulerModule()->appendLogText(QString("[H] Target FOV %1, camera FOV %2 after overlap %3, %4 tiles.").arg(ui->targetHFOVSpin->value()).arg(ui->cameraHFOVSpin->value()).arg(yFOV).arg(tiles));
    return tiles;
}

void FramingAssistantUI::calculateFOV()
{
    if (!isEquipmentValid())
        return;

    ui->nextToSelectGridB->setEnabled(true);

    ui->targetWFOVSpin->setMinimum(ui->cameraWFOVSpin->value());
    ui->targetHFOVSpin->setMinimum(ui->cameraHFOVSpin->value());

    Options::setTelescopeFocalLength(ui->focalLenSpin->value());
    Options::setTelescopeFocalReducer(ui->focalReducerSpin->value());
    Options::setCameraPixelWidth(ui->pixelWSizeSpin->value());
    Options::setCameraPixelHeight(ui->pixelHSizeSpin->value());
    Options::setCameraWidth(ui->cameraWSpin->value());
    Options::setCameraHeight(ui->cameraHSpin->value());
    Options::setCameraRotation(ui->positionAngleSpin->value());

    auto reducedFocalLength = ui->focalLenSpin->value() * ui->focalReducerSpin->value();
    // Calculate FOV in arcmins
    const auto fov_x = 206264.8062470963552 * ui->cameraWSpin->value() * ui->pixelWSizeSpin->value() / 60000.0 /
                       reducedFocalLength;
    const auto fov_y = 206264.8062470963552 * ui->cameraHSpin->value() * ui->pixelHSizeSpin->value() / 60000.0 /
                       reducedFocalLength;

    ui->cameraWFOVSpin->setValue(fov_x);
    ui->cameraHFOVSpin->setValue(fov_y);

    double const target_fov_w = getTargetWFOV();
    double const target_fov_h = getTargetHFOV();

    if (ui->targetWFOVSpin->value() < target_fov_w)
    {
        bool const sig = ui->targetWFOVSpin->blockSignals(true);
        ui->targetWFOVSpin->setValue(target_fov_w);
        ui->targetWFOVSpin->blockSignals(sig);
    }

    if (ui->targetHFOVSpin->value() < target_fov_h)
    {
        bool const sig = ui->targetHFOVSpin->blockSignals(true);
        ui->targetHFOVSpin->setValue(target_fov_h);
        ui->targetHFOVSpin->blockSignals(sig);
    }

    m_DebounceTimer->start();
}

void FramingAssistantUI::resetFOV()
{
    if (!isEquipmentValid())
        return;

    ui->targetWFOVSpin->setValue(getTargetWFOV());
    ui->targetHFOVSpin->setValue(getTargetHFOV());
}

void FramingAssistantUI::updateTargetFOVFromGrid()
{
    if (!isEquipmentValid())
        return;

    double const targetWFOV = getTargetWFOV();
    double const targetHFOV = getTargetHFOV();

    if (ui->targetWFOVSpin->value() != targetWFOV)
    {
        bool const sig = ui->targetWFOVSpin->blockSignals(true);
        ui->targetWFOVSpin->setValue(targetWFOV);
        ui->targetWFOVSpin->blockSignals(sig);
        m_DebounceTimer->start();
    }

    if (ui->targetHFOVSpin->value() != targetHFOV)
    {
        bool const sig = ui->targetHFOVSpin->blockSignals(true);
        ui->targetHFOVSpin->setValue(targetHFOV);
        ui->targetHFOVSpin->blockSignals(sig);
        m_DebounceTimer->start();
    }
}

void FramingAssistantUI::updateGridFromTargetFOV()
{
    if (!isEquipmentValid())
        return;

    double const expectedW = getTargetMosaicW();
    double const expectedH = getTargetMosaicH();

    if (expectedW != ui->mosaicWSpin->value())
    {
        bool const sig = ui->mosaicWSpin->blockSignals(true);
        ui->mosaicWSpin->setValue(expectedW);
        ui->mosaicWSpin->blockSignals(sig);
    }

    if (expectedH != ui->mosaicHSpin->value())
    {
        bool const sig = ui->mosaicHSpin->blockSignals(true);
        ui->mosaicHSpin->setValue(expectedH);
        ui->mosaicHSpin->blockSignals(sig);
    }

    // Update unconditionally, as we may be updating the overlap or the target FOV covered by the mosaic
    m_DebounceTimer->start();
}

void FramingAssistantUI::constructMosaic()
{
    m_DebounceTimer->stop();

    if (!isEquipmentValid())
        return;

    auto tiles = KStarsData::Instance()->skyComposite()->mosaicComponent()->tiles();
    // Set Basic Metadata

    // Center
    tiles->setRA0(m_CenterPoint.ra0());
    tiles->setDec0(m_CenterPoint.dec0());
    tiles->updateCoordsNow(KStarsData::Instance()->updateNum());

    // Grid Size
    tiles->setGridSize(QSize(ui->mosaicWSpin->value(), ui->mosaicHSpin->value()));
    // Position Angle
    tiles->setPositionAngle(ui->positionAngleSpin->value());
    // Camera FOV in arcmins
    tiles->setCameraFOV(QSizeF(ui->cameraWFOVSpin->value(), ui->cameraHFOVSpin->value()));
    // Mosaic overall FOV in arcsmins
    tiles->setMosaicFOV(QSizeF(ui->targetWFOVSpin->value(), ui->targetHFOVSpin->value()));
    // Overlap in %
    tiles->setOverlap(ui->overlapSpin->value());
    // Generate Tiles
    tiles->createTiles(ui->reverseOddRows->checkState() == Qt::CheckState::Checked);
}

void FramingAssistantUI::fetchINDIInformation()
{
    // Block all signals so we can set the values directly.
    for (auto oneWidget : ui->equipment->children())
        oneWidget->blockSignals(true);
    for (auto oneWidget : ui->createGrid->children())
        oneWidget->blockSignals(true);

    QDBusInterface alignInterface("org.kde.kstars",
                                  "/KStars/Ekos/Align",
                                  "org.kde.kstars.Ekos.Align",
                                  QDBusConnection::sessionBus());

    QDBusReply<QList<double>> cameraReply = alignInterface.call("cameraInfo");
    if (cameraReply.isValid())
    {
        QList<double> const values = cameraReply.value();

        m_CameraSize = QSize(values[0], values[1]);
        ui->cameraWSpin->setValue(m_CameraSize.width());
        ui->cameraHSpin->setValue(m_CameraSize.height());
        m_PixelSize = QSizeF(values[2], values[3]);
        ui->pixelWSizeSpin->setValue(m_PixelSize.width());
        ui->pixelHSizeSpin->setValue(m_PixelSize.height());
    }

    QDBusReply<QList<double>> telescopeReply = alignInterface.call("telescopeInfo");
    if (telescopeReply.isValid())
    {
        QList<double> const values = telescopeReply.value();
        m_FocalLength = values[0];
        m_FocalReducer = values[2];
        ui->focalLenSpin->setValue(m_FocalLength);
        ui->focalReducerSpin->setValue(m_FocalReducer);
    }

    QDBusReply<QList<double>> solutionReply = alignInterface.call("getSolutionResult");
    if (solutionReply.isValid())
    {
        QList<double> const values = solutionReply.value();
        if (values[0] > INVALID_VALUE)
        {
            m_PA = KSUtils::rotationToPositionAngle(values[0]);
            ui->positionAngleSpin->setValue(m_PA);
        }
    }

    calculateFOV();

    // Restore all signals
    for (auto oneWidget : ui->equipment->children())
        oneWidget->blockSignals(false);
    for (auto oneWidget : ui->createGrid->children())
        oneWidget->blockSignals(false);
}

void FramingAssistantUI::rewordStepEvery(int v)
{
    QSpinBox * sp = dynamic_cast<QSpinBox *>(sender());
    if (0 < v)
        sp->setSuffix(i18np(" Scheduler job", " Scheduler jobs", v));
    else
        sp->setSuffix(i18n(" (first only)"));
}

void FramingAssistantUI::goAndSolve()
{
    // If user click again before solver did not start while GOTO is pending
    // let's start solver immediately if the mount is already tracking.
    if (m_GOTOSolvePending && m_MountState == ISD::Mount::MOUNT_TRACKING)
    {
        m_GOTOSolvePending = false;
        ui->goSolveB->setStyleSheet("border: 1px outset yellow");
        Ekos::Manager::Instance()->alignModule()->captureAndSolve();
    }
    // Otherwise, initiate GOTO
    else
    {
        Ekos::Manager::Instance()->alignModule()->setSolverAction(Ekos::Align::GOTO_SLEW);
        Ekos::Manager::Instance()->mountModule()->gotoTarget(m_CenterPoint);
        ui->goSolveB->setStyleSheet("border: 1px outset magenta");
        m_GOTOSolvePending = true;
    }
}

void FramingAssistantUI::createJobs()
{
    auto scheduler = Ekos::Manager::Instance()->schedulerModule();
    auto tiles = KStarsData::Instance()->skyComposite()->mosaicComponent()->tiles();
    auto sequence = ui->sequenceEdit->text();
    auto outputDirectory = ui->directoryEdit->text();
    auto target = ui->targetEdit->text();
    auto group = ui->groupEdit->text();

    tiles->setTargetName(target);
    tiles->setGroup(group);
    tiles->setOutputDirectory(outputDirectory);
    tiles->setSequenceFile(sequence);
    tiles->setFocusEveryN(ui->focusEvery->value());
    tiles->setAlignEveryN(ui->alignEvery->value());
    tiles->setStepChecks(ui->trackStepCheck->isChecked(), ui->focusStepCheck->isChecked(),
                         ui->alignStepCheck->isChecked(), ui->guideStepCheck->isChecked());

    if (ui->sequenceCompletionR->isChecked())
        tiles->setCompletionCondition("FinishSequence", "");
    else if (ui->loopCompletionR->isChecked())
        tiles->setCompletionCondition("FinishLoop", "");
    else if (ui->repeatCompletionR->isChecked())
        tiles->setCompletionCondition("FinishRepeat", QString("%1").arg(ui->repeatsSpin->value()));

    tiles->setPositionAngle(ui->positionAngleSpin->value());
    // Start by removing any jobs.
    scheduler->process()->removeAllJobs();

    QString completionVal, completionArg;

    // Completion values are for all tiles.
    completionVal = tiles->completionCondition(&completionArg);
    QJsonObject completionSettings;
    if (completionVal == "FinishSequence")
        completionSettings = {{"sequenceCheck", true}};
    else if (completionVal == "FinishRepeat")
        completionSettings = {{"repeatCheck", true}, {"repeatRuns", completionArg.toInt()}};
    else if (completionVal == "FinishLoop")
        completionSettings = {{"loopCheck", true}};

    int batchCount = 0;
    for (auto oneTile : tiles->tiles())
    {
        batchCount++;
        XMLEle *root = scheduler->process()->getSequenceJobRoot(sequence);
        if (root == nullptr)
            return;

        const auto oneTarget = QString("%1-Part_%2").arg(target).arg(batchCount);
        if (scheduler->process()->createJobSequence(root, oneTarget, outputDirectory) == false)
        {
            delXMLEle(root);
            return;
        }

        delXMLEle(root);
        auto oneSequence = QString("%1/%2.esq").arg(outputDirectory, oneTarget);

        // First job should Always focus if possible
        bool shouldFocus = ui->focusStepCheck->isChecked() && (batchCount == 1 || (batchCount % ui->focusEvery->value()) == 0);
        bool shouldAlign = ui->alignStepCheck->isChecked() && (batchCount == 1 || (batchCount % ui->alignEvery->value()) == 0);
        QVariantMap settings =
        {
            {"nameEdit", oneTarget},
            {"groupEdit", tiles->group()},
            {"raBox", oneTile->skyCenter.ra0().toHMSString()},
            {"decBox", oneTile->skyCenter.dec0().toDMSString()},
            // Take care of standard range for position angle
            {"positionAngleSpin", KSUtils::rangePA(tiles->positionAngle())},
            {"sequenceEdit", oneSequence},
            {"schedulerTrackStep", ui->trackStepCheck->isChecked()},
            {"schedulerFocusStep", shouldFocus},
            {"schedulerFocusStep", shouldAlign},
            {"schedulerGuideStep", ui->guideStepCheck->isChecked()}
        };

        scheduler->setAllSettings(settings);
        scheduler->saveJob();
    }

    auto schedulerListFile = QString("%1/%2.esl").arg(outputDirectory, target);
    scheduler->process()->saveScheduler(QUrl::fromLocalFile(schedulerListFile));
    accept();
    Ekos::Manager::Instance()->activateModule(i18n("Scheduler"), true);
    scheduler->updateJobTable();
}

void FramingAssistantUI::setMountState(ISD::Mount::Status value)
{
    m_MountState = value;
    if (m_GOTOSolvePending && m_MountState == ISD::Mount::MOUNT_TRACKING)
    {
        m_GOTOSolvePending = false;
        ui->goSolveB->setStyleSheet("border: 1px outset yellow");
        Ekos::Manager::Instance()->alignModule()->captureAndSolve();
    }
}

void FramingAssistantUI::setAlignState(AlignState value)
{
    m_AlignState = value;

    if (m_AlignState == Ekos::ALIGN_COMPLETE)
        ui->goSolveB->setStyleSheet("border: 1px outset green");
    else if (m_AlignState == Ekos::ALIGN_ABORTED || m_AlignState == Ekos::ALIGN_FAILED)
        ui->goSolveB->setStyleSheet("border: 1px outset red");
}

void FramingAssistantUI::selectSequence()
{
    QString file = QFileDialog::getOpenFileName(Ekos::Manager::Instance(), i18nc("@title:window", "Select Sequence Queue"),
                   QDir::homePath(),
                   i18n("Ekos Sequence Queue (*.esq)"));

    if (!file.isEmpty())
    {
        ui->sequenceEdit->setText(file);
        ui->createJobsB->setEnabled(!ui->targetEdit->text().isEmpty() && !ui->sequenceEdit->text().isEmpty() &&
                                    !ui->directoryEdit->text().isEmpty());
    }
}

void FramingAssistantUI::selectImport()
{
    QString file = QFileDialog::getOpenFileName(Ekos::Manager::Instance(), i18nc("@title:window", "Select Mosaic Import"),
                   QDir::homePath(),
                   i18n("Telescopius CSV (*.csv)"));

    if (!file.isEmpty())
        parseMosaicCSV(file);
}

bool FramingAssistantUI::parseMosaicCSV(const QString &filename)
{
    QList< QPair<QString, KSParser::DataTypes> > csv_sequence;
    csv_sequence.append(qMakePair(QString("Pane"), KSParser::D_QSTRING));
    csv_sequence.append(qMakePair(QString("RA"), KSParser::D_QSTRING));
    csv_sequence.append(qMakePair(QString("DEC"), KSParser::D_QSTRING));
    csv_sequence.append(qMakePair(QString("Position Angle (East)"), KSParser::D_DOUBLE));
    csv_sequence.append(qMakePair(QString("Pane width (arcmins)"), KSParser::D_DOUBLE));
    csv_sequence.append(qMakePair(QString("Pane height (arcmins)"), KSParser::D_DOUBLE));
    csv_sequence.append(qMakePair(QString("Overlap"), KSParser::D_QSTRING));
    csv_sequence.append(qMakePair(QString("Row"), KSParser::D_INT));
    csv_sequence.append(qMakePair(QString("Column"), KSParser::D_INT));
    KSParser csvParser(filename, ',', csv_sequence);

    QHash<QString, QVariant> row_content;
    int maxRow = 1, maxCol = 1;
    auto haveCenter = false;
    while (csvParser.HasNextRow())
    {
        row_content = csvParser.ReadNextRow();
        auto pane = row_content["Pane"].toString();

        // Skip first line
        if (pane == "Pane")
            continue;

        if (pane != "Center")
        {
            auto row = row_content["Row"].toInt();
            maxRow = qMax(row, maxRow);
            auto col = row_content["Column"].toInt();
            maxCol = qMax(col, maxCol);
            continue;
        }

        haveCenter = true;

        auto ra = row_content["RA"].toString().trimmed();
        auto dec = row_content["DEC"].toString().trimmed();

        ui->raBox->setText(ra.replace("hr", "h"));
        ui->decBox->setText(dec.remove("º"));

        auto pa      = row_content["Position Angle (East)"].toDouble();
        ui->positionAngleSpin->setValue(pa);

        // eg. 10% --> 10
        auto overlap = row_content["Overlap"].toString().trimmed().midRef(0, 2).toDouble();
        ui->overlapSpin->setValue(overlap);
    }

    if (haveCenter == false)
    {
        KSNotification::sorry(i18n("Import must contain center coordinates."), i18n("Sorry"), 15);
        return false;
    }

    // Set WxH
    ui->mosaicWSpin->setValue(maxRow);
    ui->mosaicHSpin->setValue(maxCol);
    // Set J2000 Center
    m_CenterPoint.setRA0(ui->raBox->createDms());
    m_CenterPoint.setDec0(ui->decBox->createDms());
    m_CenterPoint.updateCoordsNow(KStarsData::Instance()->updateNum());
    // Slew to center
    SkyMap::Instance()->setDestination(m_CenterPoint);
    SkyMap::Instance()->slewFocus();
    // Now go to position adjustments
    ui->nextToAdjustGrid->click();

    return true;
}

bool FramingAssistantUI::importMosaic(const QJsonObject &payload)
{
    // CSV should contain postion angle, ra/de of each panel, and center coordinates.
    auto csv      = payload["csv"].toString();
    // Full path to sequence file to be used for imaging.
    auto sequence = payload["sequence"].toString();
    // Name of target (needs sanitization)
    auto target   = payload["target"].toString();
    // Jobs directory
    auto directory = payload["directory"].toString();

    // Scheduler steps
    auto track    = payload["track"].toBool();
    auto focus    = payload["focus"].toBool();
    auto align    = payload["align"].toBool();
    auto guide    = payload["guide"].toBool();

    // Create temporary file to save the CSV info
    QTemporaryFile csvFile;
    if (!csvFile.open())
        return false;
    csvFile.write(csv.toUtf8());
    csvFile.close();

    // Delete debounce timer since we update all parameters programatically at once
    m_DebounceTimer->disconnect();

    if (parseMosaicCSV(csvFile.fileName()) == false)
        return false;

    constructMosaic();

    m_JobsDirectory = directory;

    // Set scheduler options.
    ui->trackStepCheck->setChecked(track);
    ui->focusStepCheck->setChecked(focus);
    ui->alignStepCheck->setChecked(align);
    ui->guideStepCheck->setChecked(guide);

    ui->sequenceEdit->setText(sequence);
    ui->targetEdit->setText(target);

    sanitizeTarget();

    // If create job is still disabled, then some configuation is missing or wrong.
    if (ui->createJobsB->isEnabled() == false)
        return false;

    // Need to wait a bit since parseMosaicCSV needs to trigger UI
    // But button clicks need to be executed first in the event loop
    ui->createJobsB->click();

    return true;
}

void FramingAssistantUI::selectDirectory()
{
    m_JobsDirectory = QFileDialog::getExistingDirectory(Ekos::Manager::Instance(), i18nc("@title:window",
                      "Select Jobs Directory"),
                      QDir::homePath());

    if (!m_JobsDirectory.isEmpty())
    {
        // If we already have a target specified, then append it to directory path.
        QString sanitized = ui->targetEdit->text();
        if (sanitized.isEmpty() == false && sanitized != i18n("unnamed"))
        {
            // Remove illegal characters that can be problematic
            sanitized = KSUtils::sanitize(sanitized);
            ui->directoryEdit->setText(m_JobsDirectory + QDir::separator() + sanitized);

        }
        else
            ui->directoryEdit->setText(m_JobsDirectory);


        ui->createJobsB->setEnabled(!ui->targetEdit->text().isEmpty() && !ui->sequenceEdit->text().isEmpty() &&
                                    !ui->directoryEdit->text().isEmpty());
    }
}

void FramingAssistantUI::sanitizeTarget()
{
    QString sanitized = ui->targetEdit->text();
    if (sanitized != i18n("unnamed"))
    {
        // Remove illegal characters that can be problematic
        sanitized = KSUtils::sanitize(sanitized);
        ui->targetEdit->blockSignals(true);
        ui->targetEdit->setText(sanitized);
        ui->targetEdit->blockSignals(false);

        if (m_JobsDirectory.isEmpty())
            ui->directoryEdit->setText(QDir::cleanPath(QDir::homePath() + QDir::separator() + sanitized));
        else
            ui->directoryEdit->setText(m_JobsDirectory + QDir::separator() + sanitized);

        ui->createJobsB->setEnabled(!ui->targetEdit->text().isEmpty() && !ui->sequenceEdit->text().isEmpty() &&
                                    !ui->directoryEdit->text().isEmpty());
    }
}
}
