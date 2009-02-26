/***************************************************************************
                          skymap.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Feb 10 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "skymap.h"

#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include <QCursor>
#include <QBitmap>
#include <QPainter>
#include <QPixmap>
#include <QTextStream>
#include <QKeyEvent>
#include <QEvent>
#include <QFile>
#include <QPointF>
#include <QApplication>

#include <kactioncollection.h>
#include <kconfig.h>
#include <kiconloader.h>
#include <kstatusbar.h>
#include <kmessagebox.h>
#include <kaction.h>
#include <kstandarddirs.h>
#include <ktoolbar.h>
#include <ktoolinvocation.h>
#include <kicon.h>

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "imageviewer.h"
#include "infoboxes.h"
#include "dialogs/detaildialog.h"
#include "dialogs/addlinkdialog.h"
#include "kspopupmenu.h"
#include "simclock.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/deepskyobject.h"
#include "skyobjects/ksmoon.h"
#include "skyobjects/ksasteroid.h"
#include "skyobjects/kscomet.h"
#include "skyobjects/starobject.h"

#ifdef HAVE_XPLANET
#include <KProcess>
#include <kfiledialog.h>
#endif

SkyMap* SkyMap::pinstance = 0;

SkyMap* SkyMap::Create( KStarsData *_data, KStars *_ks )
{
    if ( pinstance ) delete pinstance;
    pinstance = new SkyMap( _data, _ks );
    return pinstance;
}

SkyMap* SkyMap::Instance( )
{
    return pinstance;
}

SkyMap::SkyMap( KStarsData *_data, KStars *_ks )
        : QWidget(_ks), computeSkymap(true), angularDistanceMode(false),
        ks(_ks), data(_data), pmenu(0), sky(0), sky2(0), IBoxes(0),
        ClickedObject(0), FocusObject(0), TransientObject(0), sp(0)
{
    m_Scale = 1.0;

    sp = new SkyPoint();            // needed by coordinate grid
    ZoomRect = QRect();

    setDefaultMouseCursor();	// set the cross cursor

    QPalette p = palette();
    p.setColor( QPalette::Window, QColor( data->colorScheme()->colorNamed( "SkyColor" ) ) );
    setPalette( p );

    setFocusPolicy( Qt::StrongFocus );
    setMinimumSize( 380, 250 );
    setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding ) );

    setMouseTracking (true); //Generate MouseMove events!
    midMouseButtonDown = false;
    mouseButtonDown = false;
    slewing = false;
    clockSlewing = false;

    ClickedObject = NULL;
    FocusObject = NULL;

    sky = new QPixmap( width(),  height() );
    sky2 = new QPixmap( width(),  height() );
    pmenu = new KSPopupMenu( ks );

    //Initialize Transient label stuff
    TransientTimeout = 100; //fade label color every 0.1 sec
    HoverTimer.setSingleShot( true ); // using this timer as a single shot timer

    connect( &HoverTimer, SIGNAL( timeout() ), this, SLOT( slotTransientLabel() ) );
    connect( &TransientTimer, SIGNAL( timeout() ), this, SLOT( slotTransientTimeout() ) );

    IBoxes = new InfoBoxes( Options::windowWidth(), Options::windowHeight(),
                            Options::positionTimeBox(), Options::shadeTimeBox(),
                            Options::positionGeoBox(), Options::shadeGeoBox(),
                            Options::positionFocusBox(), Options::shadeFocusBox(),
                            data->colorScheme()->colorNamed( "BoxTextColor" ),
                            data->colorScheme()->colorNamed( "BoxGrabColor" ),
                            data->colorScheme()->colorNamed( "BoxBGColor" ) );

    IBoxes->showTimeBox( Options::showTimeBox() );
    IBoxes->showFocusBox( Options::showFocusBox() );
    IBoxes->showGeoBox( Options::showGeoBox() );
    IBoxes->timeBox()->setAnchorFlag( Options::stickyTimeBox() );
    IBoxes->geoBox()->setAnchorFlag( Options::stickyGeoBox() );
    IBoxes->focusBox()->setAnchorFlag( Options::stickyFocusBox() );

    IBoxes->geoChanged( data->geo() );

    connect( IBoxes->timeBox(),  SIGNAL( shaded(bool) ), data, SLOT( saveTimeBoxShaded(bool) ) );
    connect( IBoxes->geoBox(),   SIGNAL( shaded(bool) ), data, SLOT( saveGeoBoxShaded(bool) ) );
    connect( IBoxes->focusBox(), SIGNAL( shaded(bool) ), data, SLOT( saveFocusBoxShaded(bool) ) );
    connect( IBoxes->timeBox(),  SIGNAL( moved(QPoint) ), data, SLOT( saveTimeBoxPos(QPoint) ) );
    connect( IBoxes->geoBox(),   SIGNAL( moved(QPoint) ), data, SLOT( saveGeoBoxPos(QPoint) ) );
    connect( IBoxes->focusBox(), SIGNAL( moved(QPoint) ), data, SLOT( saveFocusBoxPos(QPoint) ) );

    connect( this, SIGNAL( destinationChanged() ), this, SLOT( slewFocus() ) );

    //Initialize Refraction correction lookup table arrays.  RefractCorr1 is for calculating
    //the apparent altitude from the true altitude, and RefractCorr2 is for the reverse.
    for ( unsigned int index = 0; index <184; ++index ) {
        double alt = -1.75 + index*0.5;  //start at -1.75 degrees to get midpoint value for each interval.

        RefractCorr1[index] = 1.02 / tan( dms::PI*( alt + 10.3/(alt + 5.11) )/180.0 ) / 60.0; //correction in degrees.
        RefractCorr2[index] = -1.0 / tan( dms::PI*( alt + 7.31/(alt + 4.4) )/180.0 ) / 60.0;
    }
}

SkyMap::~SkyMap() {
    delete sp;
    delete sky;
    delete sky2;
    delete pmenu;
    delete IBoxes;
}

void SkyMap::setGeometry( int x, int y, int w, int h ) {
    QWidget::setGeometry( x, y, w, h );
    *sky = sky->scaled( w, h );
    *sky2 = sky2->scaled( w, h );
}

void SkyMap::setGeometry( const QRect &r ) {
    QWidget::setGeometry( r );
    *sky = sky->scaled( r.width(), r.height() );
    *sky2 = sky2->scaled( r.width(), r.height() );
}


void SkyMap::showFocusCoords( bool coordsOnly ) {
    if ( ! coordsOnly ) {
        //display object info in infoBoxes
        QString oname;
        oname = i18n( "nothing" );
        if ( focusObject() != NULL && Options::isTracking() )
            oname = focusObject()->translatedLongName();

        infoBoxes()->focusObjChanged(oname);
    }

    infoBoxes()->focusCoordChanged( focus() );
}

void SkyMap::slotTransientLabel( void ) {
    //This function is only called if the HoverTimer manages to timeout.
    //(HoverTimer is restarted with every mouseMoveEvent; so if it times
    //out, that means there was no mouse movement for HOVER_INTERVAL msec.)
    //Identify the object nearest to the mouse cursor as the
    //TransientObject.  The TransientObject is automatically labeled
    //in SkyMap::paintEvent().
    //Note that when the TransientObject pointer is not NULL, the next
    //mouseMoveEvent calls fadeTransientLabel(), which will fade out the
    //TransientLabel and then set TransientObject to NULL.
    //
    //Do not show a transient label if the map is in motion, or if the mouse
    //pointer is below the opaque horizon, or if the object has a permanent label
    if ( ! slewing && ! ( Options::useAltAz() && Options::showHorizon() && Options::showGround() &&
                          refract( mousePoint()->alt(), true ).Degrees() < 0.0 ) ) {
        double maxrad = 1000.0/Options::zoomFactor();
        SkyObject *so = data->skyComposite()->objectNearest( mousePoint(), maxrad );

        if ( so && ! isObjectLabeled( so ) ) {
            setTransientObject( so );

            TransientColor = data->colorScheme()->colorNamed( "UserLabelColor" );
            if ( TransientTimer.isActive() ) TransientTimer.stop();
            update();
        }
    }
}


//Slots

void SkyMap::slotTransientTimeout( void ) {
    //Don't fade label if the transientObject is now the focusObject!
    if ( transientObject() == focusObject() && Options::useAutoLabel() ) {
        setTransientObject( NULL );
        TransientTimer.stop();
        return;
    }

    //to fade the labels, we will need to smoothly transition the alpha
    //channel from opaque (255) to transparent (0) by step of stepAlpha
    static const int stepAlpha = 12;

    //Check to see if next step produces a transparent label
    //If so, point TransientObject to NULL.
    if ( TransientColor.alpha() <= stepAlpha ) {
        setTransientObject( NULL );
        TransientTimer.stop();
    } else {
        TransientColor.setAlpha(TransientColor.alpha()-stepAlpha);
    }

    update();
}

void SkyMap::setClickedObject( SkyObject *o ) {
	  ClickedObject = o;
}

void SkyMap::setFocusObject( SkyObject *o ) {
    FocusObject = o;

    if ( FocusObject )
        Options::setFocusObject( FocusObject->name() );
    else
        Options::setFocusObject( i18n( "nothing" ) );
}

void SkyMap::slotCenter( void ) {
    setFocusPoint( clickedPoint() );
    if ( Options::useAltAz() )
        focusPoint()->EquatorialToHorizontal( data->LST, data->geo()->lat() );

    //clear the planet trail of old focusObject, if it was temporary
    if ( focusObject() && focusObject()->isSolarSystem() && data->temporaryTrail ) {
        ((KSPlanetBase*)focusObject())->clearTrail();
        data->temporaryTrail = false;
    }

    //If the requested object is below the opaque horizon, issue a warning message
    //(unless user is already pointed below the horizon)
    if ( Options::useAltAz() && Options::showHorizon() && Options::showGround() &&
            focus()->alt()->Degrees() > -1.0 && focusPoint()->alt()->Degrees() < -1.0 ) {

        QString caption = i18n( "Requested Position Below Horizon" );
        QString message = i18n( "The requested position is below the horizon.\nWould you like to go there anyway?" );
        if ( KMessageBox::warningYesNo( this, message, caption,
                                        KGuiItem(i18n("Go Anyway")), KGuiItem(i18n("Keep Position")), "dag_focus_below_horiz" )==KMessageBox::No ) {
            setClickedObject( NULL );
            setFocusObject( NULL );
            Options::setIsTracking( false );

            return;
        }
    }

    //set FocusObject before slewing.  Otherwise, KStarsData::updateTime() can reset
    //destination to previous object...
    setFocusObject( ClickedObject );
    Options::setIsTracking( true );
    if ( ks ) {
        ks->actionCollection()->action("track_object")->setIcon( KIcon("document-encrypt") );
        ks->actionCollection()->action("track_object")->setText( i18n( "Stop &Tracking" ) );
    }

    //If focusObject is a SS body and doesn't already have a trail, set the temporaryTrail
    if ( focusObject() && focusObject()->isSolarSystem()
            && Options::useAutoTrail()
            && ! ((KSPlanetBase*)focusObject())->hasTrail() ) {
        ((KSPlanetBase*)focusObject())->addToTrail();
        data->temporaryTrail = true;
    }

    //update the destination to the selected coordinates
    if ( Options::useAltAz() ) {
        if ( Options::useRefraction() )
            setDestinationAltAz( refract( focusPoint()->alt(), true ).Degrees(), focusPoint()->az()->Degrees() );
        else
            setDestinationAltAz( focusPoint()->alt()->Degrees(), focusPoint()->az()->Degrees() );
    } else {
        setDestination( focusPoint() );
    }

    focusPoint()->EquatorialToHorizontal( data->LST, data->geo()->lat() );

    //display coordinates in statusBar
    if ( ks ) {
        if ( Options::showAltAzField() ) {
            QString sX = focusPoint()->az()->toDMSString();
            QString sY = focusPoint()->alt()->toDMSString(true);
            if ( Options::useAltAz() && Options::useRefraction() )
                sY = refract( focusPoint()->alt(), true ).toDMSString(true);
            QString s = sX + ",  " + sY;
            ks->statusBar()->changeItem( s, 1 );
        }

        if ( Options::showRADecField() ) {
            QString s = focusPoint()->ra()->toHMSString() + ",  " + focusPoint()->dec()->toDMSString(true);
            ks->statusBar()->changeItem( s, 2 );
        }
    }

    showFocusCoords(); //update FocusBox
}

void SkyMap::slotDSS( void ) {
    QString URLprefix( "http://archive.stsci.edu/cgi-bin/dss_search?v=1" );
    QString URLsuffix( "&e=J2000&h=15.0&w=15.0&f=gif&c=none&fov=NONE" );
    dms ra(0.0), dec(0.0);
    QString RAString, DecString;
    char decsgn;

    //ra and dec must be the coordinates at J2000.  If we clicked on an object, just use the object's ra0, dec0 coords
    //if we clicked on empty sky, we need to precess to J2000.
    if ( clickedObject() ) {
        ra.setH( clickedObject()->ra0()->Hours() );
        dec.setD( clickedObject()->dec0()->Degrees() );
    } else {
        //move present coords temporarily to ra0,dec0 (needed for precessToAnyEpoch)
        clickedPoint()->setRA0( clickedPoint()->ra()->Hours() );
        clickedPoint()->setDec0( clickedPoint()->dec()->Degrees() );
        clickedPoint()->precessFromAnyEpoch( data->ut().djd(), J2000 );
        ra.setH( clickedPoint()->ra()->Hours() );
        dec.setD( clickedPoint()->dec()->Degrees() );

        //restore coords from present epoch
        clickedPoint()->setRA( clickedPoint()->ra0()->Hours() );
        clickedPoint()->setDec( clickedPoint()->dec0()->Degrees() );
    }

    RAString = RAString.sprintf( "&r=%02d+%02d+%02d", ra.hour(), ra.minute(), ra.second() );

    decsgn = '+';
    if ( dec.Degrees() < 0.0 ) decsgn = '-';
    int dd = abs( dec.degree() );
    int dm = abs( dec.arcmin() );
    int ds = abs( dec.arcsec() );
    DecString = DecString.sprintf( "&d=%c%02d+%02d+%02d", decsgn, dd, dm, ds );

    //concat all the segments into the kview command line:
    KUrl url (URLprefix + RAString + DecString + URLsuffix);

    QString message = i18n( "Digitized Sky Survey image provided by the Space Telescope Science Institute [public domain]." );

    ImageViewer *iv = ks->addImageViewer( url, message );
    iv->show();
}

void SkyMap::slotSDSS() {
	QString URLprefix( "http://casjobs.sdss.org/ImgCutoutDR6/getjpeg.aspx?" );
	QString URLsuffix( "&scale=1.0&width=600&height=600&opt=GST&query=SR(10,20)" );
	dms ra(0.0), dec(0.0);
	QString RAString, DecString;

	//ra and dec must be the coordinates at J2000.  If we clicked on an object, just use the object's ra0, dec0 coords
	//if we clicked on empty sky, we need to precess to J2000.
	if ( clickedObject() ) {
		ra.setH( clickedObject()->ra0()->Hours() );
		dec.setD( clickedObject()->dec0()->Degrees() );
	} else {
		//move present coords temporarily to ra0,dec0 (needed for precessToAnyEpoch)
		clickedPoint()->setRA0( clickedPoint()->ra()->Hours() );
		clickedPoint()->setDec0( clickedPoint()->dec()->Degrees() );
		clickedPoint()->precessFromAnyEpoch( data->ut().djd(), J2000 );
		ra.setH( clickedPoint()->ra()->Hours() );
		dec.setD( clickedPoint()->dec()->Degrees() );

		//restore coords from present epoch
		clickedPoint()->setRA( clickedPoint()->ra0()->Hours() );
		clickedPoint()->setDec( clickedPoint()->dec0()->Degrees() );
	}

	RAString = RAString.sprintf( "ra=%f", ra.Degrees() );
	DecString = DecString.sprintf( "&dec=%f", dec.Degrees() );

	//concat all the segments into the kview command line:
	KUrl url (URLprefix + RAString + DecString + URLsuffix);

	QString message = i18n( "Sloan Digital Sky Survey image provided by the Astrophysical Research Consortium [free for non-commercial use]." );

	ImageViewer *iv = ks->addImageViewer( url, message );
	iv->show();
}

void SkyMap::slotBeginAngularDistance() {
    angularDistanceMode = true;
    AngularRuler.clear();

    //If the cursor is near a SkyObject, reset the AngularRuler's 
    //start point to the position of the SkyObject
    double maxrad = 1000.0/Options::zoomFactor();
    SkyObject *so = data->skyComposite()->objectNearest( clickedPoint(), maxrad );
    if ( so ) {
        AngularRuler.append( so );
        AngularRuler.append( so );
    } else {
        AngularRuler.append( clickedPoint() );
        AngularRuler.append( clickedPoint() );
    }

    AngularRuler.update( data );
}

void SkyMap::slotEndAngularDistance() {
    if( angularDistanceMode ) {
        dms angularDistance;
        QString sbMessage;

        //If the cursor is near a SkyObject, reset the AngularRuler's
        //end point to the position of the SkyObject
        double maxrad = 1000.0/Options::zoomFactor();
        SkyObject *so = data->skyComposite()->objectNearest( clickedPoint(), maxrad );
        if ( so ) {
            AngularRuler.setPoint( 1, so );
            sbMessage = so->translatedLongName() + "   ";
        } else {
            AngularRuler.setPoint( 1, clickedPoint() );
        }

        angularDistanceMode=false;
        AngularRuler.update( data );

        angularDistance = AngularRuler.angularSize();
        sbMessage += i18n( "Angular distance: %1", angularDistance.toDMSString() );

        ks->statusBar()->changeItem( sbMessage, 0 );
        
        AngularRuler.clear();
    }
}

void SkyMap::slotCancelAngularDistance(void) {
    angularDistanceMode=false;
    AngularRuler.clear();
}

void SkyMap::slotImage() {
    QString message = ((KAction*)sender())->text();
    message = message.remove( '&' ); //Get rid of accelerator markers

    // Need to do this because we are comparing translated strings
    int index = -1;
    for( int i = 0; i < clickedObject()->ImageTitle().size(); ++i ) {
        if( i18nc( "Image/info menu item (should be translated)", clickedObject()->ImageTitle().at( i ).toLocal8Bit().data() ) == message ) {
            index = i;
            break;
        }
    }

    QString sURL;
    if ( index >= 0 && index < clickedObject()->ImageList().size() ) {
        sURL = clickedObject()->ImageList()[ index ];
    } else {
        kWarning() << "ImageList index out of bounds: " << index;
        if ( index == -1 ) {
            kWarning() << "Message string \"" << message << "\" not found in ImageTitle.";
            kDebug() << clickedObject()->ImageTitle();
        }
    }

    KUrl url ( sURL );
    if ( url.isEmpty() ) return;

    ks->addImageViewer( url, clickedObject()->messageFromTitle(message) );
}

void SkyMap::slotInfo() {
    QString message = ((KAction*)sender())->text();
    message = message.remove( '&' ); //Get rid of accelerator markers

    // Need to do this because we are comparing translated strings
    int index = -1;
    for( int i = 0; i < clickedObject()->InfoTitle().size(); ++i ) {
        if( i18nc( "Image/info menu item (should be translated)", clickedObject()->InfoTitle().at( i ).toLocal8Bit().data() ) == message ) {
            index = i;
            break;
        }
    }

    QString sURL;
    if ( index >= 0 && index < clickedObject()->InfoList().size() ) {
        sURL = clickedObject()->InfoList()[ index ];
    } else {
        kWarning() << "InfoList index out of bounds: " << index;
        if ( index == -1 ) {
            kWarning() << "Message string \"" << message << "\" not found in InfoTitle.";
            kDebug() << clickedObject()->InfoTitle();
        }
    }

    KUrl url ( sURL );
    if (!url.isEmpty())
        KToolInvocation::invokeBrowser(sURL);
}

bool SkyMap::isObjectLabeled( SkyObject *object ) {
    foreach ( SkyObject *o, data->skyComposite()->labelObjects() ) {
        if ( o == object ) return true;
    }

    return false;
}

void SkyMap::slotRemoveObjectLabel( void ) {
    data->skyComposite()->removeNameLabel( clickedObject() );
    forceUpdate();
}

void SkyMap::slotAddObjectLabel( void ) {
    data->skyComposite()->addNameLabel( clickedObject() );
    //Since we just added a permanent label, we don't want it to fade away!
    if ( transientObject() == clickedObject() ) setTransientObject( NULL );
    forceUpdate();
}

void SkyMap::slotRemovePlanetTrail( void ) {
    //probably don't need this if-statement, but just to be sure...
    if ( clickedObject() && clickedObject()->isSolarSystem() ) {
        data->skyComposite()->removeTrail( clickedObject() );
        forceUpdate();
    }
}

void SkyMap::slotAddPlanetTrail( void ) {
    //probably don't need this if-statement, but just to be sure...
    if ( clickedObject() && clickedObject()->isSolarSystem() ) {
        data->skyComposite()->addTrail( clickedObject() );
        forceUpdate();
    }
}

void SkyMap::slotDetail( void ) {
    // check if object is selected
    if ( !clickedObject() ) {
        KMessageBox::sorry( this, i18n("No object selected."), i18n("Object Details") );
        return;
    }
    DetailDialog detail( clickedObject(), data->ut(), data->geo(), ks );
    detail.exec();
}

void SkyMap::slotClockSlewing() {
    //If the current timescale exceeds slewTimeScale, set clockSlewing=true, and stop the clock.
    if ( fabs( data->clock()->scale() ) > Options::slewTimeScale() ) {
        if ( ! clockSlewing ) {
            clockSlewing = true;
            data->clock()->setManualMode( true );

            // don't change automatically the DST status
            if ( ks ) ks->updateTime( false );
        }
    } else {
        if ( clockSlewing ) {
            clockSlewing = false;
            data->clock()->setManualMode( false );

            // don't change automatically the DST status
            if ( ks ) ks->updateTime( false );
        }
    }
}

void SkyMap::setFocus( SkyPoint *p ) {
    setFocus( p->ra()->Hours(), p->dec()->Degrees() );
}

void SkyMap::setFocus( const dms &ra, const dms &dec ) {
    setFocus( ra.Hours(), dec.Degrees() );
}

void SkyMap::setFocus( double ra, double dec ) {
    //QUATERNION
    m_rotAxis.createFromEuler( (dec)*dms::DegToRad, (15.0*ra)*dms::DegToRad, 0.0 );
    m_rotAxis = m_rotAxis.inverse();

    Focus.set( ra, dec );
    Options::setFocusRA( ra );
    Options::setFocusDec( dec );

    focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
}

void SkyMap::setFocusAltAz( const dms &alt, const dms &az) {
    setFocusAltAz( alt.Degrees(), az.Degrees() );
}

void SkyMap::setFocusAltAz(double alt, double az) {
    focus()->setAlt(alt);
    focus()->setAz(az);
    focus()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
    Options::setFocusRA( focus()->ra()->Hours() );
    Options::setFocusDec( focus()->dec()->Degrees() );

    slewing = false;

    oldfocus()->set( focus()->ra(), focus()->dec() );
    oldfocus()->setAz( focus()->az()->Degrees() );
    oldfocus()->setAlt( focus()->alt()->Degrees() );

    double dHA = data->LST->Hours() - focus()->ra()->Hours();
    while ( dHA < 0.0 ) dHA += 24.0;
    data->HourAngle->setH( dHA );

    forceUpdate(); //need a total update, or slewing with the arrow keys doesn't work.
}

void SkyMap::setDestination( SkyPoint *p ) {
    Destination.set( p->ra(), p->dec() );
    destination()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
    emit destinationChanged();
}

void SkyMap::setDestination( const dms &ra, const dms &dec ) {
    Destination.set( ra, dec );
    destination()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
    emit destinationChanged();
}

void SkyMap::setDestination( double ra, double dec ) {
    Destination.set( ra, dec );
    destination()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
    emit destinationChanged();
}

void SkyMap::setDestinationAltAz( const dms &alt, const dms &az) {
    destination()->setAlt(alt);
    destination()->setAz(az);
    destination()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
    emit destinationChanged();
}

void SkyMap::setDestinationAltAz(double alt, double az) {
    destination()->setAlt(alt);
    destination()->setAz(az);
    destination()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
    emit destinationChanged();
}

void SkyMap::setClickedPoint( SkyPoint *f ) { 
    PreviousClickedPoint.set(ClickedPoint.ra(), ClickedPoint.dec());
    ClickedPoint.set( f->ra(), f->dec() );
}

void SkyMap::updateFocus() {
    if ( slewing ) return;

    //Tracking on an object
    if ( Options::isTracking() && focusObject() != NULL ) {
        if ( Options::useAltAz() ) {
            //Tracking any object in Alt/Az mode requires focus updates
            double dAlt = focusObject()->alt()->Degrees();
            if ( Options::useRefraction() )
                dAlt = refract( focusObject()->alt(), true ).Degrees();
            setFocusAltAz( dAlt, focusObject()->az()->Degrees() );
            focus()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
            setDestination( focus() );
        } else {
            //Tracking in equatorial coords
            setFocus( focusObject() );
            focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
            setDestination( focus() );
        }

    //Tracking on empty sky
    } else if ( Options::isTracking() && focusPoint() != NULL ) {
        if ( Options::useAltAz() ) {
            //Tracking on empty sky in Alt/Az mode
            setFocus( focusPoint() );
            focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
            setDestination( focus() );
        }

    //Not tracking and not slewing, let sky drift by
    } else {
        if ( Options::useAltAz() ) {
            focus()->setAlt( destination()->alt()->Degrees() );
            focus()->setAz( destination()->az()->Degrees() );
            focus()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
            //destination()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
        } else {
            focus()->setRA( data->LST->Hours() - data->HourAngle->Hours() );
            setDestination( focus() );
            focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
            destination()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
        }
    }

    //Update the Hour Angle
    data->setHourAngle( data->LST->Hours() - focus()->ra()->Hours() );

    setOldFocus( focus() );
    oldfocus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
}

void SkyMap::slewFocus( void ) {
    double dX, dY, fX, fY, r, r0;
    double step0 = 0.5;
    double step = step0;
    double maxstep = 10.0;
    
    SkyPoint newFocus;

    //Don't slew if the mouse button is pressed
    //Also, no animated slews if the Manual Clock is active
    //08/2002: added possibility for one-time skipping of slew with snapNextFocus
    if ( !mouseButtonDown ) {
        bool goSlew = ( Options::useAnimatedSlewing() &&
                        ! data->snapNextFocus() ) &&
                      !( data->clock()->isManualMode() && data->clock()->isActive() );
        if ( goSlew  ) {
            if ( Options::useAltAz() ) {
                dX = destination()->az()->Degrees() - focus()->az()->Degrees();
                dY = destination()->alt()->Degrees() - focus()->alt()->Degrees();
            } else {
                dX = destination()->ra()->Degrees() - focus()->ra()->Degrees();
                dY = destination()->dec()->Degrees() - focus()->dec()->Degrees();
            }

            //switch directions to go the short way around the celestial sphere, if necessary.
            if ( dX < -180.0 ) dX = 360.0 + dX;
            else if ( dX > 180.0 ) dX = -360.0 + dX;

            r0 = sqrt( dX*dX + dY*dY );
            r = r0;
            if ( r0 < 20.0 ) { //smaller slews have smaller maxstep
                maxstep *= (10.0 + 0.5*r0)/20.0;
            }
            while ( r > step ) {
                //DEBUG
                kDebug() << step << ": " << r << ": " << r0 << endl;
                fX = dX / r;
                fY = dY / r;

                if ( Options::useAltAz() ) {
                    focus()->setAlt( focus()->alt()->Degrees() + fY*step );
                    focus()->setAz( dms( focus()->az()->Degrees() + fX*step ).reduce() );
                    focus()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
                } else {
                    fX = fX/15.; //convert RA degrees to hours
                    newFocus.set( focus()->ra()->Hours() + fX*step, focus()->dec()->Degrees() + fY*step );
                    setFocus( &newFocus );
                    focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
                }

                slewing = true;
                //since we are slewing, fade out the transient label
                if ( transientObject() && ! TransientTimer.isActive() )
                    fadeTransientLabel();

                forceUpdate();
                qApp->processEvents(); //keep up with other stuff

                if ( Options::useAltAz() ) {
                    dX = destination()->az()->Degrees() - focus()->az()->Degrees();
                    dY = destination()->alt()->Degrees() - focus()->alt()->Degrees();
                } else {
                    dX = destination()->ra()->Degrees() - focus()->ra()->Degrees();
                    dY = destination()->dec()->Degrees() - focus()->dec()->Degrees();
                }

                //switch directions to go the short way around the celestial sphere, if necessary.
                if ( dX < -180.0 ) dX = 360.0 + dX;
                else if ( dX > 180.0 ) dX = -360.0 + dX;

                r = sqrt( dX*dX + dY*dY );
                
                //Modify step according to a cosine-shaped profile
                //centered on the midpoint of the slew
                //NOTE: don't allow the full range from -PI/2 to PI/2
                //because the slew will never reach the destination as 
                //the speed approaches zero at the end!
                double t = dms::PI*(r - 0.5*r0)/(1.05*r0);
                step = cos(t)*maxstep;
            }
        }

        //Either useAnimatedSlewing==false, or we have slewed, and are within one step of destination
        //set focus=destination.
        if ( Options::useAltAz() ) {
            setFocusAltAz( destination()->alt()->Degrees(), destination()->az()->Degrees() );
            focus()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
        } else {
            setFocus( destination() );
            focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
        }

        data->HourAngle->setH( data->LST->Hours() - focus()->ra()->Hours() );
        slewing = false;

        //Turn off snapNextFocus, we only want it to happen once
        if ( data->snapNextFocus() ) {
            data->setSnapNextFocus(false);
        }

        //Start the HoverTimer. if the user leaves the mouse in place after a slew,
        //we want to attach a label to the nearest object.
        if ( Options::useHoverLabel() )
            HoverTimer.start( HOVER_INTERVAL );

        forceUpdate();
    }
}

void SkyMap::invokeKey( int key ) {
    QKeyEvent *e = new QKeyEvent( QEvent::KeyPress, key, 0 );
    keyPressEvent( e );
    delete e;
}

double SkyMap::findPA( SkyObject *o, float x, float y ) {
    //Find position angle of North using a test point displaced to the north
    //displace by 100/zoomFactor radians (so distance is always 100 pixels)
    //this is 5730/zoomFactor degrees
    double newDec = o->dec()->Degrees() + 5730.0/Options::zoomFactor();
    if ( newDec > 90.0 ) newDec = 90.0;
    SkyPoint test( o->ra()->Hours(), newDec );
    if ( Options::useAltAz() ) test.EquatorialToHorizontal( data->LST, data->geo()->lat() );
    QPointF t = toScreen( &test );
    double dx = double( t.x() - x );
    double dy = double( y - t.y() );  //backwards because QWidget Y-axis increases to the bottom
    double north;
    if ( dy ) {
        north = atan2( dx, dy )*180.0/dms::PI;
    } else {
        north = 90.0;
        if ( dx > 0 ) north = -90.0;
    }

    return ( north + o->pa() );
}

//QUATERNION
void SkyMap::slotRotateTo( SkyPoint */*p*/ ) {}

