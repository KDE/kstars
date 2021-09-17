/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mosaic.h"
#include "ui_mosaic.h"

#include "kstars.h"
#include "Options.h"
#include "scheduler.h"
#include "skymap.h"
#include "ekos/manager.h"
#include "projections/projector.h"

#include "ekos_scheduler_debug.h"

namespace Ekos
{
class MosaicTile : public QGraphicsItem
{
    public:
        // TODO: make this struct a QGraphicsItem
        typedef struct
        {
            QPointF pos;
            QPointF center;
            SkyPoint skyCenter;
            double rotation;
            int index;
        } OneTile;

    public:
        MosaicTile()
        {
            brush.setStyle(Qt::NoBrush);
            QColor lightGray(200, 200, 200, 100);
            pen.setColor(lightGray);
            pen.setWidth(1);

            textBrush.setStyle(Qt::SolidPattern);
            textPen.setColor(Qt::red);
            textPen.setWidth(2);

            setFlags(QGraphicsItem::ItemIsMovable);
        }

        ~MosaicTile()
        {
            qDeleteAll(tiles);
        }

    public:
        void setSkyCenter(SkyPoint center)
        {
            skyCenter = center;
        }

        void setPositionAngle(double positionAngle)
        {
            pa = std::fmod(positionAngle * -1 + 360.0, 360.0);

            // Rotate the whole mosaic around its local center
            setTransformOriginPoint(QPointF());
            setRotation(pa);
        }

        void setGridDimensions(int width, int height)
        {
            w = width;
            h = height;
        }

        void setSingleTileFOV(double fov_x, double fov_y)
        {
            fovW = fov_x;
            fovH = fov_y;
        }

        void setMosaicFOV(double mfov_x, double mfov_y)
        {
            mfovW = mfov_x;
            mfovH = mfov_y;
        }

        void setOverlap(double value)
        {
            overlap = (value < 0) ? 0 : (1 < value) ? 1 : value;
        }

    public:
        int getWidth()
        {
            return w;
        }

        int getHeight()
        {
            return h;
        }

        double getOverlap()
        {
            return overlap;
        }

        double getPA()
        {
            return pa;
        }

        void setPainterAlpha(int v)
        {
            m_PainterAlpha = v;
        }

    public:
        /// @internal Returns scaled offsets for a pixel local coordinate.
        ///
        /// This uses the mosaic center as reference and the argument resolution of the sky map at that center.
        QSizeF adjustCoordinate(QPointF tileCoord, QSizeF pixPerArcsec)
        {
            // Compute the declination of the tile row from the mosaic center
            double const dec = skyCenter.dec0().Degrees() + tileCoord.y() / pixPerArcsec.height();

            // Adjust RA based on the shift in declination
            QSizeF const toSpherical(
                        1 / (pixPerArcsec.width() * cos(dec * dms::DegToRad)),
                        1 / (pixPerArcsec.height()));

            // Return the adjusted coordinates as a QSizeF in degrees
            return QSizeF(tileCoord.x() * toSpherical.width(), tileCoord.y() * toSpherical.height());
        }

