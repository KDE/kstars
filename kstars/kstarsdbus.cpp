/***************************************************************************
                          kstarsdbus.cpp  -  description
                             -------------------
    begin                : Son Apr 7 2002
    copyright            : (C) 2002 by Thomas Kabelmann
    email                : tk78@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

//KStars DBUS functions

#include <QDir>
#include <QPixmap>
#include <QKeySequence>
#include <QPainter>

#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrintDialog>
#include <QtSvg/QSvgGenerator>
#include <QXmlStreamWriter>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QLineEdit>

#include <KActionCollection>

#include "colorscheme.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksdssdownloader.h"
#include "skymap.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/starobject.h"
#include "skyobjects/deepskyobject.h"
#include "skyobjects/ksplanetbase.h"
#include "skycomponents/skymapcomposite.h"
#include "simclock.h"
#include "Options.h"
#include "imageexporter.h"
#include "skycomponents/constellationboundarylines.h"
#include "observinglist.h"
#include "eyepiecefield.h"

#ifdef HAVE_CFITSIO
#include "fitsviewer/fitsviewer.h"
#endif

void KStars::setRaDec( double ra, double dec ) {
    SkyPoint p( ra, dec );
    map()->setDestination( p );
}

void KStars::setAltAz( double alt, double az ) {
    map()->setDestinationAltAz( dms(alt), dms(az) );
}

void KStars::lookTowards ( const QString &direction ) {
    QString dir = direction.toLower();
    if        (dir == i18n("zenith") || dir=="z") {
        actionCollection()->action("zenith")->trigger();
    } else if (dir == i18n("north")  || dir=="n") {
        actionCollection()->action("north")->trigger();
    } else if (dir == i18n("east")   || dir=="e") {
        actionCollection()->action("east")->trigger();
    } else if (dir == i18n("south")  || dir=="s") {
        actionCollection()->action("south")->trigger();
    } else if (dir == i18n("west")   || dir=="w") {
        actionCollection()->action("west")->trigger();
    } else if (dir == i18n("northeast") || dir=="ne") {
        map()->stopTracking();
        map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 45.0 );
        map()->clickedPoint()->HorizontalToEquatorial( data()->lst(), data()->geo()->lat() );
        map()->slotCenter();
    } else if (dir == i18n("southeast") || dir=="se") {
        map()->stopTracking();
        map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 135.0 );
        map()->clickedPoint()->HorizontalToEquatorial( data()->lst(), data()->geo()->lat() );
        map()->slotCenter();
    } else if (dir == i18n("southwest") || dir=="sw") {
        map()->stopTracking();
        map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 225.0 );
        map()->clickedPoint()->HorizontalToEquatorial( data()->lst(), data()->geo()->lat() );
        map()->slotCenter();
    } else if (dir == i18n("northwest") || dir=="nw") {
        map()->stopTracking();
        map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 315.0 );
        map()->clickedPoint()->HorizontalToEquatorial( data()->lst(), data()->geo()->lat() );
        map()->slotCenter();
    } else {
        SkyObject *target = data()->objectNamed( direction );
        if ( target != NULL ) {
            map()->setClickedObject( target );
            map()->setClickedPoint( target );
            map()->slotCenter();
        }
    }
}

void KStars::addLabel( const QString &name ) {
    SkyObject *target = data()->objectNamed( name );
    if ( target != NULL ) {
        data()->skyComposite()->addNameLabel( target );
        map()->forceUpdate();
    }
}

void KStars::removeLabel( const QString &name ) {
    SkyObject *target = data()->objectNamed( name );
    if ( target != NULL ) {
        data()->skyComposite()->removeNameLabel( target );
        map()->forceUpdate();
    }
}

void KStars::addTrail( const QString &name ) {
    TrailObject *target = dynamic_cast<TrailObject*>( data()->objectNamed( name ) );
    if( target ) {
        target->addToTrail();
        map()->forceUpdate();
    }
}

void KStars::removeTrail( const QString &name ) {
    TrailObject *target = dynamic_cast<TrailObject*>( data()->objectNamed( name ) );
    if ( target ) {
        target->clearTrail();
        map()->forceUpdate();
    }
}

void KStars::zoom( double z ) {
    map()->setZoomFactor( z );
}

void KStars::zoomIn() {
    map()->slotZoomIn();
}

void KStars::zoomOut() {
    map()->slotZoomOut();
}

void KStars::defaultZoom() {
    map()->slotZoomDefault();
}

void KStars::setLocalTime(int yr, int mth, int day, int hr, int min, int sec) {
    data()->changeDateTime( data()->geo()->LTtoUT( KStarsDateTime( QDate(yr, mth, day), QTime(hr,min,sec) ) ) );
}

void KStars::setTimeToNow() {
    slotSetTimeToNow();
}

void KStars::waitFor( double sec ) {
    QTime tm;
    tm.start();
    while ( tm.elapsed() < int( 1000.*sec ) ) { qApp->processEvents(); }
}

void KStars::waitForKey( const QString &k ) {
    data()->resumeKey = QKeySequence::fromString( k );
    if ( ! data()->resumeKey.isEmpty() ) {
        //When the resumeKey is pressed, resumeKey is set to empty
        while ( !data()->resumeKey.isEmpty() )
            qApp->processEvents();
    } else {
        qDebug() << "Error [D-Bus waitForKey()]: Invalid key requested.";
    }
}

void KStars::setTracking( bool track ) {
    if ( track != Options::isTracking() ) slotTrack();
}

void KStars::popupMessage( int /*x*/, int /*y*/, const QString& /*message*/ ){
    //Show a small popup window at (x,y) with a text message
}

