/*  Ekos Mosaic Tool
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "mosaic.h"

#include "skymap.h"
#include "projections/projector.h"

#include "scheduler.h"
#include "ekosmanager.h"


#include "Options.h"

namespace Ekos
{

MosaicTile::MosaicTile()
{
    w=h=1;
    fovW=fovH=pa=0;

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
    return QRectF(0, 0, w*fovW, h*fovH);
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



    for (int row=0; row < h; row++)
    {
        for (int col=0; col < w; col++)
        {
            OneTile *tile = getTile(row, col);
            QRect oneRect(tile->pos.x(), tile->pos.y(), fovW, fovH);

            painter->setPen(pen);
            painter->setBrush(brush);



            painter->drawRect(oneRect);
            painter->setBrush(textBrush);
            painter->drawText(oneRect, Qt::AlignHCenter | Qt::AlignVCenter, QString("%1").arg(row*w+col+1));

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
        return NULL;

    return tiles[offset];
}

void MosaicTile::updateTiles()
{
    qDeleteAll(tiles);
    tiles.clear();
    double xOffset=fovW - fovW*overlap/100.0;
    double yOffset=fovH - fovH*overlap/100.0;

    double initX=fovW*overlap/100.0*(w-1) / 2.0;
    double initY=fovH*overlap/100.0*(h-1) / 2.0;

    double x= initX, y= initY;

    qDebug() << "FovW " << fovW << " FovH " << fovH;
    qDebug() << "initX" << "initX " << initX << " initY " << initY;
    qDebug() << "Offset X " << xOffset << " Y " << yOffset;

    for (int row=0; row < h; row++)
    {
        x = initX;

        for (int col=0; col < w; col++)
        {
            OneTile *tile = new OneTile();
            tile->pos.setX(x);
            tile->pos.setY(y);

            tile->center.setX(tile->pos.x() + (fovW/2.0));
            tile->center.setY(tile->pos.y() + (fovH/2.0));

            tiles.append(tile);

            x += xOffset;

        }

        y += yOffset;
    }

    double width  = (w*fovW - ( (w-1) * fovW*overlap/100.0)) + initX*2;
    double height = (h*fovH - ( (h-1) * fovH*overlap/100.0)) + initY*2;

    QPointF centerPoint(width/2.0, height/2.0);

    for (int row=0; row < h; row++)
    {
        for (int col=0; col < w; col++)
        {
                  OneTile *tile = getTile(row, col);
                  tile->center_rot = rotatePoint(tile->center, centerPoint);
        }
    }

}

QPointF MosaicTile::rotatePoint(QPointF pointToRotate, QPointF centerPoint)
{
    double angleInRadians = pa * dms::DegToRad;
    double cosTheta = cos(angleInRadians);
    double sinTheta = sin(angleInRadians);

    QPointF rotation_point;

    rotation_point.setX((cosTheta * (pointToRotate.x() - centerPoint.x()) - sinTheta * (pointToRotate.y() - centerPoint.y()) + centerPoint.x()));
    rotation_point.setY((sinTheta * (pointToRotate.x() - centerPoint.x()) + cosTheta * (pointToRotate.y() - centerPoint.y()) + centerPoint.y()));

    return rotation_point;
}

QPointF MosaicTile::getTileCenter(int row, int col)
{
    OneTile *tile = getTile(row, col);
    if (tile == NULL)
        return QPointF();
    else
        return tile->center_rot;
}

Mosaic::Mosaic(Scheduler *scheduler)
{
    setupUi(this);

    m_skyChart = 0;
    m_skyImage = 0;

    ekosScheduler = scheduler;

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

    targetItem = scene.addPixmap(targetPix);
    mosaicTile = new MosaicTile();
    scene.addItem(mosaicTile);
    mosaicView->setScene(&scene);

    selectJobsDirB->setIcon(QIcon::fromTheme("document-open-folder"));

    //mosaicView->setResizeAnchor(QGraphicsView::AnchorViewCenter);


   // scene.addItem(mosaicTile);

}

Mosaic::~Mosaic()
{

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
    center.apparentCoord((long double) J2000, KStars::Instance()->data()->ut().djd());
}

void Mosaic::setCameraSize(uint16_t width, uint16_t height)
{
    cameraWSpin->setValue(width);
    cameraHSpin->setValue(height);
}

void  Mosaic::setPixelSize(double pixelWSize, double pixelHSize)
{
    pixelWSizeSpin->setValue(pixelWSize);
    pixelHSizeSpin->setValue(pixelHSize);
}

void  Mosaic::setFocalLength(double focalLength)
{
    focalLenSpin->setValue(focalLength);
}

void Mosaic::calculateFOV()
{
    if (cameraWSpin->value() == 0 || cameraHSpin->value() == 0 || pixelWSizeSpin->value() == 0 || pixelHSizeSpin->value() == 0 || focalLenSpin->value() == 0)
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
    double fov_x = 206264.8062470963552 * cameraWSpin->value() * pixelWSizeSpin->value() / 60000.0 / focalLenSpin->value();
    double fov_y = 206264.8062470963552 * cameraHSpin->value() * pixelHSizeSpin->value() / 60000.0 / focalLenSpin->value();

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
    KStars *ks = KStars::Instance();
    SkyMap *map = SkyMap::Instance();

    // Save the current state of the sky map
    //SkyPoint *oldFocus = map->focus();
    double oldZoomFactor = Options::zoomFactor();


    // Get double the target FOV
    ks->setApproxFOV( targetWFOVSpin->value()*2 / 60.0 );
    //    map->setFocus( sp ); // FIXME: Why does setFocus() need a non-const SkyPoint pointer?

    center.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

    map->setFocusObject(NULL);
    map->setClickedPoint(&center);
    map->slotCenter();
    qApp->processEvents();

    // Repeat -- dirty workaround for some problem in KStars

   /* map->setClickedPoint(&center);
    map->slotCenter();
    qApp->processEvents();*/

    pixelPerArcmin = Options::zoomFactor() * dms::DegToRad / 60.0;

    double fov_w = map->width()  / pixelPerArcmin;
    double fov_h = map->height() / pixelPerArcmin;

    double x = (fov_w - targetWFOVSpin->value()) / 2 * pixelPerArcmin;
    double y = (fov_h - targetHFOVSpin->value()) / 2 * pixelPerArcmin;
    double w = targetWFOVSpin->value() * pixelPerArcmin;
    double h = targetHFOVSpin->value() * pixelPerArcmin;

    // Get the sky map image
    if( m_skyChart )
        delete m_skyChart;
    QImage *fullSkyChart = new QImage( QSize( map->width(), map->height() ), QImage::Format_RGB32 );
    map->exportSkyImage( fullSkyChart, false ); // FIXME: This might make everything too small, should really use a mask and clip it down!
    qApp->processEvents();
    //m_skyChart = new QImage( fullSkyChart->copy( map->width()/2.0 - map->width()/8.0, map->height()/2.0 - map->height()/8.0, map->width()/4.0, map->height()/4.0 ) );
    m_skyChart = new QImage( fullSkyChart->copy(x,y,w,h));
    delete fullSkyChart;

    // See if we were supplied a sky image; if so, show it.
    /*if( ! imagePath.isEmpty() ) {
        if( m_skyImage )
            delete m_skyImage;
        m_skyImage = new QImage( imagePath );
        m_skyImageDisplay->setVisible( true );
    }
    else
        m_skyImageDisplay->setVisible( false );*/


    // Reset the sky-map
    map->setZoomFactor( oldZoomFactor );
    /*map->setClickedPoint( oldFocus );
    map->slotCenter();
    map->forceUpdate();
    qApp->processEvents();

    // Repeat -- dirty workaround for some problem in KStars
    map->setClickedPoint( oldFocus );
    map->slotCenter();
    qApp->processEvents();*/



    // Render the display
    //render();

     targetPix = QPixmap::fromImage(*m_skyChart);
     targetItem->setPixmap(targetPix);

    // qDebug() << "Pixmap w " << targetPix.width() << " h " << targetPix.height();
    // QRectF bound = (targetItem->boundingRect());
     scene.setSceneRect(targetItem->boundingRect());
   // mosaicView->fitInView(targetItem, Qt::KeepAspectRatio);
    // mosaicView->resize(targetItem->boundingRect().width(), targetItem->boundingRect().height());



}