//QUATERNION
QPointF SkyMap::toScreenQuaternion( SkyPoint *o ) {
    QPointF p;
    Quaternion oq = o->quat();
    //	Quaternion invRotAxis = m_rotAxis.inverse();
    oq.rotateAroundAxis( m_rotAxis );

    double zoomscale = m_Scale*Options::zoomFactor();
    double k;
    //c is the cosine of the angular distance from the center.
    //I believe this is just the z coordinate.
    double c = oq.v[Q_Z];
    switch ( Options::projection() ) {
    case Lambert:
        k = sqrt( 2.0/( 1.0 + c ) );
        break;
    case AzimuthalEquidistant:
        {
            double crad = acos(c);
            k = crad/sin(crad);
            break;
        }
    case Orthographic:
        k = 1.0;
        break;
    case Stereographic:
        k = 2.0/(1.0 + c);
        break;
    case Gnomonic:
        k = 1.0/c;
        break;
    default: //should never get here
        kWarning() << i18n("Unrecognized coordinate projection: ") << Options::projection();
        k = 1.0;  //just default to Orthographic
        break;
    }

    p.setX( 0.5*width()  - zoomscale*k*oq.v[Q_X] );
    p.setY( 0.5*height() - zoomscale*k*oq.v[Q_Y] );

    return p;
}

