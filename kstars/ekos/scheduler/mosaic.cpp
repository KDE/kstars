/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mosaic.h"
#include "ui_mosaic.h"
#include "mosaictilesmanager.h"
#include "kstars.h"
#include "Options.h"
#include "scheduler.h"
#include "skymap.h"
#include "ekos/manager.h"
#include "projections/projector.h"

#include "ekos_scheduler_debug.h"

#include <QDBusReply>

namespace Ekos
{

Mosaic::Mosaic(QString targetName, SkyPoint center, QWidget *parent): QDialog(parent), ui(new Ui::mosaicDialog())
{
    ui->setupUi(this);

    // Initial optics information is taken from Ekos options
    ui->focalLenSpin->setValue(Options::telescopeFocalLength());
    ui->pixelWSizeSpin->setValue(Options::cameraPixelWidth());
    ui->pixelHSizeSpin->setValue(Options::cameraPixelHeight());
    ui->cameraWSpin->setValue(Options::cameraWidth());
    ui->cameraHSpin->setValue(Options::cameraHeight());
    ui->rotationSpin->setValue(Options::cameraRotation());

    // Initial job location is the home path appended with the target name
    ui->jobsDir->setText(QDir::cleanPath(QDir::homePath() + QDir::separator() + targetName.replace(' ', '_')));
    ui->selectJobsDirB->setIcon(QIcon::fromTheme("document-open-folder"));

    // The update timer avoids stacking updates which crash the sky map renderer
    updateTimer = new QTimer(this);
    updateTimer->setSingleShot(true);
    updateTimer->setInterval(1000);
    connect(updateTimer, &QTimer::timeout, this, &Ekos::Mosaic::constructMosaic);

    // Scope optics information
    // - Changing the optics configuration changes the FOV, which changes the target field dimensions
    connect(ui->focalLenSpin, &QDoubleSpinBox::editingFinished, this, &Ekos::Mosaic::calculateFOV);
    connect(ui->cameraWSpin, &QSpinBox::editingFinished, this, &Ekos::Mosaic::calculateFOV);
    connect(ui->cameraHSpin, &QSpinBox::editingFinished, this, &Ekos::Mosaic::calculateFOV);
    connect(ui->pixelWSizeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Ekos::Mosaic::calculateFOV);
    connect(ui->pixelHSizeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Ekos::Mosaic::calculateFOV);
    connect(ui->rotationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Ekos::Mosaic::calculateFOV);

    // Mosaic configuration
    // - Changing the target field dimensions changes the grid dimensions
    // - Changing the overlap field changes the grid dimensions (more intuitive than changing the field)
    // - Changing the grid dimensions changes the target field dimensions
    connect(ui->targetHFOVSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Ekos::Mosaic::updateGridFromTargetFOV);
    connect(ui->targetWFOVSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Ekos::Mosaic::updateGridFromTargetFOV);
    connect(ui->overlapSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &Ekos::Mosaic::updateGridFromTargetFOV);
    connect(ui->mosaicWSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::Mosaic::updateTargetFOVFromGrid);
    connect(ui->mosaicHSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::Mosaic::updateTargetFOVFromGrid);

    // Lazy update for s-shape
    connect(ui->reverseOddRows, &QCheckBox::toggled, this, [&]()
    {
        renderedHFOV = 0;
        updateTimer->start();
    });

    // Buttons
    connect(ui->resetB, &QPushButton::clicked, this, &Ekos::Mosaic::updateTargetFOVFromGrid);
    connect(ui->selectJobsDirB, &QPushButton::clicked, this, &Ekos::Mosaic::saveJobsDirectory);
    connect(ui->fetchB, &QPushButton::clicked, this, &Mosaic::fetchINDIInformation);

    // The sky map is a pixmap background, and the mosaic tiles are rendered over it
    //m_TilesScene = new MosaicTilesScene(this);
    m_SkyPixmapItem = m_TilesScene.addPixmap(targetPix);
    m_SkyPixmapItem->setTransformationMode(Qt::TransformationMode::SmoothTransformation);
    m_MosaicTilesManager = new MosaicTilesManager(this);
    connect(m_MosaicTilesManager, &MosaicTilesManager::newOffset, this, [this](const QPointF & offset)
    {
        // Find out new center
        QPointF cartesianCenter = SkyMap::Instance()->projector()->toScreen(&m_CenterPoint);
        QPointF destinationCenter = cartesianCenter + offset;
        SkyPoint newCenter = SkyMap::Instance()->projector()->fromScreen(destinationCenter,
                             KStarsData::Instance()->lst(),
                             KStarsData::Instance()->geo()->lat());
        SkyPoint J2000Center = newCenter.catalogueCoord(KStars::Instance()->data()->ut().djd());
        setCenter(J2000Center);
        updateTimer->start();
    });
    m_TilesScene.addItem(m_MosaicTilesManager);
    ui->mosaicView->setScene(&m_TilesScene);

    // Always use Equatorial Mode in Mosaic mode
    m_RememberAltAzOption = Options::useAltAz();
    Options::setUseAltAz(false);
    m_RememberShowGround = Options::showGround();
    Options::setShowGround(false);

    // Rendering options
    connect(ui->transparencySlider, QOverload<int>::of(&QSlider::valueChanged), this, [&](int v)
    {
        ui->transparencySlider->setToolTip(QString("%1%").arg(v));
        m_MosaicTilesManager->setPainterAlpha(v);
        updateTimer->start();
    });
    connect(ui->transparencyAuto, &QCheckBox::toggled, this, [&](bool v)
    {
        ui->transparencySlider->setEnabled(!v);
        if (v)
            updateTimer->start();
    });

    // Job options
    connect(ui->alignEvery, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::Mosaic::rewordStepEvery);
    connect(ui->focusEvery, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::Mosaic::rewordStepEvery);
    ui->alignEvery->valueChanged(0);
    ui->focusEvery->valueChanged(0);

    // Center, fetch optics and adjust size
    setCenter(center);
    fetchINDIInformation();
    adjustSize();
}

Mosaic::~Mosaic()
{
    delete updateTimer;
    Options::setUseAltAz(m_RememberAltAzOption);
    Options::setShowGround(m_RememberShowGround);
}

QString Mosaic::getJobsDir() const
{
    return ui->jobsDir->text();
}

bool Mosaic::isScopeInfoValid() const
{
    if (0 < ui->focalLenSpin->value())
        if (0 < ui->cameraWSpin->value() && 0 < ui->cameraWSpin->value())
            if (0 < ui->pixelWSizeSpin->value() && 0 < ui->pixelHSizeSpin->value())
                return true;
    return false;
}

double Mosaic::getTargetWFOV() const
{
    double const xFOV = ui->cameraWFOVSpin->value() * (1 - ui->overlapSpin->value() / 100.0);
    return ui->cameraWFOVSpin->value() + xFOV * (ui->mosaicWSpin->value() - 1);
}

double Mosaic::getTargetHFOV() const
{
    double const yFOV = ui->cameraHFOVSpin->value() * (1 - ui->overlapSpin->value() / 100.0);
    return ui->cameraHFOVSpin->value() + yFOV * (ui->mosaicHSpin->value() - 1);
}

double Mosaic::getTargetMosaicW() const
{
    // If FOV is invalid, or target FOV is null, or target FOV is smaller than camera FOV, we get one tile
    if (!isScopeInfoValid() || !ui->targetWFOVSpin->value() || ui->targetWFOVSpin->value() <= ui->cameraWFOVSpin->value())
        return 1;

    // Else we get one tile, plus as many overlapping camera FOVs in the remnant of the target FOV
    double const xFOV = ui->cameraWFOVSpin->value() * (1 - ui->overlapSpin->value() / 100.0);
    int const tiles = 1 + ceil((ui->targetWFOVSpin->value() - ui->cameraWFOVSpin->value()) / xFOV);
    //Ekos::Manager::Instance()->schedulerModule()->appendLogText(QString("[W] Target FOV %1, camera FOV %2 after overlap %3, %4 tiles.").arg(ui->targetWFOVSpin->value()).arg(ui->cameraWFOVSpin->value()).arg(xFOV).arg(tiles));
    return tiles;
}

double Mosaic::getTargetMosaicH() const
{
    // If FOV is invalid, or target FOV is null, or target FOV is smaller than camera FOV, we get one tile
    if (!isScopeInfoValid() || !ui->targetHFOVSpin->value() || ui->targetHFOVSpin->value() <= ui->cameraHFOVSpin->value())
        return 1;

    // Else we get one tile, plus as many overlapping camera FOVs in the remnant of the target FOV
    double const yFOV = ui->cameraHFOVSpin->value() * (1 - ui->overlapSpin->value() / 100.0);
    int const tiles = 1 + ceil((ui->targetHFOVSpin->value() - ui->cameraHFOVSpin->value()) / yFOV);
    //Ekos::Manager::Instance()->schedulerModule()->appendLogText(QString("[H] Target FOV %1, camera FOV %2 after overlap %3, %4 tiles.").arg(ui->targetHFOVSpin->value()).arg(ui->cameraHFOVSpin->value()).arg(yFOV).arg(tiles));
    return tiles;
}

int Mosaic::exec()
{
    premosaicZoomFactor = Options::zoomFactor();

    int const result = QDialog::exec();

    // Revert various options
    updateTimer->stop();
    SkyMap *map = SkyMap::Instance();
    if (map && 0 < premosaicZoomFactor)
        map->setZoomFactor(premosaicZoomFactor);

    return result;
}

void Mosaic::accept()
{
    //createJobs();
    QDialog::accept();
}

void Mosaic::saveJobsDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(KStars::Instance(), i18nc("@title:window", "FITS Save Directory"),
                  ui->jobsDir->text());