void Mosaic::render()
{
    if (m_skyChart == NULL)
        return;

   // targetPix = QPixmap::fromImage( *m_skyChart ).scaled( mosaicView->width(), mosaicView->height(), Qt::KeepAspectRatio );
   // targetItem->setPixmap(targetPix);

    //targetPix = QPixmap::fromImage( *m_skyChart ).scaled( mosaicView->width(), mosaicView->height(), Qt::KeepAspectRatio );
    //targetItem->setPixmap(targetPix);

    //double scale_x = (double) targetPix.width() / (double) m_skyChart->width();
    //double scale_y = (double) targetPix.height() / (double) m_skyChart->height();

   // mosaicTile->moveBy(center_x, center_y);

    //QPointF tile1Center = mosaicTile->scenePos();
    mosaicTile->setTransformOriginPoint(mosaicTile->boundingRect().center());
    mosaicTile->setRotation(mosaicTile->getPA());

    //QPointF tile1Center2 = mosaicTile->scenePos();
//    mosaicView->scale(scale_x, scale_y);


    mosaicView->show();

    //resizeEvent(NULL);



    /*scene.addPixmap(QPixmap::fromImage( *m_skyChart ).scaled( mosaicView->width(), mosaicView->height(), Qt::KeepAspectRatio ) );
    mosaicView->setScene(&scene);
    mosaicView->show();*/
    //mosaicDisplay->setPixmap( QPixmap::fromImage( *m_skyChart ).scaled( mosaicDisplay->width(), mosaicDisplay->height(), Qt::KeepAspectRatio ) );


}