void KStars::drawLine( int /*x1*/, int /*y1*/, int /*x2*/, int /*y2*/, int /*speed*/ ) {
    //Draw a line on the skymap display
}

bool KStars::setGeoLocation( const QString &city, const QString &province, const QString &country ) {
    //Set the geographic location
    bool cityFound( false );

    foreach ( GeoLocation *loc, data()->geoList ) {
        if ( loc->translatedName() == city &&
                ( province.isEmpty() || loc->translatedProvince() == province ) &&
                loc->translatedCountry() == country ) {

            cityFound = true;

            data()->setLocation( *loc );

            //configure time zone rule
            KStarsDateTime ltime = loc->UTtoLT( data()->ut() );
            loc->tzrule()->reset_with_ltime( ltime, loc->TZ0(), data()->isTimeRunningForward() );
            data()->setNextDSTChange( loc->tzrule()->nextDSTChange() );

            //reset LST
            data()->syncLST();

            //make sure planets, etc. are updated immediately
            data()->setFullTimeUpdate();

            // If the sky is in Horizontal mode and not tracking, reset focus such that
            // Alt/Az remain constant.
            if ( ! Options::isTracking() && Options::useAltAz() ) {
                map()->focus()->HorizontalToEquatorial( data()->lst(), data()->geo()->lat() );
            }

            // recalculate new times and objects
            data()->setSnapNextFocus();
            updateTime();

            //no need to keep looking, we're done.
            break;
        }
    }

    if ( !cityFound ) {
        if ( province.isEmpty() )
            qDebug() << QString("Error [D-Bus setGeoLocation]: city %1, %2 not found in database.").arg(city).arg(country);
        else
            qDebug() << QString("Error [D-Bus setGeoLocation]: city %1, %2, %3 not found in database.").arg(city).arg(province).arg(country);
    }

    return cityFound;
}

void KStars::readConfig() {
    //Load config file values into Options object
    Options::self()->load();

    applyConfig();

    //Reset date, if one was stored
    if ( data()->StoredDate.isValid() ) {
        data()->changeDateTime( data()->geo()->LTtoUT( data()->StoredDate ) );
        data()->StoredDate = QDateTime(); //invalidate StoredDate
    }

    map()->forceUpdate();
}

void KStars::writeConfig() {
    Options::self()->save();

    //Store current simulation time
    data()->StoredDate = data()->lt();
}

QString KStars::getOption( const QString &name ) {
    //Some config items are not stored in the Options object while
    //the program is running; catch these here and returntheir current value.
    if ( name == "FocusRA" ) { return QString::number( map()->focus()->ra().Hours(), 'f', 6 ); }
    if ( name == "FocusDec" ) { return QString::number( map()->focus()->dec().Degrees(), 'f', 6 ); }

    KConfigSkeletonItem *it = Options::self()->findItem( name );
    if ( it ) return it->property().toString();
    else return QString();
}