QPointF SkyMap::toScreen( SkyPoint *o, bool oRefract, bool *onVisibleHemisphere) {

    QPointF p;
    double Y, dX;
    double sindX, cosdX, sinY, cosY, sinY0, cosY0;

    float Width = width() * m_Scale;
    float Height = height() * m_Scale;
    double zoomscale = Options::zoomFactor() * m_Scale;

    //oRefract = true means listen to Options::useRefraction()
    //false means do not use refraction
    bool useRefract = oRefract;
    if ( oRefract == true ) useRefract = Options::useRefraction();

    if ( Options::useAltAz() ) {
        if ( useRefract ) Y = refract( o->alt(), true ).radians(); //account for atmospheric refraction
        else Y = o->alt()->radians();

        if ( focus()->az()->Degrees() > 270.0 && o->az()->Degrees() < 90.0 ) {
            dX = -2.0*dms::PI + focus()->az()->reduce().radians() - o->az()->reduce().radians();
        } else if ( focus()->az()->Degrees() < 90.0 && o->az()->Degrees() > 270.0 ) {
            dX = 2.0*dms::PI + focus()->az()->reduce().radians() - o->az()->reduce().radians();
        } else {
            dX = focus()->az()->reduce().radians() - o->az()->reduce().radians();
        }

        focus()->alt()->SinCos( sinY0, cosY0 );

    } else {
        if (focus()->ra()->Hours() > 18.0 && o->ra()->Hours() < 6.0) {
            dX = 2.0*dms::PI + o->ra()->reduce().radians() - focus()->ra()->reduce().radians();
        } else if (focus()->ra()->Hours() < 6.0 && o->ra()->Hours() > 18.0) {
            dX = o->ra()->reduce().radians() - focus()->ra()->reduce().radians() - 2.0*dms::PI;
        } else {
            dX = o->ra()->reduce().radians() - focus()->ra()->reduce().radians();
        }
        Y = o->dec()->radians();
        focus()->dec()->SinCos( sinY0, cosY0 );
    }

    //Special case: Equirectangular projection is very simple
    if ( Options::projection() == Equirectangular ) {

        p.setX( 0.5*Width  - zoomscale*dX );
        if ( Options::useAltAz() )
            p.setY( 0.5*Height - zoomscale*(Y - focus()->alt()->radians()) );
        else
            p.setY( 0.5*Height - zoomscale*(Y - focus()->dec()->radians()) );

        if ( onVisibleHemisphere != NULL ) {
            if ( scaledRect().contains( p.toPoint() ) )  //FIXME -jbb
                *onVisibleHemisphere = true;
            else
                *onVisibleHemisphere = false;
        }

        return p;
    }

    //Convert dX, Y coords to screen pixel coords.
	#if ( __GLIBC__ >= 2 && __GLIBC_MINOR__ >=1 )
    //GNU version
    sincos( dX, &sindX, &cosdX );
    sincos( Y, &sinY, &cosY );
	#else
    //ANSI version
    sindX = sin(dX);
    cosdX = cos(dX);
    sinY  = sin(Y);
    cosY  = cos(Y);
	#endif

    // Reference for these map projections: Wolfram MathWorld
    // Lambert Azimuthal Equal-Area:
    // http://mathworld.wolfram.com/LambertAzimuthalEqual-AreaProjection.html
    // 
    // Azimuthal Equidistant:
    // http://mathworld.wolfram.com/AzimuthalEquidistantProjection.html
    //
    // Orthographic:
    // http://mathworld.wolfram.com/OrthographicProjection.html
    //
    // Stereographic:
    // http://mathworld.wolfram.com/StereographicProjection.html
    //
    // Gnomonic:
    // http://mathworld.wolfram.com/GnomonicProjection.html

    //c is the cosine of the angular distance from the center
    double c = sinY0*sinY + cosY0*cosY*cosdX;

    if (onVisibleHemisphere != NULL) {
        *onVisibleHemisphere = true;
    }

    //If c is less than 0.0, then the "field angle" (angular distance from the focus) 
    //is more than 90 degrees.  This is on the "back side" of the celestial sphere 
    //and should not be drawn.
    //The Gnomonic projection has an infinite sky horizon, so don't allow the field
    //angle to approach 90 degrees in thi scase (cut it off at c=0.2).
    if ( c < 0.0 || ( Options::projection()==Gnomonic && c < 0.2 ) ) {
        if (onVisibleHemisphere == NULL) {
            p.setX( -10000000. );
            p.setY( -10000000. );
            return p;
        }
        else {
            *onVisibleHemisphere = false;
        }
    }

    double k;
    switch ( Options::projection() ) {
    case Lambert:
        k = sqrt( 2.0/( 1.0 + c ) );
        break;
    case AzimuthalEquidistant:
        {
            double crad = acos(c);
            k = crad/sin(crad);
            break;
        }
    case Orthographic:
        k = 1.0;
        break;
    case Stereographic:
        k = 2.0/(1.0 + c);
        break;
    case Gnomonic:
        k = 1.0/c;
        break;
    default: //should never get here
        kWarning() << i18n("Unrecognized coordinate projection: ") << Options::projection() ;
        k = 1.0;  //just default to Orthographic
        break;
    }

    p.setX( 0.5*Width  - zoomscale*k*cosY*sindX );
    p.setY( 0.5*Height - zoomscale*k*( cosY0*sinY - sinY0*cosY*cosdX ) );

    return p;
}