void Mosaic::resizeEvent(QResizeEvent *)
{
       //QRectF bounds = scene.itemsBoundingRect();
        QRectF bounds = targetItem->boundingRect();
       bounds.setWidth(bounds.width()*1.1);
       bounds.setHeight(bounds.height()*1.1);
       bounds.setWidth(bounds.width());
       bounds.setHeight(bounds.height());
       mosaicView->fitInView(bounds, Qt::KeepAspectRatio);
       mosaicView->centerOn(0, 0);

}

void Mosaic::showEvent(QShowEvent *)
{
    //QRectF bounds = scene.itemsBoundingRect();
    QRectF bounds = targetItem->boundingRect();
    bounds.setWidth(bounds.width()*1.1);
    bounds.setHeight(bounds.height()*1.1);
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
    if (mosaicWSpin->value() > 1 || mosaicHSpin->value() > 1)
        createJobsB->setEnabled(true);

    if (mosaicTile->getWidth() != mosaicWSpin->value() || mosaicTile->getHeight() != mosaicHSpin->value() || mosaicTile->getOverlap() != overlapSpin->value() || mosaicTile->getPA() != rotationSpin->value())    
    {
        if (mosaicTile->getPA() != rotationSpin->value())
            Options::setCameraRotation(rotationSpin->value());

        // Update target FOV value
        targetWFOVSpin->setValue(cameraWFOVSpin->value() * mosaicWSpin->value());
        targetHFOVSpin->setValue(cameraHFOVSpin->value() * mosaicHSpin->value());

        updateTargetFOV();

        qDebug() << "Tile FOV in pixels is WIDTH " << cameraWFOVSpin->value()*pixelPerArcmin << " HEIGHT " << cameraHFOVSpin->value()*pixelPerArcmin;

        mosaicTile->setDimension(mosaicWSpin->value(), mosaicHSpin->value());
        mosaicTile->setPA(rotationSpin->value());
        mosaicTile->setFOV(cameraWFOVSpin->value()*pixelPerArcmin, cameraHFOVSpin->value()*pixelPerArcmin);
        mosaicTile->setOverlap(overlapSpin->value());
        mosaicTile->updateTiles();

        //render();
        mosaicTile->setTransformOriginPoint(mosaicTile->boundingRect().center());
        mosaicTile->setRotation(mosaicTile->getPA());

        jobCountSpin->setValue(mosaicTile->getWidth()*mosaicTile->getHeight());
    }
}