        void updateTiles(QPointF skymapCenter, QSizeF pixPerArcsec, bool s_shaped)
        {
            prepareGeometryChange();

            qDeleteAll(tiles);
            tiles.clear();

            // Sky map has objects moving from left to right, so configure the mosaic from right to left, column per column

            // Offset is our tile size with an overlap removed
            double const xOffset = fovW * (1 - overlap);
            double const yOffset = fovH * (1 - overlap);

            // We start at top right corner, (0,0) being the center of the tileset
            double initX = +(fovW + xOffset * (w - 1)) / 2.0 - fovW;
            double initY = -(fovH + yOffset * (h - 1)) / 2.0;

            double x = initX, y = initY;

            qCDebug(KSTARS_EKOS_SCHEDULER) << "Mosaic Tile FovW" << fovW << "FovH" << fovH << "initX" << x << "initY" << y <<
                                              "Offset X " << xOffset << " Y " << yOffset << " rotation " << getPA() << " reverseOdd " << s_shaped;

            int index = 0;
            for (int col = 0; col < w; col++)
            {
                y = (s_shaped && (col % 2)) ? (y - yOffset) : initY;

                for (int row = 0; row < h; row++)
                {
                    OneTile *tile = new OneTile();

                    if (!tile)
                        continue;

                    tile->pos.setX(x);
                    tile->pos.setY(y);

                    tile->center.setX(tile->pos.x() + (fovW / 2.0));
                    tile->center.setY(tile->pos.y() + (fovH / 2.0));

                    // The location of the tile on the sky map refers to the center of the mosaic, and rotates with the mosaic itself
                    QPointF tileSkyLocation = skymapCenter - rotatePoint(tile->center, QPointF(), rotation());

                    // Compute the adjusted location in RA/DEC
                    QSizeF const tileSkyOffsetScaled = adjustCoordinate(tileSkyLocation, pixPerArcsec);

                    tile->skyCenter.setRA0((skyCenter.ra0().Degrees() + tileSkyOffsetScaled.width())/15.0);
                    tile->skyCenter.setDec0(skyCenter.dec0().Degrees() + tileSkyOffsetScaled.height());
                    tile->skyCenter.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());

                    tile->rotation = tile->skyCenter.ra0().Degrees() - skyCenter.ra0().Degrees();

                    // Large rotations handled wrong by the algorithm - prefer doing multiple mosaics
                    if (abs(tile->rotation) <= 90.0)
                    {
                        tile->index = ++index;
                        tiles.append(tile);
                    }
                    else
                    {
                        delete tile;
                        tiles.append(nullptr);
                    }

                    y += (s_shaped && (col % 2)) ? -yOffset : +yOffset;
                }

                x -= xOffset;
            }
        }

        OneTile *getTile(int row, int col)
        {
            int offset = row * w + col;

            if (offset < 0 || offset >= tiles.size())
                return nullptr;

            return tiles[offset];
        }

        QRectF boundingRect() const override
        {
            double const xOffset = fovW * (1 - overlap);
            double const yOffset = fovH * (1 - overlap);
            return QRectF(0, 0, fovW + xOffset * (w - 1), fovH + yOffset * (h - 1));
        }

        QList<OneTile *> getTiles() const
        {
            return tiles;
        }

    protected:
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override
        {
            if (!tiles.size())
                return;

            QFont defaultFont = painter->font();
            QRect const oneRect(-fovW/2, -fovH/2, fovW, fovH);

            // HACK: all tiles should be QGraphicsItem instances so that texts would be scaled properly
            double const fontScale = 1/log(tiles.size() < 4 ? 4 : tiles.size());

            // Draw a light background field first to help detect holes - reduce alpha as we are stacking tiles over this
            painter->setBrush(QBrush(QColor(255, 0, 0, (200*m_PainterAlpha)/100), Qt::SolidPattern));
            painter->setPen(QPen(painter->brush(), 2, Qt::PenStyle::DotLine));
            painter->drawRect(QRectF(QPointF(-mfovW/2, -mfovH/2), QSizeF(mfovW, mfovH)));

            // Fill tiles with a transparent brush to show overlaps
            QBrush tileBrush(QColor(0, 255, 0, (200*m_PainterAlpha)/100), Qt::SolidPattern);

            // Draw each tile, adjusted for rotation
            for (int row = 0; row < h; row++)
            {
                for (int col = 0; col < w; col++)
                {
                    OneTile const * const tile = getTile(row, col);
                    if (tile)
                    {
                        QTransform const transform = painter->worldTransform();
                        painter->translate(tile->center);
                        painter->rotate(tile->rotation);

                        painter->setBrush(tileBrush);
                        painter->setPen(pen);

                        painter->drawRect(oneRect);

                        painter->setWorldTransform(transform);
                    }
                }
            }

            // Overwrite with tile information
            for (int row = 0; row < h; row++)
            {
                for (int col = 0; col < w; col++)
                {
                    OneTile const * const tile = getTile(row, col);
                    if (tile)
                    {
                        QTransform const transform = painter->worldTransform();
                        painter->translate(tile->center);
                        painter->rotate(tile->rotation);

                        painter->setBrush(textBrush);
                        painter->setPen(textPen);

                        defaultFont.setPointSize(50*fontScale);
                        painter->setFont(defaultFont);
                        painter->drawText(oneRect, Qt::AlignRight | Qt::AlignTop, QString("%1.").arg(tile->index));

                        defaultFont.setPointSize(20*fontScale);
                        painter->setFont(defaultFont);
                        painter->drawText(oneRect, Qt::AlignHCenter | Qt::AlignVCenter, QString("%1\n%2")
                                          .arg(tile->skyCenter.ra0().toHMSString())
                                          .arg(tile->skyCenter.dec0().toDMSString()));
                        painter->drawText(oneRect, Qt::AlignHCenter | Qt::AlignBottom, QString("%1%2°")
                                          .arg(tile->rotation >= 0.01 ? '+' : tile->rotation <= -0.01 ? '-' : '~')
                                          .arg(abs(tile->rotation), 5, 'f', 2));

                        painter->setWorldTransform(transform);
                    }
                }
            }
        }