QPoint SkyMap::toScreenI( SkyPoint *o, bool oRefract, bool *onVisibleHemisphere) {
    return toScreen( o, oRefract, onVisibleHemisphere ).toPoint();
}

QRect SkyMap::scaledRect() {
    return QRect( 0, 0, int(m_Scale*width()), int(m_Scale*height()) );
}

bool SkyMap::onScreen( QPoint &point ) {
    return scaledRect().contains( point );
}

bool SkyMap::onScreen( QPointF &pointF ) {
    return scaledRect().contains( pointF.toPoint() );
}

bool SkyMap::onScreen( QPointF &p1, QPointF &p2 ) {
    if ( ( p1.x() < 0        && p2.x() < 0 ) ||
            ( p1.y() < 0        && p2.y() < 0 ) ||
            ( p1.x() > scaledRect().width() && 
              p2.x() > scaledRect().width() ) ||
            ( p1.y() > scaledRect().height() && 
              p2.y() > scaledRect().height() ) ) return false;
    return true;
}

bool SkyMap::onScreen( QPoint &p1, QPoint &p2 ) {
    if ( ( p1.x() < 0        && p2.x() < 0 ) ||
            ( p1.y() < 0        && p2.y() < 0 ) ||
            ( p1.x() > scaledRect().width() && 
              p2.x() > scaledRect().width() ) ||
            ( p1.y() > scaledRect().height() && 
              p2.y() > scaledRect().height() ) ) return false;
    return true;
}



