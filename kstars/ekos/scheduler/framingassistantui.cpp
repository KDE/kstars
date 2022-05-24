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
#include "projections/projector.h"
#include "skymapcomposite.h"

#include "ekos_scheduler_debug.h"

namespace Ekos
{

FramingAssistantUI::FramingAssistantUI(): QDialog(KStars::Instance()), ui(new Ui::FramingAssistant())
{
    ui->setupUi(this);

    auto tiles = KStarsData::Instance()->skyComposite()->mosaicComponent()->tiles();

    // Initial optics information is taken from Ekos options
    ui->focalLenSpin->setValue(Options::telescopeFocalLength());
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
    ui->raBox->setDMS(m_CenterPoint.ra0().toHMSString());
    ui->decBox->setDMS(m_CenterPoint.dec0().toDMSString());

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
        ui->raBox->setDMS(m_CenterPoint.ra0().toHMSString());
        ui->decBox->setDMS(m_CenterPoint.dec0().toDMSString());
        //m_CenterPoint.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());
        m_DebounceTimer->start();
    });

    // Update target name after edit
    connect(ui->targetEdit, &QLineEdit::editingFinished, this, [this]()
    {
        QString sanitized = ui->targetEdit->text();
        if (sanitized != i18n("unnamed"))
        {
            // Remove illegal characters that can be problematic
            sanitized = sanitize(sanitized);
            ui->targetEdit->blockSignals(true);
            ui->targetEdit->setText(sanitized);
            ui->targetEdit->blockSignals(false);
            ui->directoryEdit->setText(QDir::cleanPath(QDir::homePath() + QDir::separator() + sanitized));

            ui->createJobsB->setEnabled(!ui->targetEdit->text().isEmpty() && !ui->sequenceEdit->text().isEmpty() &&
                                        !ui->directoryEdit->text().isEmpty());
        }

    });

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
        ui->raBox->setDMS(m_CenterPoint.ra0().toHMSString());
        ui->decBox->setDMS(m_CenterPoint.dec0().toDMSString());
        m_DebounceTimer->start();
    });

    // Set initial target on startup
    if (tiles->operationMode() == MosaicTiles::MODE_PLANNING && SkyMap::IsFocused())
    {
        auto target = sanitize(SkyMap::Instance()->focusObject()->name());
        ui->targetEdit->setText(target);
        ui->directoryEdit->setText(QDir::cleanPath(QDir::homePath() + QDir::separator() + target));
    }

    // Update object name
    connect(SkyMap::Instance(), &SkyMap::objectChanged, this, [this](SkyObject * o)
    {
        QString sanitized = o->name();
        if (sanitized != i18n("unnamed"))
        {
            // Remove illegal characters that can be problematic
            sanitized = sanitize(sanitized);
            ui->targetEdit->setText(sanitized);
            ui->directoryEdit->setText(QDir::cleanPath(QDir::homePath() + QDir::separator() + sanitized));
        }
    });

    // Watch for manual changes in ra box
    connect(ui->raBox, &dmsBox::editingFinished, this, [this]
    {
        m_CenterPoint.setRA0(ui->raBox->createDms(false));
        m_CenterPoint.updateCoordsNow(KStarsData::Instance()->updateNum());
        m_DebounceTimer->start();
    });

    // Watch for manual hanges in de box
    connect(ui->decBox, &dmsBox::editingFinished, this, [this]
    {
        m_CenterPoint.setDec0(ui->decBox->createDms(true));
        m_CenterPoint.updateCoordsNow(KStarsData::Instance()->updateNum());
        m_DebounceTimer->start();
    });

    connect(ui->loadSequenceB, &QPushButton::clicked, this, &FramingAssistantUI::selectSequence);
    connect(ui->selectJobsDirB, &QPushButton::clicked, this, &Ekos::FramingAssistantUI::selectDirectory);
    // Rendering options
    ui->transparencySlider->setValue(Options::mosaicTransparencyLevel());
    tiles->setPainterAlpha(Options::mosaicTransparencyLevel());
    connect(ui->transparencySlider, QOverload<int>::of(&QSlider::valueChanged), this, [&](int v)
    {
        ui->transparencySlider->setToolTip(QString("%1%").arg(v));
        auto tiles = KStarsData::Instance()->skyComposite()->mosaicComponent()->tiles();
        tiles->setPainterAlpha(v);
        m_DebounceTimer->start();
    });
    ui->transparencyAuto->setChecked(Options::mosaicTransparencyAuto());
    tiles->setPainterAlphaAuto(Options::mosaicTransparencyAuto());
    connect(ui->transparencyAuto, &QCheckBox::toggled, this, [&](bool v)
    {
        ui->transparencySlider->setEnabled(!v);
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
    Options::setCameraPixelWidth(ui->pixelWSizeSpin->value());
    Options::setCameraPixelHeight(ui->pixelHSizeSpin->value());
    Options::setCameraWidth(ui->cameraWSpin->value());
    Options::setCameraHeight(ui->cameraHSpin->value());

    // Calculate FOV in arcmins
    const auto fov_x = 206264.8062470963552 * ui->cameraWSpin->value() * ui->pixelWSizeSpin->value() / 60000.0 /
                       ui->focalLenSpin->value();
    const auto fov_y = 206264.8062470963552 * ui->cameraHSpin->value() * ui->pixelHSizeSpin->value() / 60000.0 /
                       ui->focalLenSpin->value();

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

    // Update PA
    if (std::abs(tiles->positionAngle() - ui->positionAngleSpin->value()) > 0)
    {
        Options::setCameraRotation(ui->positionAngleSpin->value());
    }

    //    qCDebug(KSTARS_EKOS_SCHEDULER) << "Tile FOV in pixels W:" << ui->cameraWFOVSpin->value() * pixelsPerArcminRA << "H:"
    //                                   << ui->cameraHFOVSpin->value() * pixelsPerArcminDE;

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
        ui->focalLenSpin->setValue(m_FocalLength);
    }

    QDBusReply<QList<double>> solutionReply = alignInterface.call("getSolutionResult");
    if (solutionReply.isValid())
    {
        QList<double> const values = solutionReply.value();
        if (values[0] > INVALID_VALUE)
        {
            m_Rotation = values[0] + 180;
            if (m_Rotation > 180)
                m_Rotation -= 360.0;
            if (m_Rotation < -180)
                m_Rotation += 360.0;
            ui->positionAngleSpin->setValue(m_Rotation);
        }
    }

    calculateFOV();
}