void KStars::changeViewOption( const QString &op, const QString &val ) {
    bool bOk(false), dOk(false);

    //parse bool value
    bool bVal(false);
    if ( val.toLower() == "true" ) { bOk = true; bVal = true; }
    if ( val.toLower() == "false" ) { bOk = true; bVal = false; }
    if ( val == "1" ) { bOk = true; bVal = true; }
    if ( val == "0" ) { bOk = true; bVal = false; }

    //parse double value
    double dVal = val.toDouble( &dOk );

    //[GUI]
    if ( op == "ShowInfoBoxes"   && bOk ) Options::setShowInfoBoxes(   bVal );
    if ( op == "ShowTimeBox"     && bOk ) Options::setShowTimeBox(     bVal );
    if ( op == "ShowGeoBox"      && bOk ) Options::setShowGeoBox(      bVal );
    if ( op == "ShowFocusBox"    && bOk ) Options::setShowFocusBox(    bVal );
    if ( op == "ShadeTimeBox"    && bOk ) Options::setShadeTimeBox(    bVal );
    if ( op == "ShadeGeoBox"     && bOk ) Options::setShadeGeoBox(     bVal );
    if ( op == "ShadeFocusBox"   && bOk ) Options::setShadeFocusBox(   bVal );

    //[View]
    // FIXME: REGRESSION
//     if ( op == "FOVName"                ) Options::setFOVName(         val );
//     if ( op == "FOVSizeX"         && dOk ) Options::setFOVSizeX( (float)dVal );
//     if ( op == "FOVSizeY"         && dOk ) Options::setFOVSizeY( (float)dVal );
//     if ( op == "FOVShape"        && nOk ) Options::setFOVShape(       nVal );
//     if ( op == "FOVColor"               ) Options::setFOVColor(        val );
    if ( op == "ShowStars"       && bOk ) Options::setShowStars(      bVal );
    if ( op == "ShowMessier"     && bOk ) Options::setShowMessier(    bVal );
    if ( op == "ShowMessierImages" && bOk ) Options::setShowMessierImages( bVal );
    if ( op == "ShowNGC"         && bOk ) Options::setShowNGC( bVal );
    if ( op == "ShowIC"          && bOk ) Options::setShowIC(  bVal );
    if ( op == "ShowCLines"      && bOk ) Options::setShowCLines(   bVal );
    if ( op == "ShowCBounds"     && bOk ) Options::setShowCBounds(  bVal );
    if ( op == "ShowCNames"      && bOk ) Options::setShowCNames(   bVal );
    if ( op == "ShowMilkyWay"    && bOk ) Options::setShowMilkyWay( bVal );
    if ( op == "AutoSelectGrid"  && bOk ) Options::setAutoSelectGrid( bVal );
    if ( op == "ShowEquatorialGrid" && bOk ) Options::setShowEquatorialGrid( bVal );
    if ( op == "ShowHorizontalGrid" && bOk ) Options::setShowHorizontalGrid( bVal );
    if ( op == "ShowEquator"     && bOk ) Options::setShowEquator(  bVal );
    if ( op == "ShowEcliptic"    && bOk ) Options::setShowEcliptic( bVal );
    if ( op == "ShowHorizon"     && bOk ) Options::setShowHorizon(  bVal );
    if ( op == "ShowGround"      && bOk ) Options::setShowGround(   bVal );
    if ( op == "ShowSun"         && bOk ) Options::setShowSun(      bVal );
    if ( op == "ShowMoon"        && bOk ) Options::setShowMoon(     bVal );
    if ( op == "ShowMercury"     && bOk ) Options::setShowMercury(  bVal );
    if ( op == "ShowVenus"       && bOk ) Options::setShowVenus(    bVal );
    if ( op == "ShowMars"        && bOk ) Options::setShowMars(     bVal );
    if ( op == "ShowJupiter"     && bOk ) Options::setShowJupiter(  bVal );
    if ( op == "ShowSaturn"      && bOk ) Options::setShowSaturn(   bVal );
    if ( op == "ShowUranus"      && bOk ) Options::setShowUranus(   bVal );
    if ( op == "ShowNeptune"     && bOk ) Options::setShowNeptune(  bVal );
    //if ( op == "ShowPluto"       && bOk ) Options::setShowPluto(    bVal );
    if ( op == "ShowAsteroids"   && bOk ) Options::setShowAsteroids( bVal );
    if ( op == "ShowComets"      && bOk ) Options::setShowComets(  bVal );
    if ( op == "ShowSolarSystem" && bOk ) Options::setShowSolarSystem( bVal );
    if ( op == "ShowDeepSky"     && bOk ) Options::setShowDeepSky( bVal );
    if ( op == "ShowSupernovae"     && bOk ) Options::setShowSupernovae( bVal );
    if ( op == "ShowStarNames"      && bOk ) Options::setShowStarNames(      bVal );
    if ( op == "ShowStarMagnitudes" && bOk ) Options::setShowStarMagnitudes( bVal );
    if ( op == "ShowAsteroidNames"  && bOk ) Options::setShowAsteroidNames(  bVal );
    if ( op == "ShowCometNames"     && bOk ) Options::setShowCometNames(     bVal );
    if ( op == "ShowPlanetNames"    && bOk ) Options::setShowPlanetNames(    bVal );
    if ( op == "ShowPlanetImages"   && bOk ) Options::setShowPlanetImages(   bVal );
    if ( op == "HideOnSlew"  && bOk ) Options::setHideOnSlew(  bVal );
    if ( op == "HideStars"   && bOk ) Options::setHideStars(   bVal );
    if ( op == "HidePlanets" && bOk ) Options::setHidePlanets( bVal );
    if ( op == "HideMessier" && bOk ) Options::setHideMessier( bVal );
    if ( op == "HideNGC"     && bOk ) Options::setHideNGC(     bVal );
    if ( op == "HideIC"      && bOk ) Options::setHideIC(      bVal );
    if ( op == "HideMilkyWay" && bOk ) Options::setHideMilkyWay( bVal );
    if ( op == "HideCNames"  && bOk ) Options::setHideCNames(  bVal );
    if ( op == "HideCLines"  && bOk ) Options::setHideCLines(  bVal );
    if ( op == "HideCBounds" && bOk ) Options::setHideCBounds( bVal );
    if ( op == "HideGrids"    && bOk ) Options::setHideGrids( bVal );
    if ( op == "HideLabels"  && bOk ) Options::setHideLabels(  bVal );

    if ( op == "UseAltAz"         && bOk ) Options::setUseAltAz(      bVal );
    if ( op == "UseRefraction"    && bOk ) Options::setUseRefraction( bVal );
    if ( op == "UseAutoLabel"     && bOk ) Options::setUseAutoLabel(  bVal );
    if ( op == "UseHoverLabel"    && bOk ) Options::setUseHoverLabel( bVal );
    if ( op == "UseAutoTrail"     && bOk ) Options::setUseAutoTrail(  bVal );
    if ( op == "UseAnimatedSlewing"   && bOk ) Options::setUseAnimatedSlewing( bVal );
    if ( op == "FadePlanetTrails" && bOk ) Options::setFadePlanetTrails( bVal );
    if ( op == "SlewTimeScale"    && dOk ) Options::setSlewTimeScale(    dVal );
    if ( op == "ZoomFactor"       && dOk ) Options::setZoomFactor(       dVal );
    //    if ( op == "MagLimitDrawStar"     && dOk )    Options::setMagLimitDrawStar(    dVal );
    if ( op == "MagLimitDrawDeepSky"     && dOk ) Options::setMagLimitDrawDeepSky( dVal );
    if ( op == "StarDensity"         && dOk ) Options::setStarDensity( dVal );
    //    if ( op == "MagLimitDrawStarZoomOut" && dOk ) Options::setMagLimitDrawStarZoomOut(        dVal );
    if ( op == "MagLimitDrawDeepSkyZoomOut" && dOk ) Options::setMagLimitDrawDeepSkyZoomOut(  dVal );
    if ( op == "StarLabelDensity" && dOk ) Options::setStarLabelDensity( dVal );
    if ( op == "MagLimitHideStar"     && dOk ) Options::setMagLimitHideStar(     dVal );
    if ( op == "MagLimitAsteroid"     && dOk ) Options::setMagLimitAsteroid(     dVal );
    if ( op == "AsteroidLabelDensity" && dOk ) Options::setAsteroidLabelDensity( dVal );
    if ( op == "MaxRadCometName"      && dOk ) Options::setMaxRadCometName(      dVal );

    //these three are a "radio group"
    if ( op == "UseLatinConstellationNames" && bOk ) {
        Options::setUseLatinConstellNames( true );
        Options::setUseLocalConstellNames( false );
        Options::setUseAbbrevConstellNames( false );
    }
    if ( op == "UseLocalConstellationNames" && bOk ) {
        Options::setUseLatinConstellNames( false );
        Options::setUseLocalConstellNames( true );
        Options::setUseAbbrevConstellNames( false );
    }
    if ( op == "UseAbbrevConstellationNames" && bOk ) {
        Options::setUseLatinConstellNames( false );
        Options::setUseLocalConstellNames( false );
        Options::setUseAbbrevConstellNames( true );
    }

    map()->forceUpdate();
}