//Return the on-screen portion of the given SkyLine, if any portion of it
//is onscreen
QList<QPointF> SkyMap::toScreen( SkyLine *line, bool oRefract, bool doClipLines ) {
    QList<QPointF> screenLine;

    //Initialize spLast to the first point
    SkyPoint *spLast = line->point(0);
    bool on(false), onLast(false); //on-screen flags

    foreach ( SkyPoint *sp, line->points() ) {
        QPointF p = toScreen( sp, oRefract, &on );
        QPointF pLast = toScreen( spLast, oRefract,&onLast );

        //Make sure the point is not null
        if ( ! isPointNull( p ) ) {

            //If either this point or the previous are offscreen
            //and the user wants clipped lines, then we have to
            //interpolate to find the intersection of the line
            //segment with the screen edge
            if ( doClipLines ) {
                onscreenLine( p, pLast );
                if ( ! isPointNull( p ) ) {
                    screenLine.append( pLast );
                    screenLine.append( p );
                }
            }
            //If the current point is onscreen, add it to the list
            else if ( on ) {
                //First, add pLast if it is offscreen
                if ( !onLast )
                    screenLine.append( pLast );

                screenLine.append( p );
            }
        }

        spLast = sp;
    }

    return screenLine;
}

bool SkyMap::onscreenLine2( QPointF &p1, QPointF &p2 ) {
    //If the SkyMap rect contains both points or either point is null,
    //we can return immediately
    bool on1 = scaledRect().contains( p1.toPoint() );
    bool on2 = scaledRect().contains( p2.toPoint() );
    if ( on1 && on2 )
        return true;

    //Given two points defining a line segment, determine the
    //endpoints of the segment which is clipped by the boundaries
    //of the SkyMap QRectF.
    QLineF screenLine( p1, p2 );

    //Define screen edges to be just beyond the scaledRect() bounds, so that clipped
    //positions are considered "offscreen"
    QPoint topLeft( scaledRect().left()-1, scaledRect().top()-1 );
    QPoint bottomLeft( scaledRect().left()-1, scaledRect().top() + height()+1 );
    QPoint topRight( scaledRect().left() + scaledRect().width()+1, scaledRect().top()-1 );
    QPoint bottomRight( scaledRect().left() + scaledRect().width()+1, scaledRect().top() + height()+1 );
    QLine topEdge( topLeft, topRight );
    QLine bottomEdge( bottomLeft, bottomRight );
    QLine leftEdge( topLeft, bottomLeft );
    QLine rightEdge( topRight, bottomRight );

    QPointF edgePoint1;
    QPointF edgePoint2;

    //If both points are offscreen in the same direction, return a null point.
    if ( ( p1.x() <= topLeft.x()    && p2.x() <= topLeft.x() ) ||
            ( p1.y() <= topLeft.y()    && p2.y() <= topLeft.y() ) ||
            ( p1.x() >= topRight.x()   && p2.x() >= topRight.x() ) ||
            ( p1.y() >= bottomLeft.y() && p2.y() >= bottomLeft.y() ) ) {
        return false;
    }

    //When an intersection betwen the line and a screen edge is found, the
    //intersection point is stored in edgePoint2.
    //If two intersection points are found for the same line, then we'll
    //return the line joining those two intersection points.
    if ( screenLine.intersect( QLineF(topEdge), &edgePoint1 ) == 1 ) {
        edgePoint2 = edgePoint1;
    }

    if ( screenLine.intersect( QLineF(leftEdge), &edgePoint1 ) == 1 ) {
        if ( edgePoint2.isNull() )
            edgePoint2 = edgePoint1;
        else {
            //Two intersection points found.  Return this line segment
            //First make sure that edgePoint1 corresponds to p1
            if ( ( p1.x() < p2.x() ) == ( edgePoint1.x() < edgePoint2.x() ) ) {
                p1 = edgePoint1;
                p2 = edgePoint2;
            } else {
                p1 = edgePoint2;
                p2 = edgePoint1;
            }
            return true;
        }
    }

    if ( screenLine.intersect( QLineF(rightEdge), &edgePoint1 ) == 1 ) {
        if ( edgePoint2.isNull() )
            edgePoint2 = edgePoint1;
        else {
            //Two intersection points found.  Return this line segment
            //First make sure that edgePoint1 corresponds to p1
            if ( ( p1.x() < p2.x() ) == ( edgePoint1.x() < edgePoint2.x() ) ) {
                p1 = edgePoint1;
                p2 = edgePoint2;
            } else {
                p1 = edgePoint2;
                p2 = edgePoint1;
            }
            return true;
        }
    }
    if ( screenLine.intersect( QLineF(bottomEdge), &edgePoint1 ) == 1 ) {
        if ( edgePoint2.isNull() )
            edgePoint2 = edgePoint1;
        else {
            //Two intersection points found.  Return this line segment
            //First make sure that edgePoint1 corresponds to p1
            if ( ( p1.x() < p2.x() ) == ( edgePoint1.x() < edgePoint2.x() ) ) {
                p1 = edgePoint1;
                p2 = edgePoint2;
            } else {
                p1 = edgePoint2;
                p2 = edgePoint1;
            }
            return true;
        }
    }
    //If we get here, zero or one intersection point was found.
    //If no intersection points were found, the line must be totally offscreen
    //return a null point
    if ( edgePoint2.isNull() ) {
        return false;
    }

    //If one intersection point was found, then one of the original endpoints
    //was onscreen.  Return the line that connects this point to the edgePoint

    //edgePoint2 is the one defined edgePoint.
    if ( on2 )
        p1 = edgePoint2;
    else
        p2 = edgePoint2;

    return true;
}


