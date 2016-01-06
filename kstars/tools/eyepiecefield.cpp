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
#include "skyqpainter.h"
#include "kstars.h"
#include "Options.h"
#include "ksdssimage.h"
/* KDE Includes */

/* Qt Includes */
#include <QString>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QImage>
#include <QPixmap>
#include <QBitmap>
#include <QTemporaryFile>
#include <QSvgRenderer>
#include <QSvgGenerator>

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
    m_overlay = new QCheckBox( i18n( "Overlay" ), this );
    m_invertColors = new QCheckBox( i18n( "Invert colors" ), this );

    QHBoxLayout *optionsLayout = new QHBoxLayout;
    optionsLayout->addWidget( m_invertView );
    optionsLayout->addWidget( m_flipView );
    optionsLayout->addStretch();
    optionsLayout->addWidget( m_overlay );
    optionsLayout->addWidget( m_invertColors );

    rows->addLayout( optionsLayout );

    m_rotationSlider = new QSlider( Qt::Horizontal, this );
    m_rotationSlider->setMaximum( 180 );
    m_rotationSlider->setMinimum( -180 );
    m_rotationSlider->setTickInterval( 30 );
    m_rotationSlider->setPageStep( 30 );

    QLabel *sliderLabel = new QLabel( i18n( "Rotation: " ), this );

    m_presetCombo = new QComboBox( this );
    m_presetCombo->addItem( i18n( "None" ) );
    m_presetCombo->addItem( i18n( "Vanilla" ) );
    m_presetCombo->addItem( i18n( "Flipped" ) );
    //    m_presetCombo->addItem( i18n( "Refractor" ) ); // TODO: Implement
    //    m_presetCombo->addItem( i18n( "Dobsonian" ) ); // TODO: Implement

    QLabel *presetLabel = new QLabel( i18n( "Preset: " ), this );

    QHBoxLayout *rotationLayout = new QHBoxLayout;
    rotationLayout->addWidget( sliderLabel );
    rotationLayout->addWidget( m_rotationSlider );
    rotationLayout->addWidget( presetLabel );
    rotationLayout->addWidget( m_presetCombo );

    rows->addLayout( rotationLayout );

    connect( m_invertView, SIGNAL( stateChanged( int ) ), this, SLOT( render() ) );
    connect( m_flipView, SIGNAL( stateChanged( int ) ), this, SLOT( render() ) );
    connect( m_invertColors, SIGNAL( stateChanged( int ) ), this, SLOT( render() ) );
    connect( m_overlay, SIGNAL( stateChanged( int ) ), this, SLOT( render() ) );
    connect( m_rotationSlider, SIGNAL( valueChanged( int ) ), this, SLOT( render() ) );
    connect( m_presetCombo, SIGNAL( currentIndexChanged( int ) ), this, SLOT( slotEnforcePreset( int ) ) );
    connect( m_presetCombo, SIGNAL( activated( int ) ), this, SLOT( slotEnforcePreset( int ) ) );

    m_skyChart = 0;
    m_skyImage = 0;

}

void EyepieceField::slotEnforcePreset( int index ) {
    if( index == -1 )
        index = m_presetCombo->currentIndex();
    if( index == -1 )
        index = 0;

    if( index == 0 )
        return; // Preset "None" makes no changes

    switch( index ) {
    case 1:
        // Preset vanilla
        m_rotationSlider->setValue( 0.0 ); // reset rotation
        m_invertView->setChecked( false ); // reset inversion
        m_flipView->setChecked( false ); // reset flip
        break;
    case 2:
        // Preset flipped
        m_rotationSlider->setValue( 0.0 ); // reset rotation
        m_invertView->setChecked( false ); // reset inversion
        m_flipView->setChecked( true ); // set flip
        break;
        /* TODO: Figure out the right rotations for below and implement
    case 3:
        // Preset refractor
        m_rotationSlider->setValue( );
        m_invertView->setChecked( true );
        m_flipView->setChecked( false );
    case 4:
        // Preset Dobsonian
        m_rotationSlider->setValue( 0.0 ); // reset rotation
        m_invertView->setChecked( false ); // reset inversion
        m_flipView->setChecked( true ); // set flip
        break;
        */
    default:
        break;
    }
}