    if (!dir.isEmpty())
        ui->jobsDir->setText(dir);
}

void Mosaic::setCenter(const SkyPoint &value)
{
    m_CenterPoint = value;
    m_CenterPoint.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());
}

void Mosaic::setCameraSize(uint16_t width, uint16_t height)
{
    ui->cameraWSpin->setValue(width);
    ui->cameraHSpin->setValue(height);
}

void Mosaic::setPixelSize(double pixelWSize, double pixelHSize)
{
    ui->pixelWSizeSpin->setValue(pixelWSize);
    ui->pixelHSizeSpin->setValue(pixelHSize);
}

void Mosaic::setFocalLength(double focalLength)
{
    ui->focalLenSpin->setValue(focalLength);
}

void Mosaic::calculateFOV()
{
    if (!isScopeInfoValid())
        return;

    ui->fovGroup->setEnabled(true);

    ui->targetWFOVSpin->setMinimum(ui->cameraWFOVSpin->value());
    ui->targetHFOVSpin->setMinimum(ui->cameraHFOVSpin->value());

    Options::setTelescopeFocalLength(ui->focalLenSpin->value());
    Options::setCameraPixelWidth(ui->pixelWSizeSpin->value());
    Options::setCameraPixelHeight(ui->pixelHSizeSpin->value());
    Options::setCameraWidth(ui->cameraWSpin->value());
    Options::setCameraHeight(ui->cameraHSpin->value());

    // Calculate FOV in arcmins
    double const fov_x =
        206264.8062470963552 * ui->cameraWSpin->value() * ui->pixelWSizeSpin->value() / 60000.0 / ui->focalLenSpin->value();
    double const fov_y =
        206264.8062470963552 * ui->cameraHSpin->value() * ui->pixelHSizeSpin->value() / 60000.0 / ui->focalLenSpin->value();

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

    updateTimer->start();
}