void KStars::setColor( const QString &name, const QString &value ) {
    ColorScheme *cs = data()->colorScheme();
    if ( cs->hasColorNamed( name ) ) {
        cs->setColor( name, value );
        map()->forceUpdate();
    }
}

void KStars::loadColorScheme( const QString &name ) {
    bool ok = data()->colorScheme()->load( name );
    QString filename = data()->colorScheme()->fileName();

    if ( ok ) {
        //set the application colors for the Night Vision scheme
        if ( Options::darkAppColors() == false && filename == "night.colors" )  {
            Options::setDarkAppColors( true );
            OriginalPalette = QApplication::palette();
            QApplication::setPalette( DarkPalette );
            //Note:  This uses style sheets to set the dark colors, this is cross platform.  Palettes have a different behavior on OS X and Windows as opposed to Linux.
            //It might be a good idea to use stylesheets in the future instead of palettes but this will work for now for OS X.
            //This is also in KStars.cpp.  If you change it, change it in BOTH places.
            #ifdef Q_OS_OSX
            qApp->setStyleSheet("QWidget { background-color: black; color:red; selection-background-color:rgb(30,30,30);selection-color:white}" \
                                "QToolBar { border:none }" \
                                "QTabBar::tab:selected { background-color:rgb(50,50,50) }" \
                                "QTabBar::tab:!selected { background-color:rgb(30,30,30) }" \
                                "QPushButton { background-color:rgb(50,50,50);border-width:1px; border-style:solid;border-color:black}" \
                                "QPushButton::disabled { background-color:rgb(10,10,10);border-width:1px; border-style:solid;border-color:black }" \
                                "QToolButton:Checked { background-color:rgb(30,30,30); border:none }" \
                                "QComboBox { background-color:rgb(30,30,30); }" \
                                "QComboBox::disabled { background-color:rgb(10,10,10) }" \
                                "QScrollBar::handle { background: rgb(30,30,30) }" \
                                "QSpinBox { border-width: 1px; border-style:solid; border-color:rgb(30,30,30) }" \
                                "QDoubleSpinBox { border-width:1px; border-style:solid; border-color:rgb(30,30,30) }" \
                                "QLineEdit { border-width: 1px; border-style: solid; border-color:rgb(30,30,30) }" \
                                "QCheckBox::indicator:unchecked { background-color:rgb(30,30,30);border-width:1px; border-style:solid;border-color:black }" \
                                "QCheckBox::indicator:checked { background-color:red;border-width:1px; border-style:solid;border-color:black }" \
                                "QRadioButton::indicator:unchecked { background-color:rgb(30,30,30) }" \
                                "QRadioButton::indicator:checked { background-color:red }" \
                                "QRoundProgressBar { alternate-background-color:black }" \
                                "QDateTimeEdit {background-color:rgb(30,30,30); border-width: 1px; border-style:solid; border-color:rgb(30,30,30) }" \
                                "QHeaderView { color:red;background-color:black }" \
                                "QHeaderView::Section { background-color:rgb(30,30,30) }" \
                                "QTableCornerButton::section{ background-color:rgb(30,30,30) }" \
                                "");
            #endif
        }

        if ( Options::darkAppColors() && filename != "night.colors" ) {
            Options::setDarkAppColors( false );
            QApplication::setPalette( OriginalPalette );
            #ifdef Q_OS_OSX
            qApp->setStyleSheet("QRoundProgressBar { background-color: rgb(208,208,208) }" \
                                "");
            #endif
        }

        Options::setColorSchemeFile( name );

        map()->forceUpdate();
    }
}

