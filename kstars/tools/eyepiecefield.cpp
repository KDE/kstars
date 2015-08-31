/***************************************************************************
                eyepiecefield.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Fri 30 May 2014 15:07:46 CDT
    copyright            : (c) 2014 by Akarsh Simha
    email                : akarsh.simha@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


/* Project Includes */
#include "eyepiecefield.h"
#include "fov.h"
#include "skypoint.h"
#include "skymap.h"
#include "kstars.h"
#include "Options.h"

/* KDE Includes */

/* Qt Includes */
#include <QString>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QImage>

EyepieceField::EyepieceField( QWidget *parent ) : QDialog( parent ) {

    setWindowTitle( i18n( "Eyepiece Field View" ) );

    QWidget *mainWidget = new QWidget( this );
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(mainWidget);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *rows = new QVBoxLayout;
    mainWidget->setLayout( rows );

    m_skyChartDisplay = new QLabel;
    m_skyChartDisplay->setBackgroundRole( QPalette::Base );
    m_skyChartDisplay->setScaledContents( false );
    m_skyChartDisplay->setMinimumWidth( 400 );

    m_skyImageDisplay = new QLabel;
    m_skyImageDisplay->setBackgroundRole( QPalette::Base );
    m_skyImageDisplay->setScaledContents( false );
    m_skyImageDisplay->setMinimumWidth( 400 );
    m_skyImageDisplay->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Expanding );

    QHBoxLayout *imageLayout = new QHBoxLayout;
    rows->addLayout( imageLayout );
    imageLayout->addWidget( m_skyChartDisplay );
    imageLayout->addWidget( m_skyImageDisplay );

    m_invertView = new QCheckBox( i18n( "Invert view" ), this );
    m_flipView = new QCheckBox( i18n( "Flip view" ), this );
    m_invertColors = new QCheckBox( i18n( "Invert colors" ), this );
    
    QHBoxLayout *optionsLayout = new QHBoxLayout;
    optionsLayout->addWidget( m_invertView );
    optionsLayout->addWidget( m_flipView );
    optionsLayout->addStretch();
    optionsLayout->addWidget( m_invertColors );

    rows->addLayout( optionsLayout );

    m_rotationSlider = new QSlider( Qt::Horizontal, this );
    m_rotationSlider->setMaximum( 180 );
    m_rotationSlider->setMinimum( -180 );
    m_rotationSlider->setTickInterval( 30 );
    m_rotationSlider->setPageStep( 30 );

    QLabel *sliderLabel = new QLabel( i18n( "Rotation: " ), this );
    
    QHBoxLayout *rotationLayout = new QHBoxLayout;
    rotationLayout->addWidget( sliderLabel );
    rotationLayout->addWidget( m_rotationSlider );

    rows->addLayout( rotationLayout );

    connect( m_invertView, SIGNAL( stateChanged( int ) ), this, SLOT( render() ) );
    connect( m_flipView, SIGNAL( stateChanged( int ) ), this, SLOT( render() ) );
    connect( m_invertColors, SIGNAL( stateChanged( int ) ), this, SLOT( render() ) );
    connect( m_rotationSlider, SIGNAL( valueChanged( int ) ), this, SLOT( render() ) );

    m_skyChart = 0;
    m_skyImage = 0;

}

void EyepieceField::showEyepieceField( SkyPoint *sp, FOV const * const fov, const QString &imagePath ) {

    SkyMap *map = SkyMap::Instance();
    KStars *ks = KStars::Instance();

    // Save the current state of the sky map
    SkyPoint *oldFocus = map->focus();
    double oldZoomFactor = Options::zoomFactor();

    Q_ASSERT( sp );


    // Set up the new sky map FOV and pointing. full map FOV = 4 times the given FOV.
    if( fov )
        ks->setApproxFOV( ( ( fov->sizeX() > fov->sizeY() ) ? fov->sizeX() : fov->sizeY() ) / 15.0 );
    else
        ks->setApproxFOV( 1*4.0 ); // Just set 1 degree anyway
    //    map->setFocus( sp ); // FIXME: Why does setFocus() need a non-const SkyPoint pointer?
    map->setClickedPoint(sp);
    map->slotCenter();
    qApp->processEvents();

    // Repeat -- dirty workaround for some problem in KStars
    map->setClickedPoint(sp);
    map->slotCenter();
    qApp->processEvents();


    // Get the sky map image
    if( m_skyChart )
        delete m_skyChart;
    QImage *fullSkyChart = new QImage( QSize( map->width(), map->height() ), QImage::Format_RGB32 );
    map->exportSkyImage( fullSkyChart, false ); // FIXME: This might make everything too small, should really use a mask and clip it down!
    qApp->processEvents();
    m_skyChart = new QImage( fullSkyChart->copy( map->width()/2.0 - map->width()/8.0, map->height()/2.0 - map->height()/8.0, map->width()/4.0, map->height()/4.0 ) );
    delete fullSkyChart;

    // See if we were supplied a sky image; if so, show it.
    if( ! imagePath.isEmpty() ) {
        if( m_skyImage )
            delete m_skyImage;
        m_skyImage = new QImage( imagePath );
        m_skyImageDisplay->setVisible( true );
    }
    else
        m_skyImageDisplay->setVisible( false );


    // Reset the sky-map
    map->setZoomFactor( oldZoomFactor );
    map->setClickedPoint( oldFocus );
    map->slotCenter();
    map->forceUpdate();
    qApp->processEvents();

    // Repeat -- dirty workaround for some problem in KStars
    map->setClickedPoint( oldFocus );
    map->slotCenter();
    qApp->processEvents();


    // Render the display
    render();
}

void EyepieceField::render() {

    QImage renderImage;
    QImage renderChart;

    QTransform transform;
    transform.rotate( m_rotationSlider->value() );
    if( m_flipView->isChecked() )
        transform.scale( -1, 1 );
    if( m_invertView->isChecked() )
        transform.scale( -1, -1 );
       
    Q_ASSERT( m_skyChart );

    renderChart = m_skyChart->transformed( transform );
    if( m_skyImage )
        renderImage = m_skyImage->transformed( transform );

    if( m_invertColors->isChecked() )
        renderImage.invertPixels();

    m_skyChartDisplay->setPixmap( QPixmap::fromImage( renderChart ).scaled( m_skyChartDisplay->width(), m_skyChartDisplay->height(), Qt::KeepAspectRatio ) );
    m_skyImageDisplay->setPixmap( QPixmap::fromImage( renderImage ).scaled( m_skyImageDisplay->width(), m_skyImageDisplay->height(), Qt::KeepAspectRatio ) );

    show();
}

EyepieceField::~EyepieceField() {
    // Empty
    delete m_skyChart;
    delete m_skyImage;
}
