/*  Ekos Mosaic Tool
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "mosaic.h"

#include "skymap.h"

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
    pen.setWidth(5);

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

void MosaicTile::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
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
            QRect oneRect(tile->x, tile->y, fovW, fovH);

            painter->setPen(pen);
            painter->setBrush(brush);



            painter->drawRect(oneRect);
            painter->setBrush(textBrush);
            painter->drawText(oneRect, Qt::AlignHCenter | Qt::AlignVCenter, QString("%1").arg(row*w+col+1));

            //painter->restore();
        }
    }

    int finalW = w*fovW - w*(fovW*(overlap/100));
    int finalH = h*fovH - h*(fovH*(overlap/100));


   //setRotation(pa);

}

MosaicTile::OneTile *MosaicTile::getTile(int row, int col)
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

    int initX=fovW*overlap/100.0*(w-1) / 2.0;
    int initY=fovH*overlap/100.0*(h-1) / 2.0;

    int x= initX, y= initY;

    for (int row=0; row < h; row++)
    {
        x = initX;

        for (int col=0; col < w; col++)
        {
            OneTile *tile = new OneTile();
            tile->x = x;
            tile->y = y;

            tiles.append(tile);

            x += xOffset;
        }

        y += yOffset;
    }

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

    /*connect(focalLenSpin, SIGNAL(editingFinished()), this, SLOT(calculateFOV()));
    connect(pixelWSizeSpin, SIGNAL(editingFinished()), this, SLOT(calculateFOV()));
    connect(pixelHSizeSpin, SIGNAL(editingFinished()), this, SLOT(calculateFOV()));
    connect(cameraWSpin, SIGNAL(valueChanged(int)), this, SLOT(calculateFOV()));
    connect(cameraHSpin, SIGNAL(valueChanged(int)), this, SLOT(calculateFOV()));

    connect(targetWFOVSpin, SIGNAL(editingFinished()), this, SLOT(drawTargetFOV()));
    connect(targetHFOVSpin, SIGNAL(editingFinished()), this, SLOT(drawTargetFOV()));*/

    connect(updateB, SIGNAL(clicked()), this, SLOT(constructMosaic()));
    connect(resetB, SIGNAL(clicked()), this, SLOT(resetFOV()));

    targetItem = scene.addPixmap(targetPix);
    mosaicTile = new MosaicTile();
    scene.addItem(mosaicTile);
    mosaicView->setScene(&scene);

    //mosaicView->setResizeAnchor(QGraphicsView::AnchorViewCenter);


   // scene.addItem(mosaicTile);

}

Mosaic::~Mosaic()
{

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

    double scale_x = (double) targetPix.width() / (double) m_skyChart->width();
    double scale_y = (double) targetPix.height() / (double) m_skyChart->height();

    double scale = sqrt(scale_x*scale_x + scale_y*scale_y);

   // mosaicTile->moveBy(center_x, center_y);
    mosaicTile->setTransformOriginPoint(mosaicTile->boundingRect().center());
    mosaicTile->setRotation(mosaicTile->getPA());



    mosaicView->scale(scale_x, scale_y);
    mosaicView->show();



    /*scene.addPixmap(QPixmap::fromImage( *m_skyChart ).scaled( mosaicView->width(), mosaicView->height(), Qt::KeepAspectRatio ) );
    mosaicView->setScene(&scene);
    mosaicView->show();*/
    //mosaicDisplay->setPixmap( QPixmap::fromImage( *m_skyChart ).scaled( mosaicDisplay->width(), mosaicDisplay->height(), Qt::KeepAspectRatio ) );


}

void Mosaic::resizeEvent(QResizeEvent *ev)
{

    render();

    ev->accept();
}

void Mosaic::resetFOV()
{
    targetWFOVSpin->setValue(cameraWFOVSpin->value());
    targetHFOVSpin->setValue(cameraHFOVSpin->value());

    updateTargetFOV();
}

void Mosaic::setPreset()
{
   /* int scale=1;

    if (scale > 1)
    {
        targetWFOVSpin->setValue(cameraWFOVSpin->value()*scale);
        targetHFOVSpin->setValue(cameraHFOVSpin->value()*scale);
        drawTargetFOV();
    }*/
}

void Mosaic::createOverlay()
{
    qDeleteAll(jobCenter);


}

void Mosaic::drawOverlay()
{

}

void Mosaic::constructMosaic()
{
    if (mosaicWSpin->value() > 1 || mosaicHSpin->value() > 1)
        createJobsB->setEnabled(true);

    if (mosaicTile->getWidth() != mosaicWSpin->value() || mosaicTile->getHeight() != mosaicHSpin->value() || mosaicTile->getOverlap() != overlapSpin->value() || mosaicTile->getPA() != rotationSpin->value())
    {
        // Update target FOV value
        targetWFOVSpin->setValue(cameraWFOVSpin->value() * mosaicWSpin->value());
        targetHFOVSpin->setValue(cameraHFOVSpin->value() * mosaicHSpin->value());

        updateTargetFOV();

        mosaicTile->setDimension(mosaicWSpin->value(), mosaicHSpin->value());
        mosaicTile->setPA(rotationSpin->value());
        mosaicTile->setFOV(cameraWFOVSpin->value()*pixelPerArcmin, cameraHFOVSpin->value()*pixelPerArcmin);
        mosaicTile->setOverlap(overlapSpin->value());
        mosaicTile->updateTiles();

        render();
    }
}

}