void SkyMap::onscreenLine( QPointF &p1, QPointF &p2 ) {
    //If the SkyMap rect contains both points or either point is null,
    //we can return immediately
    if ( isPointNull( p1 ) || isPointNull( p2 ) )
        return;
    bool on1 = scaledRect().contains( p1.toPoint() );
    bool on2 = scaledRect().contains( p2.toPoint() );
    if ( on1 && on2 )
        return;

    //Given two points defining a line segment, determine the
    //endpoints of the segment which is clipped by the boundaries
    //of the SkyMap QRectF.
    QLineF screenLine( p1, p2 );

    //Define screen edges to be just beyond the scaledRect() bounds, so that clipped
    //positions are considered "offscreen"
    QPoint topLeft( scaledRect().left()-1, scaledRect().top()-1 );
    QPoint bottomLeft( scaledRect().left()-1, scaledRect().top() + height()+1 );
    QPoint topRight( scaledRect().left() + scaledRect().width()+1, scaledRect().top()-1 );
    QPoint bottomRight( scaledRect().left() + scaledRect().width()+1, scaledRect().top() + height()+1 );
    QLine topEdge( topLeft, topRight );
    QLine bottomEdge( bottomLeft, bottomRight );
    QLine leftEdge( topLeft, bottomLeft );
    QLine rightEdge( topRight, bottomRight );

    QPointF edgePoint1;
    QPointF edgePoint2;

    //If both points are offscreen in the same direction, return a null point.
    if ( ( p1.x() <= topLeft.x() && p2.x() <= topLeft.x() ) ||
            ( p1.y() <= topLeft.y() && p2.y() <= topLeft.y() ) ||
            ( p1.x() >= topRight.x() && p2.x() >= topRight.x() ) ||
            ( p1.y() >= bottomLeft.y() && p2.y() >= bottomLeft.y() ) ) {
        p1 = QPointF( -10000000., -10000000. );
        return;
    }

    //When an intersection betwen the line and a screen edge is found, the
    //intersection point is stored in edgePoint2.
    //If two intersection points are found for the same line, then we'll
    //return the line joining those two intersection points.
    if ( screenLine.intersect( QLineF(topEdge), &edgePoint1 ) == 1 ) {
        edgePoint2 = edgePoint1;
    }

    if ( screenLine.intersect( QLineF(leftEdge), &edgePoint1 ) == 1 ) {
        if ( edgePoint2.isNull() )
            edgePoint2 = edgePoint1;
        else {
            //Two intersection points found.  Return this line segment
            //First make sure that edgePoint1 corresponds to p1
            if ( ( p1.x() < p2.x() ) == ( edgePoint1.x() < edgePoint2.x() ) ) {
                p1 = edgePoint1;
                p2 = edgePoint2;
            } else {
                p1 = edgePoint2;
                p2 = edgePoint1;
            }
            return;
        }
    }

    if ( screenLine.intersect( QLineF(rightEdge), &edgePoint1 ) == 1 ) {
        if ( edgePoint2.isNull() )
            edgePoint2 = edgePoint1;
        else {
            //Two intersection points found.  Return this line segment
            //First make sure that edgePoint1 corresponds to p1
            if ( ( p1.x() < p2.x() ) == ( edgePoint1.x() < edgePoint2.x() ) ) {
                p1 = edgePoint1;
                p2 = edgePoint2;
            } else {
                p1 = edgePoint2;
                p2 = edgePoint1;
            }
            return;
        }
    }
    if ( screenLine.intersect( QLineF(bottomEdge), &edgePoint1 ) == 1 ) {
        if ( edgePoint2.isNull() )
            edgePoint2 = edgePoint1;
        else {
            //Two intersection points found.  Return this line segment
            //First make sure that edgePoint1 corresponds to p1
            if ( ( p1.x() < p2.x() ) == ( edgePoint1.x() < edgePoint2.x() ) ) {
                p1 = edgePoint1;
                p2 = edgePoint2;
            } else {
                p1 = edgePoint2;
                p2 = edgePoint1;
            }
            return;
        }
    }
    //If we get here, zero or one intersection point was found.
    //If no intersection points were found, the line must be totally offscreen
    //return a null point
    if ( edgePoint2.isNull() ) {
        p1 = QPointF( -10000000., -10000000. );
        return;
    }

    //If one intersection point was found, then one of the original endpoints
    //was onscreen.  Return the line that connects this point to the edgePoint

    //edgePoint2 is the one defined edgePoint.
    if ( on2 )
        p1 = edgePoint2;
    else
        p2 = edgePoint2;

    return;
}

// QLine SkyMap::toScreenI( SkyLine *line, double scale, bool oRefract, bool doClipLines ) {
// 	return toScreen( line, scale, oRefract, doClipLines ).toLine();
// }

SkyPoint SkyMap::fromScreen( const QPointF &p, dms *LST, const dms *lat ) {
    //Determine RA and Dec of a point, given (dx, dy): its pixel
    //coordinates in the SkyMap with the center of the map as the origin.
    SkyPoint result;
    double sinY0, cosY0, sinc, cosc;

    //Convert pixel position to x and y offsets in radians
    double dx = ( 0.5*width()  - p.x() )/Options::zoomFactor();
    double dy = ( 0.5*height() - p.y() )/Options::zoomFactor();

    //Special case: Equirectangular
    if ( Options::projection() == Equirectangular ) {
        if ( Options::useAltAz() ) {
            dms az, alt;
            dx = -1.0*dx;  //Azimuth goes in opposite direction compared to RA
            az.setRadians( dx + focus()->az()->radians() );
            alt.setRadians( dy + focus()->alt()->radians() );
            result.setAz( az.reduce() );
            if ( Options::useRefraction() ) alt.setD( refract( &alt, false ).Degrees() );  //find true alt from apparent alt
            result.setAlt( alt );
            result.HorizontalToEquatorial( LST, lat );
            return result;
        } else {
            dms ra, dec;
            ra.setRadians( dx + focus()->ra()->radians() );
            dec.setRadians( dy + focus()->dec()->radians() );
            result.set( ra.reduce(), dec );
            result.EquatorialToHorizontal( LST, lat );
            return result;
        }
    }

    double r  = sqrt( dx*dx + dy*dy );
    dms c;
    switch( Options::projection() ) {
    case Lambert:
        c.setRadians( 2.0*asin(0.5*r) );
        break;
    case AzimuthalEquidistant:
        c.setRadians( r );
        break;
    case Orthographic:
        c.setRadians( asin( r ) );
        break;
    case Stereographic:
        c.setRadians( 2.0*atan2( r, 2.0 ) );
        break;
    case Gnomonic:
        c.setRadians( atan( r ) );
        break;
    default: //should never get here
        kWarning() << i18n("Unrecognized coordinate projection: ") << Options::projection() ;
        c.setRadians( asin( r ) );  //just default to Orthographic
        break;
    }
    c.SinCos( sinc, cosc );

    if ( Options::useAltAz() ) {
        focus()->alt()->SinCos( sinY0, cosY0 );
        dx = -1.0*dx; //Azimuth goes in opposite direction compared to RA
    } else
        focus()->dec()->SinCos( sinY0, cosY0 );

    double Y, A, atop, abot; //A = atan( atop/abot )

    Y = asin( cosc*sinY0 + ( dy*sinc*cosY0 )/r );
    atop = dx*sinc;
    abot = r*cosY0*cosc - dy*sinY0*sinc;
    A = atan2( atop, abot );

    if ( Options::useAltAz() ) {
        dms alt, az;
        alt.setRadians( Y );
        az.setRadians( A + focus()->az()->radians() );
        if ( Options::useRefraction() ) alt.setD( refract( &alt, false ).Degrees() );  //find true alt from apparent alt
        result.setAlt( alt );
        result.setAz( az );
        result.HorizontalToEquatorial( LST, lat );
    } else {
        dms ra, dec;
        dec.setRadians( Y );
        ra.setRadians( A + focus()->ra()->radians() );
        result.set( ra.reduce(), dec );
        result.EquatorialToHorizontal( LST, lat );
    }

    return result;
}