void Mosaic::updateTargetFOV()
{
    KStars *ks  = KStars::Instance();
    SkyMap *map = SkyMap::Instance();

    // Render the required FOV
    renderedWFOV = ui->targetWFOVSpin->value();// * cos(ui->rotationSpin->value() * dms::DegToRad);
    renderedHFOV = ui->targetHFOVSpin->value();// * sin(ui->rotationSpin->value() * dms::DegToRad);

    // Pick thrice the largest FOV to obtain a proper zoom
    double const spacing = ui->mosaicWSpin->value() < ui->mosaicHSpin->value() ? ui->mosaicHSpin->value() :
                           ui->mosaicWSpin->value();
    double const scale = 1.0 + 2.0 / (1.0 + spacing);
    double const renderedFOV = scale * (renderedWFOV < renderedHFOV ? renderedHFOV : renderedWFOV);

    // Check the aspect ratio of the sky map, assuming the map zoom considers the width (see KStars::setApproxFOV)
    double const aspect_ratio = map->width() / map->height();

    // Set the zoom (in degrees) that gives the expected FOV for the map aspect ratio, and center the target
    ks->setApproxFOV(renderedFOV * aspect_ratio / 60.0);
    //center.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    map->setClickedObject(nullptr);
    map->setClickedPoint(&m_CenterPoint);
    map->slotCenter();

    // Wait for the map to stop slewing, so that HiPS renders properly
    while(map->isSlewing())
        qApp->processEvents();
    qApp->processEvents();

    // Compute the horizontal and vertical resolutions, deduce the actual FOV of the map in arcminutes
    pixelsPerArcminRA = pixelsPerArcminDE = Options::zoomFactor() * dms::DegToRad / 60.0;

    // Get the sky map image - don't bother subframing, it causes imprecision sometimes
    QImage fullSkyChart(QSize(map->width(), map->height()), QImage::Format_RGB32);
    map->exportSkyImage(&fullSkyChart, false);
    qApp->processEvents();
    m_SkyPixmapItem->setPixmap(QPixmap::fromImage(fullSkyChart));

    // Relocate
    QRectF sceneRect = m_SkyPixmapItem->boundingRect().translated(-m_SkyPixmapItem->boundingRect().center());
    m_TilesScene.setSceneRect(sceneRect);
    m_SkyPixmapItem->setOffset(sceneRect.topLeft());
    m_SkyPixmapItem->setPos(QPointF());

}