void KStars::exportImage( const QString &url, int w, int h, bool includeLegend ) {

    ImageExporter*  m_ImageExporter = m_KStarsData->imageExporter();
    if ( w <= 0 )
        w = map()->width();
    if ( h <= 0 )
        h = map()->height();
    m_ImageExporter->includeLegend( includeLegend );
    m_ImageExporter->setRasterOutputSize( new QSize( w, h ) );
    m_ImageExporter->exportImage( url );
}

QString KStars::getDSSURL( const QString &objectName ) {
    SkyObject *target = data()->objectNamed( objectName );
    if ( !target ) {
        return QString( "ERROR" );
    }
    else {
        return KSDssDownloader::getDSSURL( target );
    }
}

QString KStars::getDSSURL( double RA_J2000, double Dec_J2000, float width, float height ) {
    dms ra( RA_J2000 ),  dec( Dec_J2000 );
    return KSDssDownloader::getDSSURL( ra, dec, width, height );
}

QString KStars::getObjectDataXML( const QString &objectName ) {
    SkyObject *target = data()->objectNamed( objectName );
    if ( !target ) {
        return QString( "<xml></xml>" );
    }
    QString output;
    QXmlStreamWriter stream( &output );
    stream.setAutoFormatting( true );
    stream.writeStartDocument();
    stream.writeStartElement( "object" );
    stream.writeTextElement( "Name", target->name() );
    stream.writeTextElement( "Alt_Name", target->name2() );
    stream.writeTextElement( "Long_Name", target->longname() );
    stream.writeTextElement( "Constellation", KStarsData::Instance()->skyComposite()->constellationBoundary()->constellationName( target ) );
    stream.writeTextElement( "RA_Dec_Epoch_JD", QString::number( target->getLastPrecessJD(), 'f', 3 ) );
    stream.writeTextElement( "RA_HMS", target->ra().toHMSString() );
    stream.writeTextElement( "Dec_DMS", target->dec().toDMSString() );
    stream.writeTextElement( "RA_J2000_HMS", target->ra0().toHMSString() );
    stream.writeTextElement( "Dec_J2000_DMS", target->dec0().toDMSString() );
    stream.writeTextElement( "RA_Degrees", QString::number( target->ra().Degrees() ) );
    stream.writeTextElement( "Dec_Degrees", QString::number( target->dec().Degrees() ) );
    stream.writeTextElement( "RA_J2000_Degrees", QString::number( target->ra0().Degrees() ) );
    stream.writeTextElement( "Dec_J2000_Degrees", QString::number( target->dec0().Degrees() ) );
    stream.writeTextElement( "Type", target->typeName() );
    stream.writeTextElement( "Magnitude", QString::number( target->mag(), 'g', 2 ) );
    stream.writeTextElement( "Position_Angle", QString::number( target->pa(), 'g', 3 ) );
    StarObject *star = dynamic_cast< StarObject* >( target );
    DeepSkyObject *dso = dynamic_cast< DeepSkyObject* >( target );
    if ( star ) {
        stream.writeTextElement( "Spectral_Type",  star->sptype() );
        stream.writeTextElement( "Genetive_Name", star->gname() );
        stream.writeTextElement( "Greek_Letter", star->greekLetter() );
        stream.writeTextElement( "Proper_Motion", QString::number( star->pmMagnitude() ) );
        stream.writeTextElement( "Proper_Motion_RA", QString::number( star->pmRA() ) );
        stream.writeTextElement( "Proper_Motion_Dec", QString::number( star->pmDec() ) );
        stream.writeTextElement( "Parallax_mas", QString::number( star->parallax() ) );
        stream.writeTextElement( "Distance_pc", QString::number( star->distance() ) );
        stream.writeTextElement( "Henry_Draper", QString::number( star->getHDIndex() ) );
        stream.writeTextElement( "BV_Index", QString::number( star->getBVIndex() ) );
    }
    else if ( dso ) {
        stream.writeTextElement( "Catalog", dso->catalog() );
        stream.writeTextElement( "Major_Axis", QString::number( dso->a() ) );
        stream.writeTextElement( "Minor_Axis", QString::number( dso->a() * dso->e() ) );
    }
    stream.writeEndElement(); // object
    stream.writeEndDocument();
    return output;
}