        QPointF rotatePoint(QPointF pointToRotate, QPointF centerPoint, double paDegrees)
        {
            double angleInRadians = paDegrees * dms::DegToRad;
            double cosTheta       = cos(angleInRadians);
            double sinTheta       = sin(angleInRadians);

            QPointF rotation_point;

            rotation_point.setX((cosTheta * (pointToRotate.x() - centerPoint.x()) -
                                 sinTheta * (pointToRotate.y() - centerPoint.y()) + centerPoint.x()));
            rotation_point.setY((sinTheta * (pointToRotate.x() - centerPoint.x()) +
                                 cosTheta * (pointToRotate.y() - centerPoint.y()) + centerPoint.y()));

            return rotation_point;
        }

    private:
        SkyPoint skyCenter;

        double overlap { 0 };
        int w { 1 };
        int h { 1 };
        double fovW { 0 };
        double fovH { 0 };
        double mfovW { 0 };
        double mfovH { 0 };
        double pa { 0 };

        QBrush brush;
        QPen pen;

        QBrush textBrush;
        QPen textPen;

        int m_PainterAlpha { 50 };

        QList<OneTile *> tiles;
};

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
    ui->jobsDir->setText(QDir::cleanPath(QDir::homePath() + QDir::separator() + targetName.replace(' ','_')));
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
    connect(ui->targetHFOVSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Ekos::Mosaic::updateGridFromTargetFOV);
    connect(ui->targetWFOVSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Ekos::Mosaic::updateGridFromTargetFOV);
    connect(ui->overlapSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Ekos::Mosaic::updateGridFromTargetFOV);
    connect(ui->mosaicWSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::Mosaic::updateTargetFOVFromGrid);
    connect(ui->mosaicHSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::Mosaic::updateTargetFOVFromGrid);

    // Lazy update for s-shape
    connect(ui->reverseOddRows, &QCheckBox::toggled, this, [&]() { renderedHFOV = 0; updateTimer->start(); });

    // Buttons
    connect(ui->resetB, &QPushButton::clicked, this, &Ekos::Mosaic::updateTargetFOVFromGrid);
    connect(ui->selectJobsDirB, &QPushButton::clicked, this, &Ekos::Mosaic::saveJobsDirectory);
    connect(ui->fetchB, &QPushButton::clicked, this, &Mosaic::fetchINDIInformation);

    // The sky map is a pixmap background, and the mosaic tiles are rendered over it
    skyMapItem = scene.addPixmap(targetPix);
    skyMapItem->setTransformationMode(Qt::TransformationMode::SmoothTransformation);
    mosaicTileItem = new MosaicTile();
    scene.addItem(mosaicTileItem);
    ui->mosaicView->setScene(&scene);

    // Always use Equatorial Mode in Mosaic mode
    rememberAltAzOption = Options::useAltAz();
    Options::setUseAltAz(false);

    // Dialog can only be accepted when creating two tiles or more
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    // Rendering options
    connect(ui->transparencySlider, QOverload<int>::of(&QSlider::valueChanged), this, [&](int v)
    {
        ui->transparencySlider->setToolTip(QString("%1%").arg(v));
        mosaicTileItem->setPainterAlpha(v);
        updateTimer->start();
    });
    connect(ui->transparencyAuto, &QCheckBox::toggled, this, [&](bool v)
    {
        emit ui->transparencySlider->setEnabled(!v);
        if (v)
            updateTimer->start();
    });

    // Job options
    connect(ui->alignEvery, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::Mosaic::rewordStepEvery);
    connect(ui->focusEvery, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::Mosaic::rewordStepEvery);
    emit ui->alignEvery->valueChanged(0);
    emit ui->focusEvery->valueChanged(0);

    // Center, fetch optics and adjust size
    setCenter(center);
    fetchINDIInformation();
    adjustSize();
}