dms SkyMap::refract( const dms *alt, bool findApparent ) {
    if ( alt->Degrees() <= -2.000 ) return dms( alt->Degrees() );

    int index = int( ( alt->Degrees() + 2.0 )*2. );  //RefractCorr arrays start at alt=-2.0 degrees.
    int index2( index + 1 );

    //compute dx, normalized distance from nearest position in lookup table
    double x1 = 0.5*float(index) - 1.75;
    if ( alt->Degrees()<x1 ) index2 = index - 1;
    if ( index2 < 0 ) index2 = index + 1;
    if ( index2 > 183 ) index2 = index - 1;

    //Failsafe: if indices are out of range, return the input altitude
    if ( index < 0 || index > 183 || index2 < 0 || index2 > 183 ) {
        return dms( alt->Degrees() );
    }

    double x2 = 0.5*float(index2) - 1.75;
    double dx = (alt->Degrees() - x1)/(x2 - x1);

    double y1 = RefractCorr1[index];
    double y2 = RefractCorr1[index2];
    if ( !findApparent ) {
        y1 = RefractCorr2[index];
        y2 = RefractCorr2[index2];
    }

    //linear interpolation to find refracted altitude
    dms result( alt->Degrees() + y2*dx + y1*(1.0-dx) );
    return result;
}

//---------------------------------------------------------------------------


// force a new calculation of the skymap (used instead of update(), which may skip the redraw)
// if now=true, SkyMap::paintEvent() is run immediately, rather than being added to the event queue
// also, determine new coordinates of mouse cursor.
void SkyMap::forceUpdate( bool now )
{
    QPoint mp( mapFromGlobal( QCursor::pos() ) );
    if (! unusablePoint ( mp )) {
        //determine RA, Dec of mouse pointer
        setMousePoint( fromScreen( mp, data->LST, data->geo()->lat() ) );
    }

    computeSkymap = true;
    if ( now ) repaint();
    else update();
}

float SkyMap::fov() {
    float diagonalPixels = sqrt( (double)(width() * width() + height() * height()) );
    return diagonalPixels / ( 2 * Options::zoomFactor() * dms::DegToRad );

    //if ( width() >= height() )
    //	return 28.65*width()/Options::zoomFactor();
    //else
    //	return 28.65*height()/Options::zoomFactor();
}

bool SkyMap::checkVisibility( SkyLine *sl ) {
    foreach ( SkyPoint *p, sl->points() )
    if ( checkVisibility( p ) )
        return true;

    return false;
}

bool SkyMap::checkVisibility( SkyPoint *p ) {
    //TODO deal with alternate projections
    double dX, dY;
    bool useAltAz = Options::useAltAz();

    //Skip objects below the horizon if using Horizontal coords,
    //and the ground is drawn
    if ( useAltAz && Options::showHorizon() && Options::showGround() && p->alt()->Degrees() < -1.0 ) return false;

    if ( useAltAz ) {
        if ( Options::useRefraction() ) 
            dY = fabs( refract( p->alt(), true ).Degrees() - focus()->alt()->Degrees() );
        else
            dY = fabs( p->alt()->Degrees() - focus()->alt()->Degrees() );
    } else {
        dY = fabs( p->dec()->Degrees() - focus()->dec()->Degrees() );
    }
    if ( isPoleVisible ) dY *= 0.75; //increase effective FOV when pole visible.
    if ( dY > fov() ) return false;
    if ( isPoleVisible ) return true;

    if ( useAltAz ) {
        dX = fabs( p->az()->Degrees() - focus()->az()->Degrees() );
    } else {
        dX = fabs( p->ra()->Degrees() - focus()->ra()->Degrees() );
    }
    if ( dX > 180.0 ) dX = 360.0 - dX; // take shorter distance around sky

    if ( dX < XRange ) {
        return true;
    } else {
        return false;
    }
}

bool SkyMap::unusablePoint( const QPointF &p )
{
    double r0 = 1.0;
    //r0 is the angular size of the sky horizon, in radians
    //See HorizonComponent::draw() for documentation of these values
    switch ( Options::projection() ) {
    case Lambert:
        r0 = 1.41421356; break;
    case AzimuthalEquidistant:
        r0 = 1.57079633; break;
    case Stereographic:
        r0 = 2.0; break;
    case Gnomonic:
        r0 = 6.28318531; break; //Gnomonic has an infinite horizon; this is 2*PI
    case Orthographic:
    default:
        break;
    }

    //If the zoom is high enough, all points are usable
    //The center-to-corner distance, in radians
    double r = 0.5*1.41421356*width() / Options::zoomFactor();
    if ( r < r0 ) {
        return false;
    }

    //At low zoom, we have to determine whether the point is beyond the sky horizon
    //Convert pixel position to x and y offsets in radians
    double dx = ( 0.5*width()  - p.x() )/Options::zoomFactor();
    double dy = ( 0.5*height() - p.y() )/Options::zoomFactor();
    double rsq = ( dx*dx + dy*dy );
    r0 = r0*r0;

    if (rsq < r0) {
        return false;
    }

    return true;
}

void SkyMap::setZoomMouseCursor()
{
    mouseMoveCursor = false;	// no mousemove cursor

    QPainter p;
    QPixmap cursorPix (32, 32); // size 32x32 (this size is compatible to all systems)
    // the center of the pixmap
    int mx = cursorPix.	width() / 2;
    int my = cursorPix.	height() / 2;

    cursorPix.fill ( Qt::white );  // white background
    p.begin (&cursorPix);
    p.setPen (QPen ( Qt::black, 2));	// black lines

    p.drawEllipse( mx - 7, my - 7, 14, 14 );
    p.drawLine( mx + 5, my + 5, mx + 11, my + 11 );
    p.end();

    // create a mask to make parts of the pixmap invisible
    QBitmap mask (32, 32);
    mask.fill ( Qt::color0 );	// all is invisible

    p.begin (&mask);
    // paint over the parts which should be visible
    p.setPen( QPen ( Qt::color1, 3 ) );
    p.drawEllipse( mx - 7, my - 7, 14, 14 );
    p.drawLine( mx + 5, my + 5, mx + 12, my + 12 );
    p.end();

    cursorPix.setMask (mask);	// set the mask
    QCursor cursor (cursorPix);
    setCursor (cursor);
}

void SkyMap::setDefaultMouseCursor()
{
    mouseMoveCursor = false;        // no mousemove cursor

    QPainter p;
    QPixmap cursorPix (32, 32); // size 32x32 (this size is compatible to all systems)
    // the center of the pixmap
    int mx = cursorPix.	width() / 2;
    int my = cursorPix.	height() / 2;

    cursorPix.fill ( Qt::white );  // white background
    p.begin (&cursorPix);
    p.setPen( QPen ( Qt::black, 2 ) );	// black lines
    // 1. diagonal
    p.drawLine (mx - 2, my - 2, mx - 8, mx - 8);
    p.drawLine (mx + 2, my + 2, mx + 8, mx + 8);
    // 2. diagonal
    p.drawLine (mx - 2, my + 2, mx - 8, mx + 8);
    p.drawLine (mx + 2, my - 2, mx + 8, mx - 8);
    p.end();

    // create a mask to make parts of the pixmap invisible
    QBitmap mask (32, 32);
    mask.fill( Qt::color0 );	// all is invisible

    p.begin (&mask);
    // paint over the parts which should be visible
    p.setPen( QPen( Qt::color1, 3 ) );
    // 1. diagonal
    p.drawLine (mx - 2, my - 2, mx - 8, mx - 8);
    p.drawLine (mx + 2, my + 2, mx + 8, mx + 8);
    // 2. diagonal
    p.drawLine (mx - 2, my + 2, mx - 8, mx + 8);
    p.drawLine (mx + 2, my - 2, mx + 8, mx - 8);
    p.end();

    cursorPix.setMask( mask );  // set the mask
    QCursor cursor( cursorPix );
    setCursor( cursor );
}