QString KStars::getObjectPositionInfo( const QString &objectName ) {
    Q_ASSERT( data() );
    const SkyObject *obj = data()->objectNamed( objectName ); // make sure we work with a clone
    if ( !obj ) {
        return QString( "<xml></xml>" );
    }
    SkyObject *target = obj->clone();
    if( !target ) { // should not happen
        qWarning() << "ERROR: Could not clone SkyObject " << objectName << "!";
        return QString( "<xml></xml>" );
    }

    const KSNumbers *updateNum = data()->updateNum();
    const KStarsDateTime ut = data()->ut();
    const GeoLocation *geo = data()->geo();
    QString riseTimeString, setTimeString, transitTimeString;
    QString riseAzString, setAzString, transitAltString;

    // Make sure the coordinates of the SkyObject are updated
    target->updateCoords( updateNum, true, geo->lat(), data()->lst(), true );
    target->EquatorialToHorizontal( data()->lst(), geo->lat() );

    // Compute rise, set and transit times and parameters -- Code pulled from DetailDialog
    QTime riseTime = target->riseSetTime( ut, geo, true ); //true = use rise time
    dms riseAz = target->riseSetTimeAz( ut, geo, true ); //true = use rise time
    QTime transitTime = target->transitTime( ut, geo );
    dms transitAlt = target->transitAltitude( ut, geo );
    if ( transitTime < riseTime ) {
        transitTime = target->transitTime( ut.addDays( 1 ), geo );
        transitAlt = target->transitAltitude( ut.addDays( 1 ), geo );
    }
    //If set time is before rise time, use set time for tomorrow
    QTime setTime = target->riseSetTime(  ut, geo, false ); //false = use set time
    dms setAz = target->riseSetTimeAz( ut, geo, false ); //false = use set time
    if ( setTime < riseTime ) {
        setTime = target->riseSetTime( ut.addDays( 1 ), geo, false ); //false = use set time
        setAz = target->riseSetTimeAz( ut.addDays( 1 ), geo, false ); //false = use set time
    }
    if ( riseTime.isValid() ) {
        riseTimeString = QString().sprintf( "%02d:%02d", riseTime.hour(), riseTime.minute() );
        setTimeString = QString().sprintf( "%02d:%02d", setTime.hour(), setTime.minute() );
        riseAzString = riseAz.toDMSString( true, true );
        setAzString = setAz.toDMSString( true, true );
    } else {
        if ( target->alt().Degrees() > 0.0 ) {
            riseTimeString = setTimeString = QString( "Circumpolar" );
        } else {
            riseTimeString = setTimeString = QString( "Never Rises" );
        }
        riseAzString = setAzString = QString( "N/A" );
    }

    transitTimeString = QString().sprintf( "%02d:%02d", transitTime.hour(), transitTime.minute() );
    transitAltString = transitAlt.toDMSString( true, true );

    QString output;
    QXmlStreamWriter stream( &output );
    stream.setAutoFormatting( true );
    stream.writeStartDocument();
    stream.writeStartElement( "object" );
    stream.writeTextElement( "Name", target->name() );
    stream.writeTextElement( "RA_Dec_Epoch_JD", QString::number( target->getLastPrecessJD(), 'f', 3 ) );
    stream.writeTextElement( "AltAz_JD", QString::number( data()->ut().djd(), 'f', 3 ) );
    stream.writeTextElement( "RA_HMS", target->ra().toHMSString(true) );
    stream.writeTextElement( "Dec_DMS", target->dec().toDMSString(true, true) );
    stream.writeTextElement( "RA_J2000_HMS", target->ra0().toHMSString(true) );
    stream.writeTextElement( "Dec_J2000_DMS", target->dec0().toDMSString(true, true) );
    stream.writeTextElement( "RA_Degrees", QString::number( target->ra().Degrees() ) );
    stream.writeTextElement( "Dec_Degrees", QString::number( target->dec().Degrees() ) );
    stream.writeTextElement( "RA_J2000_Degrees", QString::number( target->ra0().Degrees() ) );
    stream.writeTextElement( "Dec_J2000_Degrees", QString::number( target->dec0().Degrees() ) );
    stream.writeTextElement( "Altitude_DMS", target->alt().toDMSString(true, true) );
    stream.writeTextElement( "Azimuth_DMS", target->az().toDMSString(true, true) );
    stream.writeTextElement( "Altitude_Degrees", QString::number( target->alt().Degrees() ) );
    stream.writeTextElement( "Azimuth_Degrees", QString::number( target->az().Degrees() ) );
    stream.writeTextElement( "Rise", riseTimeString );
    stream.writeTextElement( "Rise_Az_DMS", riseAzString );
    stream.writeTextElement( "Set", setTimeString );
    stream.writeTextElement( "Set_Az_DMS", setAzString );
    stream.writeTextElement( "Transit", transitTimeString );
    stream.writeTextElement( "Transit_Alt_DMS", transitAltString );
    stream.writeTextElement( "Time_Zone_Offset", QString().sprintf( "%02.2f", geo->TZ() ) );

    stream.writeEndElement(); // object
    stream.writeEndDocument();
    return output;
}