void EyepieceField::showEyepieceField( SkyPoint *sp, FOV const * const fov, const QString &imagePath ) {

    double dssWidth, dssHeight, fovWidth, fovHeight;

    Q_ASSERT( sp );

    // See if we were supplied a sky image; if so, load its metadata
    if( ! imagePath.isEmpty() ) {
        KSDssImage dssImage( imagePath );
        dssWidth = dssImage.getMetadata().width;
        dssHeight = dssImage.getMetadata().height;
        if( dssWidth == 0 || dssHeight == 0 ) {
            // Metadata unavailable, guess based on most common DSS arcsec/pixel
            const double dssArcSecPerPixel = 1.01;
            dssWidth = dssImage.getImage().width()*1.01/60.0;
            dssHeight = dssImage.getImage().height()*1.01/60.0;
        }
    }

    // Set up the new sky map FOV and pointing. full map FOV = 4 times the given FOV.
    if( fov ) {
        fovWidth = fov->sizeX();
        fovHeight = fov->sizeY();
    }
    else if( dssWidth > 0 && dssHeight > 0 ) {
        fovWidth = dssWidth;
        fovHeight = dssHeight;
    }
    else {
        fovWidth = 60.0;
        fovHeight = 60.0;
    }

    showEyepieceField( sp, fovWidth, fovHeight, imagePath, dssWidth, dssHeight );

}

void EyepieceField::showEyepieceField( SkyPoint *sp, const double fovWidth, double fovHeight, const QString &imagePath,
                                       const double dssWidth, const double dssHeight ) {

    SkyMap *map = SkyMap::Instance();
    KStars *ks = KStars::Instance();

    Q_ASSERT( sp );
    Q_ASSERT( map );
    Q_ASSERT( ks );

    if( fovWidth <= 0 )
        return;
    if( fovHeight <= 0 )
        fovHeight = fovWidth;

    // Save the current state of the sky map
    SkyPoint *oldFocus = map->focus();
    double oldZoomFactor = Options::zoomFactor();

    // Set the right zoom
    ks->setApproxFOV( ( ( fovWidth > fovHeight ) ? fovWidth : fovHeight ) / 15.0 );

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

    // determine screen arcminutes per pixel value
    const double arcMinToScreen = dms::PI * Options::zoomFactor() / 10800.0;

    // Vector export
    QTemporaryFile m_TempSvgFile;
    m_TempSvgFile.open();

    // export as SVG
    QSvgGenerator svgGenerator;
    svgGenerator.setFileName( m_TempSvgFile.fileName() );
    // svgGenerator.setTitle(i18n(""));
    // svgGenerator.setDescription(i18n(""));
    svgGenerator.setSize( QSize(map->width(), map->height()) );
    svgGenerator.setResolution(qMax(map->logicalDpiX(), map->logicalDpiY()));
    svgGenerator.setViewBox( QRect( map->width()/2.0 - arcMinToScreen*fovWidth/2.0, map->height()/2.0 - arcMinToScreen*fovHeight/2.0,
                                    arcMinToScreen*fovWidth, arcMinToScreen*fovHeight ) );

    SkyQPainter painter(KStars::Instance(), &svgGenerator);
    painter.begin();

    map->exportSkyImage(&painter);

    painter.end();

    QSvgRenderer svgRenderer( m_TempSvgFile.fileName() );
    m_skyChart = new QImage( arcMinToScreen * fovWidth * 2.0, arcMinToScreen * fovHeight * 2.0, QImage::Format_ARGB32 ); // 2 times bigger in both dimensions.
    QPainter p2( m_skyChart );
    svgRenderer.render( &p2 );

    m_TempSvgFile.close();


    // Reset the sky-map
    map->setZoomFactor( oldZoomFactor );
    map->setClickedPoint( oldFocus );
    map->slotCenter();
    qApp->processEvents();

    // Repeat -- dirty workaround for some problem in KStars
    map->setZoomFactor( oldZoomFactor );
    map->setClickedPoint( oldFocus );
    map->slotCenter();
    qApp->processEvents();
    map->forceUpdate();

    // Prepare the sky image
    if( ! imagePath.isEmpty() ) {

        m_skyImageDisplay->setVisible( true );

        if( m_skyImage )
            delete m_skyImage;

        m_skyImage = new QImage( int(arcMinToScreen * fovWidth * 2.0), int(arcMinToScreen * fovHeight * 2.0), QImage::Format_ARGB32 );
        m_skyImage->fill( Qt::transparent );
        QPainter p( m_skyImage );
        QImage rawImg( imagePath );
        QImage img = rawImg.scaled( arcMinToScreen * dssWidth * 2.0, arcMinToScreen * dssHeight * 2.0, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );

        if( Options::useAltAz() ) {
            // Need to rotate the image so that up is towards zenith rather than north.
            KStarsData *data = KStarsData::Instance();
            double lat = data->geo()->lat()->radians();

            // We trust that EquatorialToHorizontal has been called on geo, after all, how else can it have an alt/az representation.
            // Use spherical cosine rule (the triangle with vertices at sp, zenith and NCP) to compute the angle between direction of increasing altitude and north
            double cosNorthAngle = ( sin( lat ) - sin( sp->alt().radians() ) * sin( sp->dec().radians() ) )/( cos( sp->alt().radians() ) * cos( sp->dec().radians() ) );
            double northAngle = acos( cosNorthAngle ); // arccosine is blind to sign of the angle
            if( sp->az().reduce().Degrees() < 180.0 ) // if on the eastern hemisphere, flip sign
                northAngle = -northAngle;

            qDebug() << "North angle = " << dms( northAngle * 180/M_PI ).toDMSString();

            QTransform transform;
            transform.rotate( northAngle * 180/M_PI );
            img = img.transformed( transform, Qt::SmoothTransformation );
        }
        p.drawImage( QPointF( m_skyImage->width()/2.0 - img.width()/2.0, m_skyImage->height()/2.0 - img.width()/2.0 ), img );
        p.end();
    }
    else
        m_skyImageDisplay->setVisible( false );

    // Enforce preset as per selection, since we have loaded a new eyepiece view
    slotEnforcePreset( -1 );
    // Render the display
    render();
}