void SkyMap::setMouseMoveCursor()
{
    if (mouseButtonDown)
    {
        setCursor(Qt::SizeAllCursor);	// cursor shape defined in qt
        mouseMoveCursor = true;
    }
}

void SkyMap::addLink() {
    AddLinkDialog adialog( this, clickedObject()->name() );
    QString entry;
    QFile file;

    if ( adialog.exec()==QDialog::Accepted ) {
        if ( adialog.isImageLink() ) {
            //Add link to object's ImageList, and descriptive text to its ImageTitle list
            clickedObject()->ImageList().append( adialog.url() );
            clickedObject()->ImageTitle().append( adialog.desc() );

            //Also, update the user's custom image links database
            //check for user's image-links database.  If it doesn't exist, create it.
            file.setFileName( KStandardDirs::locateLocal( "appdata", "image_url.dat" ) ); //determine filename in local user KDE directory tree.

            if ( !file.open( QIODevice::ReadWrite | QIODevice::Append ) ) {
                QString message = i18n( "Custom image-links file could not be opened.\nLink cannot be recorded for future sessions." );
                KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
                return;
            } else {
                entry = clickedObject()->name() + ':' + adialog.desc() + ':' + adialog.url();
                QTextStream stream( &file );
                stream << entry << endl;
                file.close();
                emit linkAdded();
            }
        } else {
            clickedObject()->InfoList().append( adialog.url() );
            clickedObject()->InfoTitle().append( adialog.desc() );

            //check for user's image-links database.  If it doesn't exist, create it.
            file.setFileName( KStandardDirs::locateLocal( "appdata", "info_url.dat" ) ); //determine filename in local user KDE directory tree.

            if ( !file.open( QIODevice::ReadWrite | QIODevice::Append ) ) {
                QString message = i18n( "Custom information-links file could not be opened.\nLink cannot be recorded for future sessions." );						KMessageBox::sorry( 0, message, i18n( "Could not Open File" ) );
                return;
            } else {
                entry = clickedObject()->name() + ':' + adialog.desc() + ':' + adialog.url();
                QTextStream stream( &file );
                stream << entry << endl;
                file.close();
                emit linkAdded();
            }
        }
    }
}

void SkyMap::updateAngleRuler() {
    if(isAngleMode() && (!pmenu || !pmenu -> isVisible()))
        AngularRuler.setPoint( 1, mousePoint() );
    AngularRuler.update( data );
}

bool SkyMap::isSlewing() const  {
    return (slewing || ( clockSlewing && data->clock()->isActive() ) );
}

bool SkyMap::isPointNull( const QPointF &p ) {
    if ( p.x() < -100000. ) return true;
    return false;
}

#ifdef HAVE_XPLANET
void SkyMap::startXplanet( const QString & outputFile ) {
    QString year, month, day, hour, minute, seconde, fov;

    // If Options::xplanetPath() is empty, return
    if ( Options::xplanetPath().isEmpty() ) {
        KMessageBox::error(0, i18n("Xplanet binary path is empty in config panel."));
        return;
    }

    // Format date
    if ( year.setNum( data->ut().date().year() ).size() == 1 ) year.push_front( '0' );
    if ( month.setNum( data->ut().date().month() ).size() == 1 ) month.push_front( '0' );
    if ( day.setNum( data->ut().date().day() ).size() == 1 ) day.push_front( '0' );
    if ( hour.setNum( data->ut().time().hour() ).size() == 1 ) hour.push_front( '0' );
    if ( minute.setNum( data->ut().time().minute() ).size() == 1 ) minute.push_front( '0' );
    if ( seconde.setNum( data->ut().time().second() ).size() == 1 ) seconde.push_front( '0' );

    // Create xplanet process
    KProcess *xplanetProc = new KProcess;

    // Add some options
    *xplanetProc << Options::xplanetPath()
            << "-body" << clickedObject()->name().toLower() 
            << "-geometry" << Options::xplanetWidth() + 'x' + Options::xplanetHeight()
            << "-date" <<  year + month + day + '.' + hour + minute + seconde
            << "-glare" << Options::xplanetGlare()
            << "-base_magnitude" << Options::xplanetMagnitude()
            << "-light_time"
            << "-window";

    // General options
    if ( ! Options::xplanetTitle().isEmpty() )
        *xplanetProc << "-window_title" << "\"" + Options::xplanetTitle() + "\"";
    if ( Options::xplanetFOV() )
        *xplanetProc << "-fov" << fov.setNum( this->fov() ).replace( '.', ',' );
    if ( Options::xplanetConfigFile() )
        *xplanetProc << "-config" << Options::Options::xplanetConfigFilePath();
    if ( Options::xplanetStarmap() )
        *xplanetProc << "-starmap" << Options::xplanetStarmapPath();
    if ( Options::xplanetArcFile() )
        *xplanetProc << "-arc_file" << Options::Options::xplanetArcFilePath();
    if ( Options::xplanetWait() )
        *xplanetProc << "-wait" << Options::xplanetWaitValue();
    if ( !outputFile.isEmpty() )
        *xplanetProc << "-output" << outputFile << "-quality" << Options::xplanetQuality();

    // Labels
    if ( Options::xplanetLabel() ) {
        *xplanetProc << "-fontsize" << Options::xplanetFontSize()
                << "-color" << "0x" + Options::xplanetColor().mid( 1 )
                << "-date_format" << Options::xplanetDateFormat();

        if ( Options::xplanetLabelGMT() )
            *xplanetProc << "-gmtlabel";
        else
            *xplanetProc << "-label";
        if ( !Options::xplanetLabelString().isEmpty() )
            *xplanetProc << "-label_string" << "\"" + Options::xplanetLabelString() + "\"";
        if ( Options::xplanetLabelTL() )
            *xplanetProc << "-labelpos" << "+15+15";
        else if ( Options::xplanetLabelTR() )
            *xplanetProc << "-labelpos" << "-15+15";
        else if ( Options::xplanetLabelBR() )
            *xplanetProc << "-labelpos" << "-15-15";
        else if ( Options::xplanetLabelBL() )
            *xplanetProc << "-labelpos" << "+15-15";
    }

    // Markers
    if ( Options::xplanetMarkerFile() )
        *xplanetProc << "-marker_file" << Options::xplanetMarkerFilePath();
    if ( Options::xplanetMarkerBounds() )
        *xplanetProc << "-markerbounds" << Options::xplanetMarkerBoundsPath();

    // Position
    if ( Options::xplanetRandom() )
        *xplanetProc << "-random";
    else
        *xplanetProc << "-latitude" << Options::xplanetLatitude() << "-longitude" << Options::xplanetLongitude();

    // Projection
    if ( Options::xplanetProjection() ) {
        switch ( Options::xplanetProjection() ) {
            case 1 : *xplanetProc << "-projection" << "ancient"; break;
            case 2 : *xplanetProc << "-projection" << "azimuthal"; break;
            case 3 : *xplanetProc << "-projection" << "bonne"; break;
            case 4 : *xplanetProc << "-projection" << "gnomonic"; break;
            case 5 : *xplanetProc << "-projection" << "hemisphere"; break;
            case 6 : *xplanetProc << "-projection" << "lambert"; break;
            case 7 : *xplanetProc << "-projection" << "mercator"; break;
            case 8 : *xplanetProc << "-projection" << "mollweide"; break;
            case 9 : *xplanetProc << "-projection" << "orthographic"; break;
            case 10 : *xplanetProc << "-projection" << "peters"; break;
            case 11 : *xplanetProc << "-projection" << "polyconic"; break;
            case 12 : *xplanetProc << "-projection" << "rectangular"; break;
            case 13 : *xplanetProc << "-projection" << "tsc"; break;
            default : break;
        }
        if ( Options::xplanetBackground() ) {
            if ( Options::xplanetBackgroundImage() )
                *xplanetProc << "-background" << Options::xplanetBackgroundImagePath();
            else
                *xplanetProc << "-background" << "0x" + Options::xplanetBackgroundColorValue().mid( 1 );
        }
    }

    // We add this option at the end otherwise it does not work (???)
    *xplanetProc << "-origin" << "earth";

    // Run xplanet
    kWarning() << i18n( "Run : %1" , xplanetProc->program().join(" "));
    xplanetProc->start();
}

void SkyMap::slotXplanetToScreen() {
    startXplanet();
}

void SkyMap::slotXplanetToFile() {
    QString filename = KFileDialog::getSaveFileName( );
    if ( ! filename.isEmpty() ) {
        startXplanet( filename );
    }
}
#endif

#include "skymap.moc"