void Mosaic::resizeEvent(QResizeEvent *)
{
    // Adjust scene rect to avoid rounding holes on border
    QRectF adjustedSceneRect(m_TilesScene.sceneRect());
    adjustedSceneRect.setTop(adjustedSceneRect.top() + 2);
    adjustedSceneRect.setLeft(adjustedSceneRect.left() + 2);
    adjustedSceneRect.setRight(adjustedSceneRect.right() - 2);
    adjustedSceneRect.setBottom(adjustedSceneRect.bottom() - 2);

    ui->mosaicView->fitInView(adjustedSceneRect, Qt::KeepAspectRatioByExpanding);
    ui->mosaicView->centerOn(QPointF());
}

void Mosaic::showEvent(QShowEvent *)
{
    resizeEvent(nullptr);
}

void Mosaic::resetFOV()
{
    if (!isScopeInfoValid())
        return;

    ui->targetWFOVSpin->setValue(getTargetWFOV());
    ui->targetHFOVSpin->setValue(getTargetHFOV());
}

void Mosaic::updateTargetFOVFromGrid()
{
    if (!isScopeInfoValid())
        return;

    double const targetWFOV = getTargetWFOV();
    double const targetHFOV = getTargetHFOV();

    if (ui->targetWFOVSpin->value() != targetWFOV)
    {
        bool const sig = ui->targetWFOVSpin->blockSignals(true);
        ui->targetWFOVSpin->setValue(targetWFOV);
        ui->targetWFOVSpin->blockSignals(sig);
        updateTimer->start();
    }

    if (ui->targetHFOVSpin->value() != targetHFOV)
    {
        bool const sig = ui->targetHFOVSpin->blockSignals(true);
        ui->targetHFOVSpin->setValue(targetHFOV);
        ui->targetHFOVSpin->blockSignals(sig);
        updateTimer->start();
    }
}

void Mosaic::updateGridFromTargetFOV()
{
    if (!isScopeInfoValid())
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
    updateTimer->start();
}

void Mosaic::constructMosaic()
{
    updateTimer->stop();

    if (!isScopeInfoValid())
        return;

    updateTargetFOV();

    if (m_MosaicTilesManager->getPA() != ui->rotationSpin->value())
        Options::setCameraRotation(ui->rotationSpin->value());

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Tile FOV in pixels W:" << ui->cameraWFOVSpin->value() * pixelsPerArcminRA << "H:"
                                   << ui->cameraHFOVSpin->value() * pixelsPerArcminDE;

    m_MosaicTilesManager->setPos(0, 0);
    m_MosaicTilesManager->setSkyCenter(m_CenterPoint);
    m_MosaicTilesManager->setGridDimensions(ui->mosaicWSpin->value(), ui->mosaicHSpin->value());
    m_MosaicTilesManager->setPositionAngle(ui->rotationSpin->value());
    m_MosaicTilesManager->setSingleTileFOV(ui->cameraWFOVSpin->value() * pixelsPerArcminRA,
                                           ui->cameraHFOVSpin->value() * pixelsPerArcminDE);
    m_MosaicTilesManager->setMosaicFOV(ui->targetWFOVSpin->value() * pixelsPerArcminRA,
                                       ui->targetHFOVSpin->value() * pixelsPerArcminDE);
    m_MosaicTilesManager->setOverlap(ui->overlapSpin->value() / 100);
    m_MosaicTilesManager->setPixelScale(QSizeF(pixelsPerArcminRA * 60.0, pixelsPerArcminDE * 60.0));
    m_MosaicTilesManager->updateTiles(m_MosaicTilesManager->mapToItem(m_SkyPixmapItem,
                                      m_SkyPixmapItem->boundingRect().center()),
                                      ui->reverseOddRows->checkState() == Qt::CheckState::Checked);

    ui->jobCountSpin->setValue(m_MosaicTilesManager->getWidth() * m_MosaicTilesManager->getHeight());

    if (ui->transparencyAuto->isChecked())
    {
        // Tiles should be more transparent when many are overlapped
        // Overlap < 50%: low transparency, as only two tiles will overlap on a line
        // 50% < Overlap < 75%: mid transparency, as three tiles will overlap one a line
        // 75% < Overlap: high transparency, as four tiles will overlap on a line
        // Slider controlling transparency provides [5%,50%], which is scaled to 0-200 alpha.

        if (1 < ui->jobCountSpin->value())
            ui->transparencySlider->setValue(40 - ui->overlapSpin->value() / 2);
        else
            ui->transparencySlider->setValue(40);

        ui->transparencySlider->update();
    }

    resizeEvent(nullptr);
    m_MosaicTilesManager->show();

    ui->mosaicView->update();
}