void Mosaic::createJobs()
{
    qDebug() << "Target Item X " << targetItem->x() << " Y " << targetItem->y() << " Width " << targetItem->boundingRect().width() << " height " << targetItem->boundingRect().height();

    qDebug() << "Target SCENE Item X " << targetItem->mapToScene(targetItem->x(), targetItem->y()).x() << " Y " << targetItem->mapToScene(targetItem->x(), targetItem->y()).y();

    qDebug() << "Mosaic Item X " << mosaicTile->x() << " Y " << mosaicTile->y() << " Width " << mosaicTile->boundingRect().width() << " height " << mosaicTile->boundingRect().height();

    qDebug() << "Mosaic SCENE Item X " << mosaicTile->mapToScene(0,0).x() << " Y " << mosaicTile->mapToScene(0,0).y();

    qDebug() << "Center RA" << center.ra().toHMSString() << " DEC " << center.dec().toDMSString();

    KStars::Instance()->setApproxFOV( targetWFOVSpin->value()*2 / 60.0 );

    center.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

    SkyMap *map = SkyMap::Instance();

    map->setFocusObject(NULL);
    map->setClickedPoint(&center);
    map->slotCenter();
    qApp->processEvents();

    screenPoint = map->projector()->toScreen(&center);
    double northPA = map->projector()->findNorthPA(&center, screenPoint.x(), screenPoint.y());

    qDebug() << "North PA " << northPA;
    qDebug() << "Target Screen X " << screenPoint.x() << " Y " << screenPoint.y();

    for (int i=0; i < mosaicTile->getHeight(); i++)
    {

        for (int j=0; j < mosaicTile->getWidth(); j++)
        {
            OneTile *tile = mosaicTile->getTile(i, j);
            qDebug() << "Tile # " << i*mosaicTile->getWidth() + j << " X " << tile->center.x() << " Y " << tile->center.y() << endl;
            qDebug() << "Scene coords" << mosaicTile->mapToScene(tile->center);
            qDebug() << "Rotated coords" << tile->center_rot;

            QPointF tileScreenPoint(screenPoint.x()-targetItem->boundingRect().width()/2.0 + tile->center_rot.x(), screenPoint.y()-targetItem->boundingRect().height()/2.0 + tile->center_rot.y());

            tile->skyCenter = map->projector()->fromScreen(tileScreenPoint, KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
            tile->skyCenter.deprecess(KStarsData::Instance()->updateNum());

            // TODO Check if J2000 are VALID. If they are use them to generate the jobs
            // Add directory to save job file along with sequence files
            // Add target name + part # as the prefix, and also the directory
            qDebug() << "Coords are J2000 RA " <<  tile->skyCenter.ra0().toHMSString() << " DEC " <<  tile->skyCenter.dec0().toDMSString();

        }
    }

    accept();
}

QPointF Mosaic::rotatePoint(QPointF pointToRotate, QPointF centerPoint)
{
    double angleInRadians = rotationSpin->value() * dms::DegToRad;
    double cosTheta = cos(angleInRadians);
    double sinTheta = sin(angleInRadians);

    QPointF rotation_point;

    rotation_point.setX((cosTheta * (pointToRotate.x() - centerPoint.x()) - sinTheta * (pointToRotate.y() - centerPoint.y()) + centerPoint.x()));
    rotation_point.setY((sinTheta * (pointToRotate.x() - centerPoint.x()) + cosTheta * (pointToRotate.y() - centerPoint.y()) + centerPoint.y()));

    return rotation_point;
}

}

