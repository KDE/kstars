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

    connect(updateB, SIGNAL(clicked()), this, SLOT(calculateFOV()));
    connect(resetB, SIGNAL(clicked()), this, SLOT(resetFOV()));

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

    drawTargetFOV();
}

void Mosaic::drawTargetFOV()
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

    double pixelPerArcmin = Options::zoomFactor() * dms::DegToRad / 60.0;

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
    render();
}

void Mosaic::render()
{
    mosaicDisplay->setPixmap( QPixmap::fromImage( *m_skyChart ).scaled( mosaicDisplay->width(), mosaicDisplay->height(), Qt::KeepAspectRatio ) );
}

void Mosaic::resizeEvent(QResizeEvent *ev)
{
    ev->accept();

    render();
}

void Mosaic::resetFOV()
{
    targetWFOVSpin->setValue(cameraWFOVSpin->value());
    targetHFOVSpin->setValue(cameraHFOVSpin->value());

    drawTargetFOV();
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

}