void EyepieceField::render() {

    QPixmap renderImage;
    QPixmap renderChart;

    QTransform transform;
    transform.rotate( m_rotationSlider->value() );
    if( m_flipView->isChecked() )
        transform.scale( -1, 1 );
    if( m_invertView->isChecked() )
        transform.scale( -1, -1 );

    Q_ASSERT( m_skyChart );

    renderChart = QPixmap::fromImage( m_skyChart->transformed( transform, Qt::SmoothTransformation ) );
    if( m_skyImage ) {
        QImage i;
        i = m_skyImage->transformed( transform, Qt::SmoothTransformation );
        if( m_invertColors->isChecked() )
            i.invertPixels();
        renderImage = QPixmap::fromImage( i );
    }
    if( m_overlay->isChecked() && m_skyImage ) {
        QColor skyColor = KStarsData::Instance()->colorScheme()->colorNamed( "SkyColor" );
        QBitmap mask = QBitmap::fromImage( m_skyChart->createMaskFromColor( skyColor.rgb() ).transformed( transform, Qt::SmoothTransformation ) );
        renderChart.setMask( mask );
        QPainter p( &renderImage );
        p.drawImage( QPointF( renderImage.width()/2.0 - renderChart.width()/2.0, renderImage.height()/2.0 - renderChart.height()/2.0 ),
                     renderChart.toImage() );
        QPixmap temp( renderImage.width(), renderImage.height() );
        temp.fill( skyColor );
        QPainter p2( &temp );
        p2.drawImage( QPointF(0,0), renderImage.toImage() );
        p2.end();
        p.end();
        renderImage = temp;
        m_skyChartDisplay->setVisible( false );
    }
    else
        m_skyChartDisplay->setVisible( true );

    if( m_skyChartDisplay->isVisible() )
        m_skyChartDisplay->setPixmap( renderChart.scaled( m_skyChartDisplay->width(), m_skyChartDisplay->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
    if( m_skyImageDisplay->isVisible() )
        m_skyImageDisplay->setPixmap( renderImage.scaled( m_skyImageDisplay->width(), m_skyImageDisplay->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation ) );

    show();
}

EyepieceField::~EyepieceField() {
    // Empty
    delete m_skyChart;
    delete m_skyImage;
}