void KStars::renderEyepieceView( const QString &objectName, const QString &destPathChart, const double fovWidth, const double fovHeight, const double rotation, const double scale,
                                const bool flip, const bool invert, QString imagePath, const QString &destPathImage, const bool overlay, const bool invertColors ) {
    const SkyObject *obj = data()->objectNamed( objectName );
    if ( !obj ) {
        qWarning() << "Object named " << objectName << " was not found!";
        return;
    }
    SkyObject *target = obj->clone();
    const KSNumbers *updateNum = data()->updateNum();
    const KStarsDateTime ut = data()->ut();
    const GeoLocation *geo = data()->geo();
    QPixmap *renderChart = new QPixmap();
    QPixmap *renderImage = 0;
    QTemporaryFile tempFile;
    if(  overlay || ( !destPathImage.isEmpty() ) ) {
        if( !QFile::exists( imagePath ) ) {
            // We must download a DSS image
            tempFile.open();
            QEventLoop loop;
            std::function<void( bool )> slot = [ &loop ]( bool unused ) {
                Q_UNUSED( unused );
                loop.quit();
            };
            new KSDssDownloader( target, tempFile.fileName(), slot, this );
            qDebug() << "DSS download requested. Waiting for download to complete...";
            loop.exec(); // wait for download to complete
            imagePath = tempFile.fileName();
        }
        if( QFile::exists( imagePath ) ) // required because DSS Download may fail
            renderImage = new QPixmap();
    }

    // Make sure the coordinates of the SkyObject are updated
    target->updateCoords( updateNum, true, geo->lat(), data()->lst(), true );
    target->EquatorialToHorizontal( data()->lst(), geo->lat() );

    EyepieceField::renderEyepieceView( target, renderChart, fovWidth, fovHeight, rotation, scale, flip, invert,
                                       imagePath, renderImage, overlay, invertColors );
    renderChart->save( destPathChart );
    delete renderChart;
    if( renderImage ) {
        renderImage->save( destPathImage );
        delete renderImage;
    }
}
QString KStars::getObservingWishListObjectNames() {
    QString output;
    foreach( QSharedPointer<SkyObject> object,  KStarsData::Instance()->observingList()->obsList() ) {
        output.append( object->name() + '\n' );
    }
    return output;
}