QList <Mosaic::Job> Mosaic::getJobs() const
{
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Mosaic Tile W:" << m_MosaicTilesManager->boundingRect().width() << "H:" <<
                                   m_MosaicTilesManager->boundingRect().height();

    QList <Mosaic::Job> result;

    // We have two items:
    // 1. SkyMapItem is the pixmap we fetch from KStars that shows the sky field.
    // 2. MosaicItem is the constructed mosaic boxes.
    // We already know the center (RA0,DE0) of the SkyMapItem.
    // We Map the coordinate of each tile to the SkyMapItem to find out where the tile center is located
    // on the SkyMapItem pixmap.
    // We calculate the difference between the tile center and the SkyMapItem center and then find the tile coordinates
    // in J2000 coords.
    for (int i = 0; i < m_MosaicTilesManager->getHeight(); i++)
    {
        for (int j = 0; j < m_MosaicTilesManager->getWidth(); j++)
        {
            MosaicTilesManager::OneTile * const tile = m_MosaicTilesManager->getTile(i, j);
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Tile #" << i * m_MosaicTilesManager->getWidth() + j << "Center:" << tile->center;

            Job ts;
            ts.center.setRA0(tile->skyCenter.ra0().Hours());
            ts.center.setDec0(tile->skyCenter.dec0().Degrees());
            ts.rotation = -m_MosaicTilesManager->getPA();

            ts.doAlign =
                (0 < ui->alignEvery->value()) &&
                (0 == ((j + i * m_MosaicTilesManager->getHeight()) % ui->alignEvery->value()));

            ts.doFocus =
                (0 < ui->focusEvery->value()) &&
                (0 == ((j + i * m_MosaicTilesManager->getHeight()) % ui->focusEvery->value()));

            qCDebug(KSTARS_EKOS_SCHEDULER) << "Tile RA0:" << tile->skyCenter.ra0().toHMSString() << "DE0:" <<
                                           tile->skyCenter.dec0().toDMSString();
            result.append(ts);
        }
    }

    return result;
}

void Mosaic::fetchINDIInformation()
{
    QDBusInterface alignInterface("org.kde.kstars",
                                  "/KStars/Ekos/Align",
                                  "org.kde.kstars.Ekos.Align",
                                  QDBusConnection::sessionBus());

    QDBusReply<QList<double>> cameraReply = alignInterface.call("cameraInfo");
    if (cameraReply.isValid())
    {
        QList<double> const values = cameraReply.value();

        setCameraSize(values[0], values[1]);
        setPixelSize(values[2], values[3]);
    }

    QDBusReply<QList<double>> telescopeReply = alignInterface.call("telescopeInfo");
    if (telescopeReply.isValid())
    {
        QList<double> const values = telescopeReply.value();
        setFocalLength(values[0]);
    }

    QDBusReply<QList<double>> solutionReply = alignInterface.call("getSolutionResult");
    if (solutionReply.isValid())
    {
        QList<double> const values = solutionReply.value();
        if (values[0] > INVALID_VALUE)
            ui->rotationSpin->setValue(values[0]);
    }

    calculateFOV();
}

void Mosaic::rewordStepEvery(int v)
{
    QSpinBox * sp = dynamic_cast<QSpinBox *>(sender());
    if (0 < v)
        sp->setSuffix(i18np(" Scheduler job", " Scheduler jobs", v));
    else
        sp->setSuffix(i18n(" (first only)"));
}

}