Mosaic::~Mosaic()
{
    delete updateTimer;
    Options::setUseAltAz(rememberAltAzOption);
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
    double const xFOV = ui->cameraWFOVSpin->value() * (1 - ui->overlapSpin->value()/100.0);
    return ui->cameraWFOVSpin->value() + xFOV * (ui->mosaicWSpin->value() - 1);
}

double Mosaic::getTargetHFOV() const
{
    double const yFOV = ui->cameraHFOVSpin->value() * (1 - ui->overlapSpin->value()/100.0);
    return ui->cameraHFOVSpin->value() + yFOV * (ui->mosaicHSpin->value() - 1);
}

double Mosaic::getTargetMosaicW() const
{
    // If FOV is invalid, or target FOV is null, or target FOV is smaller than camera FOV, we get one tile
    if (!isScopeInfoValid() || !ui->targetWFOVSpin->value() || ui->targetWFOVSpin->value() <= ui->cameraWFOVSpin->value())
        return 1;

    // Else we get one tile, plus as many overlapping camera FOVs in the remnant of the target FOV
    double const xFOV = ui->cameraWFOVSpin->value() * (1 - ui->overlapSpin->value()/100.0);
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
    double const yFOV = ui->cameraHFOVSpin->value() * (1 - ui->overlapSpin->value()/100.0);
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
    QString dir = QFileDialog::getExistingDirectory(KStars::Instance(), i18nc("@title:window", "FITS Save Directory"), ui->jobsDir->text());

    if (!dir.isEmpty())
        ui->jobsDir->setText(dir);
}

void Mosaic::setCenter(const SkyPoint &value)
{
    center = value;
    center.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());
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
    double const spacing = ui->mosaicWSpin->value() < ui->mosaicHSpin->value() ? ui->mosaicHSpin->value() : ui->mosaicWSpin->value();
    double const scale = 1.0 + 2.0 / (1.0 + spacing);
    double const renderedFOV = scale * (renderedWFOV < renderedHFOV ? renderedHFOV : renderedWFOV);

    // Check the aspect ratio of the sky map, assuming the map zoom considers the width (see KStars::setApproxFOV)
    double const aspect_ratio = map->width() / map->height();

    // Set the zoom (in degrees) that gives the expected FOV for the map aspect ratio, and center the target
    ks->setApproxFOV(renderedFOV * aspect_ratio / 60.0);
    //center.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    map->setClickedObject(nullptr);
    map->setClickedPoint(&center);
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
    skyMapItem->setPixmap(QPixmap::fromImage(fullSkyChart));

    // Relocate
    QRectF sceneRect = skyMapItem->boundingRect().translated(-skyMapItem->boundingRect().center());
    scene.setSceneRect(sceneRect);
    skyMapItem->setOffset(sceneRect.topLeft());
    skyMapItem->setPos(QPointF());
}