QString KStars::getObservingSessionPlanObjectNames() {
    QString output;
    foreach( QSharedPointer<SkyObject> object,  KStarsData::Instance()->observingList()->sessionList() ) {
        output.append( object->name() + '\n' );
    }
    return output;
}

void KStars::setApproxFOV( double FOV_Degrees ) {
    zoom( map()->width() / ( FOV_Degrees * dms::DegToRad ) );
}

QString KStars::getSkyMapDimensions() {
    return ( QString::number( map()->width() ) + 'x' + QString::number( map()->height() ) );
}
void KStars::printImage( bool usePrintDialog, bool useChartColors ) {
    //QPRINTER_FOR_NOW
//    KPrinter printer( true, QPrinter::HighResolution );
    QPrinter printer( QPrinter::HighResolution );
    printer.setFullPage( false );

    //Set up the printer (either with the Print Dialog,
    //or using the default settings)
    bool ok( false );
    if ( usePrintDialog ) {
        //QPRINTER_FOR_NOW
//        ok = printer.setup( this, i18n("Print Sky") );
        //QPrintDialog *dialog = KdePrint::createPrintDialog(&printer, this);
        QPrintDialog *dialog = new QPrintDialog(&printer, this);
        dialog->setWindowTitle( i18n("Print Sky") );
        if ( dialog->exec() == QDialog::Accepted )
            ok = true;
        delete dialog;
    } else {
        //QPRINTER_FOR_NOW
//        ok = printer.autoConfigure();
        ok = true;
    }

    if( ok ) {
        QApplication::setOverrideCursor( Qt::WaitCursor );

        //Save current ColorScheme file name and switch to Star Chart
        //scheme (if requested)
        QString schemeName = data()->colorScheme()->fileName();
        if ( useChartColors ) {
            loadColorScheme( "chart.colors" );
        }

        map()->setupProjector();
        map()->exportSkyImage( &printer, true );

        //Restore old color scheme if necessary
        //(if printing was aborted, the ColorScheme is still restored)
        if ( useChartColors ) {
            loadColorScheme( schemeName );
            map()->forceUpdate();
        }

        QApplication::restoreOverrideCursor();
    }
}

bool KStars::openFITS(const QString &imageURL)
{
    QUrl url(imageURL);
    return openFITS(url);
}

bool KStars::openFITS(const QUrl &imageURL)
{
    #ifndef HAVE_CFITSIO
    qWarning() << "KStars does not support loading FITS. Please recompile KStars with FITS support.";
    return false;
    #else
    FITSViewer * fv = NULL;
    if (Options::singleWindowOpenedFITS())
        fv = genericFITSViewer();
    else
        fv = new FITSViewer((Options::independentWindowFITS()) ? NULL : this);
    // Error opening file
    if (fv->addFITS(&imageURL) == -2)
    {
        delete (fv);
        return false;
    }
    else
    {
       fv->show();
       return true;
    }
    #endif
}