void FramingAssistantUI::rewordStepEvery(int v)
{
    QSpinBox * sp = dynamic_cast<QSpinBox *>(sender());
    if (0 < v)
        sp->setSuffix(i18np(" Scheduler job", " Scheduler jobs", v));
    else
        sp->setSuffix(i18n(" (first only)"));
}

QString FramingAssistantUI::sanitize(const QString &name)
{
    QString sanitized = name;
    if (sanitized != i18n("unnamed"))
    {
        // Remove illegal characters that can be problematic
        sanitized = sanitized.replace( QRegularExpression("\\s|/|\\(|\\)|:|\\*|~|\"" ), "_" )
                    // Remove any two or more __
                    .replace( QRegularExpression("_{2,}"), "_")
                    // Remove any _ at the end
                    .replace( QRegularExpression("_$"), "");
    }
    return sanitized;
}

void FramingAssistantUI::goAndSolve()
{
    // If user click again before solver did not start while GOTO is pending
    // let's start solver immediately if the mount is already tracking.
    if (m_GOTOSolvePending && m_MountState == ISD::Telescope::MOUNT_TRACKING)
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

    tiles->setTargetName(target);
    tiles->setOutputDirectory(outputDirectory);
    tiles->setSequenceFile(sequence);
    tiles->setFocusEveryN(ui->focusEvery->value());
    tiles->setAlignEveryN(ui->alignEvery->value());
    tiles->setStepChecks(ui->trackStepCheck->isChecked(), ui->focusStepCheck->isChecked(),
                         ui->alignStepCheck->isChecked(), ui->guideStepCheck->isChecked());
    tiles->setPositionAngle(ui->positionAngleSpin->value());
    // Start by removing any jobs.
    scheduler->removeAllJobs();

    int batchCount = 0;
    for (auto oneTile : tiles->tiles())
    {
        batchCount++;
        XMLEle *root = scheduler->getSequenceJobRoot(sequence);
        if (root == nullptr)
            return;

        const auto oneTarget = QString("%1-Part%2").arg(target).arg(batchCount);
        if (scheduler->createJobSequence(root, oneTarget, outputDirectory) == false)
        {
            delXMLEle(root);
            return;
        }

        delXMLEle(root);
        auto oneSequence = QString("%1/%2.esq").arg(outputDirectory, oneTarget);

        // First job should Always focus if possible
        bool shouldFocus = ui->focusStepCheck->isChecked() && (batchCount == 1 || (batchCount % ui->focusEvery->value()) == 0);
        bool shouldAlign = ui->alignStepCheck->isChecked() && (batchCount == 1 || (batchCount % ui->alignEvery->value()) == 0);
        QJsonObject settings =
        {
            {"target", oneTarget},
            {"ra", oneTile->skyCenter.ra0().toHMSString()},
            {"dec", oneTile->skyCenter.dec0().toDMSString()},
            {"pa", tiles->positionAngle()},
            {"sequence", oneSequence},
            {"track", ui->trackStepCheck->isChecked()},
            {"focus", shouldFocus},
            {"align", shouldAlign},
            {"guide", ui->guideStepCheck->isChecked()}
        };

        scheduler->setPrimarySettings(settings);

        scheduler->saveJob();
    }

    auto schedulerListFile = QString("%1/%2.esl").arg(outputDirectory, target);
    scheduler->saveScheduler(QUrl::fromLocalFile(schedulerListFile));
    accept();
    Ekos::Manager::Instance()->raise();
}

void FramingAssistantUI::setMountState(ISD::Telescope::Status value)
{
    m_MountState = value;
    if (m_GOTOSolvePending && m_MountState == ISD::Telescope::MOUNT_TRACKING)
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

void FramingAssistantUI::selectDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(Ekos::Manager::Instance(), i18nc("@title:window", "Select Jobs Directory"),
                  QDir::homePath());

    if (!dir.isEmpty())
    {
        ui->directoryEdit->setText(dir);
        ui->createJobsB->setEnabled(!ui->targetEdit->text().isEmpty() && !ui->sequenceEdit->text().isEmpty() &&
                                    !ui->directoryEdit->text().isEmpty());
    }
}

}