void Mosaic::resizeEvent(QResizeEvent *)
{
    // Adjust scene rect to avoid rounding holes on border
    QRectF adjustedSceneRect(scene.sceneRect());
    adjustedSceneRect.setTop(adjustedSceneRect.top()+2);
    adjustedSceneRect.setLeft(adjustedSceneRect.left()+2);
    adjustedSceneRect.setRight(adjustedSceneRect.right()-2);
    adjustedSceneRect.setBottom(adjustedSceneRect.bottom()-2);

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

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(ui->mosaicWSpin->value() > 1 || ui->mosaicHSpin->value() > 1);

    if (mosaicTileItem->getPA() != ui->rotationSpin->value())
        Options::setCameraRotation(ui->rotationSpin->value());

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Tile FOV in pixels W:" << ui->cameraWFOVSpin->value() * pixelsPerArcminRA << "H:"
                                   << ui->cameraHFOVSpin->value() * pixelsPerArcminDE;

    mosaicTileItem->setSkyCenter(center);
    mosaicTileItem->setGridDimensions(ui->mosaicWSpin->value(), ui->mosaicHSpin->value());
    mosaicTileItem->setPositionAngle(ui->rotationSpin->value());
    mosaicTileItem->setSingleTileFOV(ui->cameraWFOVSpin->value() * pixelsPerArcminRA, ui->cameraHFOVSpin->value() * pixelsPerArcminDE);
    mosaicTileItem->setMosaicFOV(ui->targetWFOVSpin->value() * pixelsPerArcminRA, ui->targetHFOVSpin->value() * pixelsPerArcminDE);
    mosaicTileItem->setOverlap(ui->overlapSpin->value() / 100);
    mosaicTileItem->updateTiles(mosaicTileItem->mapToItem(skyMapItem, skyMapItem->boundingRect().center()), QSizeF(pixelsPerArcminRA*60.0, pixelsPerArcminDE*60.0), ui->reverseOddRows->checkState() == Qt::CheckState::Checked);

    ui->jobCountSpin->setValue(mosaicTileItem->getWidth() * mosaicTileItem->getHeight());

    if (ui->transparencyAuto->isChecked())
    {
        // Tiles should be more transparent when many are overlapped
        // Overlap < 50%: low transparency, as only two tiles will overlap on a line
        // 50% < Overlap < 75%: mid transparency, as three tiles will overlap one a line
        // 75% < Overlap: high transparency, as four tiles will overlap on a line
        // Slider controlling transparency provides [5%,50%], which is scaled to 0-200 alpha.

        if (1 < ui->jobCountSpin->value())
            ui->transparencySlider->setValue(40 - ui->overlapSpin->value()/2);
        else
            ui->transparencySlider->setValue(40);

        ui->transparencySlider->update();
    }

    resizeEvent(nullptr);
    mosaicTileItem->show();

    ui->mosaicView->update();
}

QList <Mosaic::Job> Mosaic::getJobs() const
{
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Mosaic Tile W:" << mosaicTileItem->boundingRect().width() << "H:" <<
                                   mosaicTileItem->boundingRect().height();

    QList <Mosaic::Job> result;

    // We have two items:
    // 1. SkyMapItem is the pixmap we fetch from KStars that shows the sky field.
    // 2. MosaicItem is the constructed mosaic boxes.
    // We already know the center (RA0,DE0) of the SkyMapItem.
    // We Map the coordinate of each tile to the SkyMapItem to find out where the tile center is located
    // on the SkyMapItem pixmap.
    // We calculate the difference between the tile center and the SkyMapItem center and then find the tile coordinates
    // in J2000 coords.
    for (int i = 0; i < mosaicTileItem->getHeight(); i++)
    {
        for (int j = 0; j < mosaicTileItem->getWidth(); j++)
        {
            MosaicTile::OneTile * const tile = mosaicTileItem->getTile(i, j);
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Tile #" << i * mosaicTileItem->getWidth() + j << "Center:" << tile->center;

            Job ts;
            ts.center.setRA0(tile->skyCenter.ra0().Hours());
            ts.center.setDec0(tile->skyCenter.dec0().Degrees());
            ts.rotation = -mosaicTileItem->getPA();

            ts.doAlign =
                    (0 < ui->alignEvery->value()) &&
                    (0 == ((j+i*mosaicTileItem->getHeight()) % ui->alignEvery->value()));

            ts.doFocus =
                    (0 < ui->focusEvery->value()) &&
                    (0 == ((j+i*mosaicTileItem->getHeight()) % ui->focusEvery->value()));

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
