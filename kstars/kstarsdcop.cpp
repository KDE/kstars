
/***************************************************************************
                          kstarsdcop.cpp  -  description
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

//KStars DCOP functions

#include <QDir>
#include <QPixmap>
#include <QKeySequence>
#include <QPainter>
//QPRINTER_FOR_NOW
#include <QPrinter>
#include <QPrintDialog>
#include <QSvgGenerator>

//QPRINTER_FOR_NOW
//#include <kprinter.h>
#include <kdeprintdialog.h>
#include <kpushbutton.h>
#include <klineedit.h>
#include <knuminput.h>
#include <kactioncollection.h>

#include "colorscheme.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/ksplanetbase.h"
#include "skycomponents/skymapcomposite.h"
#include "simclock.h"
#include "Options.h"

#include "dialogs/exportimagedialog.h"

// INDI includes
#include <config-kstars.h>

#ifdef HAVE_INDI_H
#include "indi/indidriver.h"
#include "indi/indimenu.h"
#include "indi/indielement.h"
#include "indi/indidevice.h"
#include "indi/indiproperty.h"
#include "indi/devicemanager.h"
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
    if        (dir == "zenith" || dir=="z") {
        actionCollection()->action("zenith")->trigger();
    } else if (dir == "north"  || dir=="n") {
        actionCollection()->action("north")->trigger();
    } else if (dir == "east"   || dir=="e") {
        actionCollection()->action("east")->trigger();
    } else if (dir == "south"  || dir=="s") {
        actionCollection()->action("south")->trigger();
    } else if (dir == "west"   || dir=="w") {
        actionCollection()->action("west")->trigger();
    } else if (dir == "northeast" || dir=="ne") {
        map()->stopTracking();
        map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 45.0 );
        map()->clickedPoint()->HorizontalToEquatorial( data()->lst(), data()->geo()->lat() );
        map()->slotCenter();
    } else if (dir == "southeast" || dir=="se") {
        map()->stopTracking();
        map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 135.0 );
        map()->clickedPoint()->HorizontalToEquatorial( data()->lst(), data()->geo()->lat() );
        map()->slotCenter();
    } else if (dir == "southwest" || dir=="sw") {
        map()->stopTracking();
        map()->clickedPoint()->setAlt( 15.0 ); map()->clickedPoint()->setAz( 225.0 );
        map()->clickedPoint()->HorizontalToEquatorial( data()->lst(), data()->geo()->lat() );
        map()->slotCenter();
    } else if (dir == "northwest" || dir=="nw") {
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
        kDebug() << i18n( "Error [D-Bus waitForKey()]: Invalid key requested." );
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

void KStars::setGeoLocation( const QString &city, const QString &province, const QString &country ) {
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
            kDebug() << i18n( "Error [D-Bus setGeoLocation]: city " ) << city << ", "
            << country << i18n( " not found in database." ) << endl;
        else
            kDebug() << i18n( "Error [D-Bus setGeoLocation]: city " ) << city << ", "
            << province << ", " << country << i18n( " not found in database." ) << endl;
    }
}

void KStars::readConfig() {
    //Load config file values into Options object
    Options::self()->readConfig();

    applyConfig();

    //Reset date, if one was stored
    if ( data()->StoredDate.isValid() ) {
        data()->changeDateTime( data()->geo()->LTtoUT( data()->StoredDate ) );
        data()->StoredDate = KDateTime(); //invalidate StoredDate
    }

    map()->forceUpdate();
}

void KStars::writeConfig() {
    Options::self()->writeConfig();

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
    bool bOk(false), nOk(false), dOk(false);

    //parse bool value
    bool bVal(false);
    if ( val.toLower() == "true" ) { bOk = true; bVal = true; }
    if ( val.toLower() == "false" ) { bOk = true; bVal = false; }
    if ( val == "1" ) { bOk = true; bVal = true; }
    if ( val == "0" ) { bOk = true; bVal = false; }

    //parse int value
    int nVal = val.toInt( &nOk );

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
    if ( op == "ShowPluto"       && bOk ) Options::setShowPluto(    bVal );
    if ( op == "ShowAsteroids"   && bOk ) Options::setShowAsteroids( bVal );
    if ( op == "ShowComets"      && bOk ) Options::setShowComets(  bVal );
    if ( op == "ShowSolarSystem" && bOk ) Options::setShowSolarSystem( bVal );
    if ( op == "ShowDeepSky"     && bOk ) Options::setShowDeepSky( bVal );
    if ( op == "ShowStarNames"      && bOk ) Options::setShowStarNames(      bVal );
    if ( op == "ShowStarMagnitudes" && bOk ) Options::setShowStarMagnitudes( bVal );
    if ( op == "ShowAsteroidNames"  && bOk ) Options::setShowAsteroidNames(  bVal );
    if ( op == "ShowCometNames"     && bOk ) Options::setShowCometNames(     bVal );
    if ( op == "ShowPlanetNames"    && bOk ) Options::setShowPlanetNames(    bVal );
    if ( op == "ShowPlanetImages"   && bOk ) Options::setShowPlanetImages(   bVal );
    if ( op == "HideOnSlew"  && bOk ) Options::setHideOnSlew(  bVal );
    if ( op == "ObsListSaveImage"  && bOk ) Options::setObsListSaveImage(  bVal );
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
        }

        if ( Options::darkAppColors() && filename != "night.colors" ) {
            Options::setDarkAppColors( false );
            QApplication::setPalette( OriginalPalette );
        }

        Options::setColorSchemeFile( name );

        map()->forceUpdate();
    }
}

void KStars::exportImage( const QString &url, int w, int h ) {
    // execute image export dialog
    if ( !imgExportDialog ) {
        imgExportDialog = new ExportImageDialog( url, QSize( w, h ) );
    } else {
        imgExportDialog->setOutputUrl( url );
        imgExportDialog->setOutputSize( QSize ( w, h ) );
    }

    imgExportDialog->show();
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
        QPrintDialog *dialog = KdePrint::createPrintDialog(&printer, this);
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
        map()->exportSkyImage( &printer );

        //Restore old color scheme if necessary
        //(if printing was aborted, the ColorScheme is still restored)
        if ( useChartColors ) {
            loadColorScheme( schemeName );
            map()->forceUpdate();
        }

        QApplication::restoreOverrideCursor();
    }
}

// TODO JM: INDI Scripting to be supported in KDE 4.1
#if 0
void KStars::startINDI (const QString &deviceName, bool useLocal)
{

    establishINDI();
    QList<QTreeWidgetItem *> found;

    if (!indidriver || !indimenu)
    {
        kDebug() << "establishINDI() failed.";
        return;
    }
    QTreeWidgetItem *driverItem = NULL;
    found = indidriver->ui->localTreeWidget->findItems(deviceName, Qt::MatchExactly | Qt::MatchRecursive);

    if (found.empty())
    {
        kDebug() << "Device " << deviceName << " not found!";
        return;
    }

    driverItem = found.first();

    // If device is already running, we need to shut it down first
    if (indidriver->isDeviceRunning(deviceName))
    {
        indidriver->ui->localTreeWidget->setCurrentItem(driverItem);
        indidriver->processDeviceStatus(1);
    }

    // Set custome label for device
    indimenu->setCustomLabel(deviceName);
    // Set DCOP device name
    indimenu->setCurrentDevice(deviceName);

    // Select it
    indidriver->ui->localTreeWidget->setCurrentItem(driverItem);

    // Start it either locally or as series
    if (useLocal)
        indidriver->ui->localR->setChecked(true);
    else
        indidriver->ui->serverR->setChecked(true);

    // Run it
    indidriver->processDeviceStatus(0);
}

void KStars::setINDIDevice (const QString &deviceName)
{

    if (!indidriver || !indimenu)
    {
        kDebug() << "establishINDI() failed.";
        return;
    }

    indimenu->setCurrentDevice(deviceName);

}

void KStars::shutdownINDI (const QString &deviceName)
{
    QList<QTreeWidgetItem *> found;

    if (!indidriver || !indimenu)
    {
        kDebug() << "establishINDI() failed.";
        return;
    }

    QTreeWidgetItem *driverItem = NULL;
    found = indidriver->ui->localTreeWidget->findItems(deviceName, Qt::MatchExactly | Qt::MatchRecursive);

    if (found.empty())
    {
        kDebug() << "Device " << deviceName << " not found!";
        return;
    }

    driverItem = found.first();

    indidriver->ui->localTreeWidget->setCurrentItem(driverItem);
    indidriver->processDeviceStatus(1);
}


void KStars::switchINDI(bool turnOn)
{
    INDI_D *dev;
    INDI_P *prop;

    if (!indidriver || !indimenu)
    {
        kDebug() << "switchINDI: establishINDI() failed.";
        return;
    }

    dev = indimenu->findDevice(indimenu->getCurrentDevice());
    if (!dev)
        dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
    if (!dev)
    {
        kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!";
        return;
    }

    if (turnOn && dev->isOn() || (!turnOn && !dev->isOn()))
        return;

    prop = dev->findProp("CONNECTION");
    if (!prop) return;

    if (turnOn)
        prop->newSwitch(0);
    else
        prop->newSwitch(1);

}


void KStars::setINDIPort(const QString &port)
{
    INDI_D *dev;
    INDI_P *prop;
    INDI_E *el;

    if (!indidriver || !indimenu)
    {
        kDebug() << "setINDIPort: establishINDI() failed.";
        return;
    }

    dev = indimenu->findDevice(indimenu->getCurrentDevice());
    if (!dev)
        dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
    if (!dev)
    {
        kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!";
        return;
    }

    prop = dev->findProp("DEVICE_PORT");
    if (!prop) return;

    el   = prop->findElement("PORT");

    if (!el || !el->write_w)
        return;

    el->write_w->setText(port);

    prop->newText();

}


void KStars::setINDITargetCoord(double RA, double DEC)
{
    INDI_D *dev;
    INDI_P *prop;
    INDI_E *el;

    if (!indidriver || !indimenu)
    {
        kDebug() << "setINDITarget: establishINDI() failed.";
        return;
    }

    dev = indimenu->findDevice(indimenu->getCurrentDevice());
    if (!dev)
        dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
    if (!dev)
    {
        kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!";
        return;
    }

    prop = dev->findProp("EQUATORIAL_EOD_COORD");
    if (!prop) return;

    el   = prop->findElement("RA");
    if (!el) return;
    if (!el->write_w) return;

    el->write_w->setText(QString::number(RA));

    el  = prop->findElement("DEC");
    if (!el) return;
    if (!el->write_w) return;

    el->write_w->setText(QString::number(DEC));

    prop->newText();

}


void KStars::setINDITargetName(const QString &objectName)
{
    INDI_D *dev;
    INDI_P *prop;
    INDI_E *el;

    if (!indidriver || !indimenu) {
        kDebug() << "setINDITarget: establishINDI() failed.";
        return;
    }

    SkyObject *target = data()->objectNamed( objectName );
    if (!target) return;

    dev = indimenu->findDevice(indimenu->getCurrentDevice());
    if (!dev)
        dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
    if (!dev) {
        kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!";
        return;
    }

    prop = dev->findProp("EQUATORIAL_EOD_COORD");
    if (!prop) return;

    el = prop->findElement("RA");
    if( !el || !el->write_w)
        return;
    el->write_w->setText(QString::number(target->ra().Hours()));

    el  = prop->findElement("DEC");
    if( !el || !el->write_w)
        return;
    el->write_w->setText(QString::number(target->dec().Degrees()));

    prop->newText();
}


void KStars::setINDIAction(const QString &action)
{
    INDI_D *dev;
    INDI_E *el;

    if (!indidriver || !indimenu)
    {
        kDebug() << "setINDIAction: establishINDI() failed.";
        return;
    }

    dev = indimenu->findDevice(indimenu->getCurrentDevice());
    if (!dev)
        dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
    if (!dev) {
        kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!";
        return;
    }

    el = dev->findElem(action);
    if( !el )
        return;

    el->pp->activateSwitch(action);

}

//FIXME: DBUS: needs to be reimplemented without suspend/resume
void KStars::waitForINDIAction(const QString &action)
{
    //
    //  INDI_D *dev;
    //  INDI_P *prop;
    //  INDI_E *el;
    //
    //  if (!indidriver || !indimenu)
    //  {
    //    kDebug() << "waitForINDIAction: establishINDI() failed.";
    //    return;
    //  }
    //
    //  dev = indimenu->findDevice(indimenu->getCurrentDevice());
    //  if (!dev)
    //    dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
    //  if (!dev)
    //  {
    //    kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!";
    //    return;
    //  }
    //
    //  prop = dev->findProp(action);
    //
    //  if (prop == NULL)
    //  {
    //    el = dev->findElem(action);
    //    if (!el) return;
    //
    //    QObject::connect(el->pp, SIGNAL(okState()), this, SLOT(resumeDCOP(void )));
    //  }
    //  else
    //    QObject::connect(prop, SIGNAL(okState()), this, SLOT(resumeDCOP(void )));
    //
    //  kapp->dcopClient()->suspend();
    //
}


void KStars::setINDIFocusSpeed(unsigned int speed)
{
    INDI_D *dev;
    INDI_P *prop;
    INDI_E *el;

    if (!indidriver || !indimenu)
    {
        kDebug() << "setINDIFocusSpeed: establishINDI() failed.";
        return;
    }

    dev = indimenu->findDevice(indimenu->getCurrentDevice());
    if (!dev)
        dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
    if (!dev)
    {
        kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!";
        return;
    }

    prop = dev->findProp("FOCUS_SPEED");
    if (!prop) return;

    el   = prop->findElement("SPEED");
    if (!el) return;
    if (!el->write_w) return;

    el->write_w->setText(QString::number(speed));

    prop->newText();

}


void KStars::startINDIFocus(int focusDir)
{
    if (!indidriver || !indimenu)
    {
        kDebug() << "setINDIFocus: establishINDI() failed!";
        return;
    }

    if (focusDir == 0)
        setINDIAction("IN");
    else if (focusDir == 1)
        setINDIAction("OUT");

}


void KStars::setINDIGeoLocation(double longitude, double latitude)
{
    INDI_D *dev;
    INDI_P *prop;
    INDI_E *el;

    if (!indidriver || !indimenu)
    {
        kDebug() << "setINDIGeoLocation: establishINDI() failed.";
        return;
    }

    dev = indimenu->findDevice(indimenu->getCurrentDevice());
    if (!dev)
        dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
    if (!dev)
    {
        kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!";
        return;
    }

    prop = dev->findProp("GEOGRAPHICAL_COORD");
    if (!prop) return;

    el   = prop->findElement("LONG");
    if (!el) return;
    if (!el->write_w) return;

    el->write_w->setText(QString::number(longitude));

    el  = prop->findElement("LAT");
    if (!el) return;
    if (!el->write_w) return;

    el->write_w->setText(QString::number(latitude));

    prop->newText();

}


void KStars::setINDIFocusTimeout(int timeout)
{
    INDI_D *dev;
    INDI_P *prop;
    INDI_E *el;

    if (!indidriver || !indimenu)
    {
        kDebug() << "startINDIFocus: establishINDI() failed.";
        return;
    }

    dev = indimenu->findDevice(indimenu->getCurrentDevice());
    if (!dev)
        dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
    if (!dev)
    {
        kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!";
        return;
    }

    prop = dev->findProp("FOCUS_TIMEOUT");
    if (!prop)
        return;

    el   = prop->findElement("TIMEOUT");
    if (!el) return;

    if (el->write_w)
        el->write_w->setText(QString::number(timeout));
    else if (el->spin_w)
        el->spin_w->setValue(timeout);

    prop->newText();

}


void KStars::startINDIExposure(int timeout)
{
    INDI_D *dev;
    INDI_P *prop;
    INDI_E *el;

    if (!indidriver || !indimenu)
    {
        kDebug() << "startINDIExposure: establishINDI() failed.";
        return;
    }

    dev = indimenu->findDevice(indimenu->getCurrentDevice());
    if (!dev)
        dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
    if (!dev)
    {
        kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!";
        return;
    }

    prop = dev->findProp("CCD_EXPOSE_DURATION");
    if (!prop) return;

    el   = prop->findElement("EXPOSE_DURATION");
    if (!el) return;

    if (el->write_w)
        el->write_w->setText(QString::number(timeout));
    else if (el->spin_w)
        el->spin_w->setValue(timeout);


    prop->newText();

}

void KStars::setINDIFilterNum(int filter_num)
{
    INDI_D *dev;
    INDI_P *prop;
    INDI_E *el;

    if (!indidriver || !indimenu)
    {
        kDebug() << "setINDIFilterNum: establishINDI() failed.";
        return;
    }

    dev = indimenu->findDevice(indimenu->getCurrentDevice());
    if (!dev)
        dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
    if (!dev)
    {
        kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!";
        return;
    }

    prop = dev->findProp("FILTER_SLOT");
    if (!prop) return;

    el   = prop->findElement("SLOT");
    if (!el) return;

    if (el->write_w)
        el->write_w->setText(QString::number(filter_num));
    else if (el->spin_w)
        el->spin_w->setValue(filter_num);

    prop->newText();

}

void KStars::setINDIUTC(const QString &UTCDateTime)
{
    INDI_D *dev;
    INDI_P *prop;
    INDI_E *el;

    if (!indidriver || !indimenu)
    {
        kDebug() << "startINDIUTC: establishINDI() failed.";
        return;
    }

    dev = indimenu->findDevice(indimenu->getCurrentDevice());
    if (!dev)
        dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
    if (!dev)
    {
        kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!";
        return;
    }

    prop = dev->findProp("TIME_UTC");
    if (!prop) return;

    el   = prop->findElement("UTC");
    if (!el) return;
    if (!el->write_w) return;

    el->write_w->setText(UTCDateTime);

    prop->newText();

}

void KStars::setINDIScopeAction(const QString &action)
{
    setINDIAction(action);
}

void KStars::setINDIFrameType(const QString &type)
{
    setINDIAction(type);
}

void KStars::setINDICCDTemp(int temp)
{
    INDI_D *dev;
    INDI_P *prop;
    INDI_E *el;

    if (!indidriver || !indimenu)
    {
        kDebug() << "setINDICCDTemp: establishINDI() failed.";
        return;
    }

    dev = indimenu->findDevice(indimenu->getCurrentDevice());
    if (!dev)
        dev = indimenu->findDeviceByLabel(indimenu->getCurrentDevice());
    if (!dev)
    {
        kDebug() << "Device " << indimenu->getCurrentDevice() << " not found!";
        return;
    }

    prop = dev->findProp("CCD_TEMPERATURE");
    if (!prop) return;

    el   = prop->findElement("TEMPERATURE");
    if (!el) return;

    if (el->write_w)
        el->write_w->setText(QString::number(temp));
    else if (el->spin_w)
        el->spin_w->setValue(temp);


    prop->newText();

}
#endif

