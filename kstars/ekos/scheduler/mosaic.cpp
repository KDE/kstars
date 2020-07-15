/*  Ekos Mosaic Tool
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "mosaic.h"

#include "kstars.h"
#include "Options.h"
#include "scheduler.h"
#include "skymap.h"
#include "ekos/manager.h"
#include "projections/projector.h"

#include "ekos_scheduler_debug.h"

namespace Ekos
{
MosaicTile::MosaicTile()
{
    brush.setStyle(Qt::NoBrush);
    pen.setColor(Qt::red);
    pen.setWidth(1);

    textBrush.setStyle(Qt::SolidPattern);
    textPen.setColor(Qt::blue);
    textPen.setWidth(2);

    setFlags(QGraphicsItem::ItemIsMovable);
}

MosaicTile::~MosaicTile()
{
    qDeleteAll(tiles);
}

QRectF MosaicTile::boundingRect() const
{
    return QRectF(0, 0, w * fovW, h * fovH);
}

void MosaicTile::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (w == 1 && h == 1)
        return;

    QFont defaultFont = painter->font();
    defaultFont.setPointSize(50);
    painter->setFont(defaultFont);

    //double center_x = (w*fovW)/2.0;
    //double center_y = (h*fovH)/2.0;

    //double center_x = parentItem()->boundingRect().center().x();
    // double center_y = parentItem()->boundingRect().center().y();

    //    painter->save();
    // QTransform transform;
    //transform.translate(center_x, center_y);
    //transform.rotate(pa);
    //transform.translate(-center_x, -center_y);
    //    transform.translate(0, 0);

    //painter->setTransform(transform);

    for (int row = 0; row < h; row++)
    {
        for (int col = 0; col < w; col++)
        {
            OneTile *tile = getTile(row, col);
            QRect oneRect(tile->pos.x(), tile->pos.y(), fovW, fovH);

            painter->setPen(pen);
            painter->setBrush(brush);

            painter->drawRect(oneRect);
            painter->setBrush(textBrush);
            painter->drawText(oneRect, Qt::AlignHCenter | Qt::AlignVCenter, QString("%1").arg(row * w + col + 1));

            //painter->restore();
        }
    }

    //setRotation(pa);
}
QList<OneTile *> MosaicTile::getTiles() const
{
    return tiles;
}

OneTile *MosaicTile::getTile(int row, int col)
{
    int offset = row * w + col;

    if (offset < 0 || offset > tiles.size())
        return nullptr;

    return tiles[offset];
}

void MosaicTile::updateTiles()
{
    qDeleteAll(tiles);
    tiles.clear();
    double xOffset = fovW - fovW * overlap / 100.0;
    double yOffset = fovH - fovH * overlap / 100.0;

    double initX = fovW * overlap / 100.0 * (w - 1) / 2.0;
    double initY = fovH * overlap / 100.0 * (h - 1) / 2.0;

    double x = initX, y = initY;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Mosaic Tile FovW" << fovW << "FovH" << fovH << "initX" << x << "initY" << y <<
                                   "Offset X " << xOffset << " Y " << yOffset;

    for (int row = 0; row < h; row++)
    {
        x = initX;

        for (int col = 0; col < w; col++)
        {
            OneTile *tile = new OneTile();
            tile->pos.setX(x);
            tile->pos.setY(y);

            tile->center.setX(tile->pos.x() + (fovW / 2.0));
            tile->center.setY(tile->pos.y() + (fovH / 2.0));

            tiles.append(tile);

            x += xOffset;
        }

        y += yOffset;
    }

    double width  = (w * fovW - ((w - 1) * fovW * overlap / 100.0)) + initX * 2;
    double height = (h * fovH - ((h - 1) * fovH * overlap / 100.0)) + initY * 2;

    QPointF centerPoint(width / 2.0, height / 2.0);

    for (int row = 0; row < h; row++)
    {
        for (int col = 0; col < w; col++)
        {
            OneTile *tile    = getTile(row, col);
            tile->center_rot = rotatePoint(tile->center, centerPoint);
        }
    }
}

QPointF MosaicTile::rotatePoint(QPointF pointToRotate, QPointF centerPoint)
{
    double angleInRadians = pa * dms::DegToRad;
    double cosTheta       = cos(angleInRadians);
    double sinTheta       = sin(angleInRadians);

    QPointF rotation_point;

    rotation_point.setX((cosTheta * (pointToRotate.x() - centerPoint.x()) -
                         sinTheta * (pointToRotate.y() - centerPoint.y()) + centerPoint.x()));
    rotation_point.setY((sinTheta * (pointToRotate.x() - centerPoint.x()) +
                         cosTheta * (pointToRotate.y() - centerPoint.y()) + centerPoint.y()));

    return rotation_point;
}

Mosaic::Mosaic()
{
    setupUi(this);

    focalLenSpin->setValue(Options::telescopeFocalLength());
    pixelWSizeSpin->setValue(Options::cameraPixelWidth());
    pixelHSizeSpin->setValue(Options::cameraPixelHeight());
    cameraWSpin->setValue(Options::cameraWidth());
    cameraHSpin->setValue(Options::cameraHeight());
    rotationSpin->setValue(Options::cameraRotation());

    jobsDir->setText(QDir::homePath());

    connect(updateB, SIGNAL(clicked()), this, SLOT(constructMosaic()));
    connect(resetB, SIGNAL(clicked()), this, SLOT(resetFOV()));
    connect(createJobsB, SIGNAL(clicked()), this, SLOT(createJobs()));
    connect(selectJobsDirB, SIGNAL(clicked()), this, SLOT(saveJobsDirectory()));

    skyMapItem = scene.addPixmap(targetPix);
    mosaicTileItem = new MosaicTile();
    scene.addItem(mosaicTileItem);
    mosaicView->setScene(&scene);

    selectJobsDirB->setIcon(
        QIcon::fromTheme("document-open-folder"));

    connect(fetchB, &QPushButton::clicked, this, &Mosaic::fetchINDIInformation);

    //mosaicView->setResizeAnchor(QGraphicsView::AnchorViewCenter);

    // scene.addItem(mosaicTile);

    rememberAltAzOption = Options::useAltAz();

    // Always use Equatorial Mode in Mosaic mode
    Options::setUseAltAz(false);
}

Mosaic::~Mosaic()
{
    Options::setUseAltAz(rememberAltAzOption);
}

void Mosaic::saveJobsDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(KStars::Instance(), i18n("FITS Save Directory"), jobsDir->text());

    if (!dir.isEmpty())
        jobsDir->setText(dir);
}

void Mosaic::setCenter(const SkyPoint &value)
{
    center = value;
    center.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());
}

void Mosaic::setCameraSize(uint16_t width, uint16_t height)
{
    cameraWSpin->setValue(width);
    cameraHSpin->setValue(height);
}

void Mosaic::setPixelSize(double pixelWSize, double pixelHSize)
{
    pixelWSizeSpin->setValue(pixelWSize);
    pixelHSizeSpin->setValue(pixelHSize);
}

void Mosaic::setFocalLength(double focalLength)
{
    focalLenSpin->setValue(focalLength);
}

void Mosaic::calculateFOV()
{
    if (cameraWSpin->value() == 0 || cameraHSpin->value() == 0 || pixelWSizeSpin->value() == 0 ||
            pixelHSizeSpin->value() == 0 || focalLenSpin->value() == 0)
        return;

    fovGroup->setEnabled(true);

    targetWFOVSpin->setMinimum(cameraWFOVSpin->value());
    targetHFOVSpin->setMinimum(cameraHFOVSpin->value());

    Options::setTelescopeFocalLength(focalLenSpin->value());
    Options::setCameraPixelWidth(pixelWSizeSpin->value());
    Options::setCameraPixelHeight(pixelHSizeSpin->value());
    Options::setCameraWidth(cameraWSpin->value());
    Options::setCameraHeight(cameraHSpin->value());

    // Calculate FOV in arcmins
    double fov_x =
        206264.8062470963552 * cameraWSpin->value() * pixelWSizeSpin->value() / 60000.0 / focalLenSpin->value();
    double fov_y =
        206264.8062470963552 * cameraHSpin->value() * pixelHSizeSpin->value() / 60000.0 / focalLenSpin->value();

    cameraWFOVSpin->setValue(fov_x);
    cameraHFOVSpin->setValue(fov_y);

    if (targetWFOVSpin->value() < fov_x)
        targetWFOVSpin->setValue(fov_x);

    if (targetHFOVSpin->value() < fov_y)
        targetHFOVSpin->setValue(fov_y);

    updateTargetFOV();
}

void Mosaic::updateTargetFOV()
{
    KStars *ks  = KStars::Instance();
    SkyMap *map = SkyMap::Instance();

    double oldZoomFactor = Options::zoomFactor();

    // Get 2x the target FOV
    ks->setApproxFOV(targetWFOVSpin->value() * 5 / 60.0);

    center.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

    //map->setFocusObject(nullptr);
    //map->setFocusPoint(&center);
    map->setClickedObject(nullptr);
    map->setClickedPoint(&center);
    map->slotCenter();
    qApp->processEvents();

    pixelsPerArcminRA = (Options::zoomFactor() * dms::DegToRad / 60.0);
    pixelsPerArcminDE = Options::zoomFactor() * dms::DegToRad / 60.0;

    double fov_w = map->width() / pixelsPerArcminRA;
    double fov_h = map->height() / pixelsPerArcminDE;

    // 150% of desired FOV so we get extra space for rotations
    double x = (fov_w - targetWFOVSpin->value() * 2) / 2 * pixelsPerArcminRA;
    double y = (fov_h - targetHFOVSpin->value() * 2) / 2 * pixelsPerArcminDE;
    double w = targetWFOVSpin->value() * 2 * pixelsPerArcminRA;
    double h = targetHFOVSpin->value() * 2 * pixelsPerArcminDE;

    // Get the sky map image
    if (m_skyChart)
        delete m_skyChart;
    QImage *fullSkyChart = new QImage(QSize(map->width(), map->height()), QImage::Format_RGB32);

    map->exportSkyImage(fullSkyChart, false);

    qApp->processEvents();

    m_skyChart = new QImage(fullSkyChart->copy(x, y, w, h));
    delete fullSkyChart;

    // Reset the sky-map
    map->setZoomFactor(oldZoomFactor);

    targetPix = QPixmap::fromImage(*m_skyChart);
    skyMapItem->setPixmap(targetPix);

    scene.setSceneRect(skyMapItem->boundingRect());

    // Center tile
    mosaicTileItem->setPos(skyMapItem->mapToScene(QPointF( mosaicWSpin->value()*cameraWFOVSpin->value()*pixelsPerArcminRA / 2,
                           mosaicHSpin->value()*cameraHFOVSpin->value()*pixelsPerArcminDE / 2)));
}

void Mosaic::render()
{
    if (m_skyChart == nullptr)
        return;

    mosaicTileItem->setTransformOriginPoint(mosaicTileItem->boundingRect().center());
    mosaicTileItem->setRotation(mosaicTileItem->getPA());

    mosaicView->show();
}

void Mosaic::resizeEvent(QResizeEvent *)
{
    //QRectF bounds = scene.itemsBoundingRect();
    QRectF bounds = skyMapItem->boundingRect();
    bounds.setWidth(bounds.width() * 1.1);
    bounds.setHeight(bounds.height() * 1.1);
    bounds.setWidth(bounds.width());
    bounds.setHeight(bounds.height());
    mosaicView->fitInView(bounds, Qt::KeepAspectRatio);
    mosaicView->centerOn(0, 0);
}

void Mosaic::showEvent(QShowEvent *)
{
    //QRectF bounds = scene.itemsBoundingRect();
    QRectF bounds = skyMapItem->boundingRect();
    bounds.setWidth(bounds.width() * 1.1);
    bounds.setHeight(bounds.height() * 1.1);
    bounds.setWidth(bounds.width());
    bounds.setHeight(bounds.height());
    mosaicView->fitInView(bounds, Qt::KeepAspectRatio);
    mosaicView->centerOn(0, 0);
}

void Mosaic::resetFOV()
{
    targetWFOVSpin->setValue(cameraWFOVSpin->value());
    targetHFOVSpin->setValue(cameraHFOVSpin->value());

    updateTargetFOV();
}

void Mosaic::constructMosaic()
{
    if (focalLenSpin->value() == 0 || cameraWSpin->value() == 0 || pixelHSizeSpin->value() == 0)
        return;

    //if (cameraWFOVSpin->value() == 0)
    //{
    calculateFOV();
    showEvent(nullptr);
    //}

    if (mosaicWSpin->value() > 1 || mosaicHSpin->value() > 1)
        createJobsB->setEnabled(true);

    if (mosaicTileItem->getWidth() != mosaicWSpin->value() || mosaicTileItem->getHeight() != mosaicHSpin->value() ||
            mosaicTileItem->getOverlap() != overlapSpin->value() || mosaicTileItem->getPA() != rotationSpin->value())
    {
        if (mosaicTileItem->getPA() != rotationSpin->value())
            Options::setCameraRotation(rotationSpin->value());

        // Update target FOV value
        targetWFOVSpin->setValue(cameraWFOVSpin->value() * mosaicWSpin->value());
        targetHFOVSpin->setValue(cameraHFOVSpin->value() * mosaicHSpin->value());

        updateTargetFOV();

        qCDebug(KSTARS_EKOS_SCHEDULER) << "Tile FOV in pixels W:" << cameraWFOVSpin->value() * pixelsPerArcminRA << "H:"
                                       << cameraHFOVSpin->value() * pixelsPerArcminDE;

        mosaicTileItem->setDimension(mosaicWSpin->value(), mosaicHSpin->value());
        mosaicTileItem->setPA(rotationSpin->value());
        mosaicTileItem->setFOV(cameraWFOVSpin->value() * pixelsPerArcminRA, cameraHFOVSpin->value() * pixelsPerArcminDE);
        mosaicTileItem->setOverlap(overlapSpin->value());
        mosaicTileItem->updateTiles();

        //render();
        mosaicTileItem->setTransformOriginPoint(mosaicTileItem->boundingRect().center());
        mosaicTileItem->setRotation(mosaicTileItem->getPA());

        jobCountSpin->setValue(mosaicTileItem->getWidth() * mosaicTileItem->getHeight());
    }
}

void Mosaic::createJobs()
{
    QPointF skymapCenterPoint = skyMapItem->boundingRect().center();

    /*
    QPointF mosaicCenterPoint = mosaicTileItem->mapToItem(skyMapItem, mosaicTileItem->boundingRect().center());

    QPointF diffFromSkyMapCenter = skymapCenterPoint - mosaicCenterPoint;

    qCDebug(KSTARS_EKOS_SCHEDULER) << "#1 Center RA:" << center.ra0().toHMSString() << "DE:" << center.dec0().toDMSString();

    center.setRA0( (center.ra0().Degrees() + (diffFromSkyMapCenter.x() / (pixelsPerArcmin * 60.0))) / 15.0);
    center.setDec0(center.dec0().Degrees() + (diffFromSkyMapCenter.y() / (pixelsPerArcmin * 60.0)));

    qCDebug(KSTARS_EKOS_SCHEDULER) << "#2 Center RA:" << center.ra0().toHMSString() << "DE:" << center.dec0().toDMSString();

    center.apparentCoord(static_cast<long double>J2000, KStars::Instance()->data()->ut().djd());

    KStars::Instance()->setApproxFOV(targetWFOVSpin->value() * 2 / 60.0);

    center.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

    SkyMap *map = SkyMap::Instance();

    map->setFocusObject(nullptr);
    map->setClickedPoint(&center);
    map->slotCenter();
    qApp->processEvents();

    screenPoint    = map->projector()->toScreen(&center);
    qCDebug(KSTARS_EKOS_SCHEDULER) << "Target Screen X:" << screenPoint.x() << "Y:" << screenPoint.y();

    */

    /*for (int i = 0; i < mosaicTileItem->getHeight(); i++)
    {
        for (int j = 0; j < mosaicTileItem->getWidth(); j++)
        {
            OneTile *tile = mosaicTileItem->getTile(i, j);
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Tile #" << i * mosaicTileItem->getWidth() + j << "X:" << tile->center.x() << "Y:"
                     << tile->center.y();
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Scene coords:" << mosaicTileItem->mapToScene(tile->center);
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Rotated coords:" << tile->center_rot;

            QPointF tileScreenPoint(screenPoint.x() - skyMapItem->boundingRect().width() / 2.0 + tile->center_rot.x(),
                                    screenPoint.y() - skyMapItem->boundingRect().height() / 2.0 + tile->center_rot.y());

            tile->skyCenter = map->projector()->fromScreen(tileScreenPoint, KStarsData::Instance()->lst(),
                                                           KStarsData::Instance()->geo()->lat());
            tile->skyCenter.deprecess(KStarsData::Instance()->updateNum());

            // TODO Check if J2000 are VALID. If they are use them to generate the jobs
            // Add directory to save job file along with sequence files
            // Add target name + part # as the prefix, and also the directory
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Coords are J2000 RA:" << tile->skyCenter.ra0().toHMSString() << "DE:"
                     << tile->skyCenter.dec0().toDMSString();
        }
    }*/

    qCDebug(KSTARS_EKOS_SCHEDULER) << "Mosaic Tile W:" << mosaicTileItem->boundingRect().width() << "H:" <<
                                   mosaicTileItem->boundingRect().height();

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
            OneTile *tile = mosaicTileItem->getTile(i, j);
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Tile #" << i * mosaicTileItem->getWidth() + j << "Center:" << tile->center <<
                                           "Rot Center:" << tile->center_rot;

            QPointF tileCenterPoint = mosaicTileItem->mapToItem(skyMapItem, tile->center);

            QPointF diffFromSkyMapCenter = skymapCenterPoint - tileCenterPoint;

            tile->skyCenter.setRA0( (center.ra0().Degrees() + (diffFromSkyMapCenter.x() / (pixelsPerArcminRA * cos(
                                         center.dec0().radians()) * 60.0))) / 15.0);
            tile->skyCenter.setDec0( center.dec0().Degrees() + (diffFromSkyMapCenter.y() / (pixelsPerArcminDE * 60.0)));

            qCDebug(KSTARS_EKOS_SCHEDULER) << "Tile RA0:" << tile->skyCenter.ra0().toHMSString() << "DE0:" <<
                                           tile->skyCenter.dec0().toDMSString();
        }
    }

    accept();
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
        QList<double> values = cameraReply.value();

        cameraWSpin->setValue(values[0]);
        cameraHSpin->setValue(values[1]);
        pixelWSizeSpin->setValue(values[2]);
        pixelHSizeSpin->setValue(values[3]);
    }

    QDBusReply<QList<double>> telescopeReply = alignInterface.call("telescopeInfo");
    if (telescopeReply.isValid())
    {
        QList<double> values = telescopeReply.value();
        focalLenSpin->setValue(values[0]);
    }

    QDBusReply<QList<double>> solutionReply = alignInterface.call("getSolutionResult");
    if (solutionReply.isValid())
    {
        QList<double> values = solutionReply.value();
        if (values[0] > INVALID_VALUE)
            rotationSpin->setValue(values[0]);
    }

    constructMosaic();
}

}
