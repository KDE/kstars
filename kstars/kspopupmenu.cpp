/***************************************************************************
                          kspopupmenu.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Feb 27 2003
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

#include "kspopupmenu.h"

#include <KGlobal>
#include <KLocale>

#include "kstars.h"
#include "kstarsdata.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/starobject.h"
#include "skyobjects/trailobject.h"
#include "skyobjects/deepskyobject.h"
#include "skyobjects/ksmoon.h"
#include "skycomponents/skymapcomposite.h"
#include "skymap.h"

#include <config-kstars.h>

#ifdef HAVE_INDI_H
#include "indi/indimenu.h"
#include "indi/devicemanager.h"
#include "indi/indidevice.h"
#include "indi/indigroup.h"
#include "indi/indiproperty.h"
#endif

#include "skycomponents/constellationboundarylines.h"


namespace {
    // Convert magnitude to string representation for QLabel
    QString magToStr(double m) {
        return QString("%1<sup>m</sup>").arg(m, 0, 'f', 2);
    }

    // Return object name
    QString getObjectName(SkyObject *obj) {
        // FIXME: make logic less convoluted.
        if( obj->longname() != obj->name() ) { // Object has proper name
            return obj->translatedLongName() + ", " + obj->translatedName();
        } else {
            if( !obj->translatedName2().isEmpty() )
                return obj->translatedName() + ", " + obj->translatedName2();
            else
                return obj->translatedName();
        }
    }

    // String representation for rise/set time of object. If object
    // doesn't rise/set returns descriptive string.
    //
    // Second parameter choose between raise and set. 'true' for
    // raise, 'false' for set.
    QString riseSetTimeLabel(SkyObject* o, bool isRaise) {
        KStarsData* data = KStarsData::Instance();
        QTime t = o->riseSetTime( data->ut(), data->geo(), isRaise );
        if ( t.isValid() ) {
            //We can round to the nearest minute by simply adding 30 seconds to the time.
            QString time = KGlobal::locale()->formatTime( t.addSecs(30) );
            return isRaise ?
                i18n ("Rise time: %1", time) :
                i18nc("the time at which an object falls below the horizon", "Set time: %1" , time);
        }
        if( o->alt().Degrees() > 0 )
            return isRaise ? i18n( "No rise time: Circumpolar" ) : i18n( "No set time: Circumpolar" );
        else
            return isRaise ? i18n( "No rise time: Never rises" ) : i18n( "No set time: Never rises" );
    }

    // String representation for transit time for object
    QString transitTimeLabel(SkyObject* o) {
        KStarsData* data = KStarsData::Instance();
        QTime t = o->transitTime( data->ut(), data->geo() );
        if ( t.isValid() )
            //We can round to the nearest minute by simply adding 30 seconds to the time.
            return i18n( "Transit time: %1", KGlobal::locale()->formatTime( t.addSecs(30) ) );
        else
            return "--:--";
    }

}


KSPopupMenu::KSPopupMenu()
    : KMenu( KStars::Instance() )
{}

KSPopupMenu::~KSPopupMenu()
{}

void KSPopupMenu::createEmptyMenu( SkyPoint *nullObj ) {
    KStars* ks = KStars::Instance();
    SkyObject o( SkyObject::TYPE_UNKNOWN, nullObj->ra(), nullObj->dec() );
    initPopupMenu( &o, i18n( "Empty sky" ), QString(), QString(), false, false );
    addAction( i18nc( "Sloan Digital Sky Survey", "Show SDSS Image" ), ks->map(), SLOT( slotSDSS() ) );
    addAction( i18nc( "Digitized Sky Survey", "Show DSS Image" ),      ks->map(), SLOT( slotDSS()  ) );
}

void KSPopupMenu::createStarMenu( StarObject *star ) {
    KStars* ks = KStars::Instance();
    //Add name, rise/set time, center/track, and detail-window items
	QString name;
	if( star->name() != "star" ) {
		name = star->translatedLongName();
	} else {
		if( star->getHDIndex() ) {
			name = QString( "HD%1" ).arg( QString::number( star->getHDIndex() ) );
		} else {
			// FIXME: this should be some catalog name too
			name = "Star";
		}
	}
    initPopupMenu( star, name, i18n( "star" ), i18n("%1<sup>m</sup>, %2", star->mag(), star->sptype()) );
    //If the star is named, add custom items to popup menu based on object's ImageList and InfoList
    if ( star->name() != "star" ) {
        addLinksToMenu( star );
    } else {
        addAction( i18nc( "Sloan Digital Sky Survey", "Show SDSS Image" ), ks->map(), SLOT( slotSDSS() ) );
        addAction( i18nc( "Digitized Sky Survey", "Show DSS Image" ), ks->map(), SLOT( slotDSS() ) );
    }
}

void KSPopupMenu::createDeepSkyObjectMenu( DeepSkyObject *obj ) {
	QString name = getObjectName(obj);
    QString typeName = KStarsData::Instance()->typeName( obj->type() );
	// FIXME: information about angular sizes should be added.
	QString info = magToStr( obj->mag() );
	initPopupMenu( obj, name, typeName, info );
    addLinksToMenu( obj );
}

void KSPopupMenu::createPlanetMenu( SkyObject *p ) {
    QString info = magToStr( p->mag() );
	QString type = i18n("Solar system object");;
    initPopupMenu( p, p->translatedName(), type, info);
    addLinksToMenu( p, false ); //don't offer DSS images for planets
}

void KSPopupMenu::createMoonMenu( KSMoon *moon ) {
    QString info = QString("%1, %2").arg( magToStr(moon->mag()), moon->phaseName() );
    initPopupMenu( moon, moon->translatedName(), QString(), info);
    addLinksToMenu( moon, false ); //don't offer DSS images for planets
}

void KSPopupMenu::initPopupMenu( SkyObject *obj, QString name, QString type, QString info,
                                 bool showDetails, bool showObsList )
{
    KStars* ks = KStars::Instance();

    clear();
    bool showLabel = name != i18n("star") && !name.isEmpty();
    if( name.isEmpty() )
        name = i18n( "Empty sky" );

    addFancyLabel( name );
    addFancyLabel( type );
    addFancyLabel( info );
    addFancyLabel( KStarsData::Instance()->skyComposite()->getConstellationBoundary()->constellationName( obj ) );

    //Insert Rise/Set/Transit labels
    SkyObject* o = obj->clone();
    addSeparator();
    addFancyLabel( riseSetTimeLabel(o, true),  -2 );
    addFancyLabel( riseSetTimeLabel(o, false), -2 );
    addFancyLabel( transitTimeLabel(o),        -2 );
    addSeparator();
    delete o;
    //Insert item for centering on object
    addAction( i18n( "Center && Track" ), ks->map(), SLOT( slotCenter() ) );
    //Insert item for measuring distances
    //FIXME: add key shortcut to menu items properly!
    addAction( i18n( "Angular Distance To...            [" ), ks->map(),
               SLOT(slotBeginAngularDistance()) );
    addAction( i18n( "Starhop from here to...            " ), ks->map(),
               SLOT(slotBeginStarHop()) ); 

    //Insert item for Showing details dialog
    if ( showDetails )
        addAction( i18nc( "Show Detailed Information Dialog", "Details" ), ks->map(), SLOT( slotDetail() ) );
    //Insert "Add/Remove Label" item
    if ( showLabel ) {
        if ( ks->map()->isObjectLabeled( obj ) ) {
            addAction( i18n( "Remove Label" ), ks->map(), SLOT( slotRemoveObjectLabel() ) );
        } else {
            addAction( i18n( "Attach Label" ), ks->map(), SLOT( slotAddObjectLabel() ) );
        }
    }
    // Should show observing list
    if( showObsList ) {
        if ( ks->observingList()->contains( obj ) )
            addAction( i18n("Remove From Observing WishList"), ks->observingList(), SLOT( slotRemoveObject() ) );
        else
            addAction( i18n("Add to Observing WishList"), ks->observingList(), SLOT( slotAddObject() ) );
    }
    // Should we show trail actions
    TrailObject* t = dynamic_cast<TrailObject*>( obj );
    if( t ) {
        if( t->hasTrail() )
            addAction( i18n( "Remove Trail" ), ks->map(), SLOT( slotRemovePlanetTrail() ) );
        else
            addAction( i18n( "Add Trail" ), ks->map(), SLOT( slotAddPlanetTrail() ) );
    }

    addSeparator();
#ifdef HAVE_XPLANET
    if ( obj->isSolarSystem() && obj->type() != SkyObject::COMET ) {
        QMenu *xplanetSubmenu = new QMenu();
        xplanetSubmenu->setTitle( i18n( "Print Xplanet view" ) );
        xplanetSubmenu->addAction( i18n( "To screen" ), ks->map(), SLOT( slotXplanetToScreen() ) );
        xplanetSubmenu->addAction( i18n( "To file..." ), ks->map(), SLOT( slotXplanetToFile() ) );
        addMenu( xplanetSubmenu );
    }
#endif
    addSeparator();
    addINDI();
}

void KSPopupMenu::addLinksToMenu( SkyObject *obj, bool showDSS ) {
    KStars* ks = KStars::Instance();
    QString sURL;
    QStringList::ConstIterator itList, itTitle, itListEnd;

    itList  = obj->ImageList().constBegin();
    itTitle = obj->ImageTitle().constBegin();
    itListEnd = obj->ImageList().constEnd();

    for ( ; itList != itListEnd; ++itList ) {
        QString t = QString(*itTitle);
        sURL = QString(*itList);
        addAction( i18nc( "Image/info menu item (should be translated)", t.toLocal8Bit() ), ks->map(), SLOT( slotImage() ) );
        ++itTitle;
    }

    if ( showDSS ) {
        addAction( i18nc( "Sloan Digital Sky Survey", "Show SDSS Image" ), ks->map(), SLOT( slotSDSS() ) );
        addAction( i18nc( "Digitized Sky Survey", "Show DSS Image" ), ks->map(), SLOT( slotDSS() ) );
    }

    if( obj->ImageList().count() || showDSS )
        addSeparator();

    itList  = obj->InfoList().constBegin();
    itTitle = obj->InfoTitle().constBegin();
    itListEnd = obj->InfoList().constEnd();

    for ( ; itList != itListEnd; ++itList ) {
        QString t = QString(*itTitle);
        sURL = QString(*itList);
        addAction( i18nc( "Image/info menu item (should be translated)", t.toLocal8Bit() ), ks->map(), SLOT( slotInfo() ) );
        ++itTitle;
    }
}

void KSPopupMenu::addINDI()
{
#ifdef HAVE_INDI_H
    INDIMenu *indiMenu = KStars::Instance()->indiMenu();
    DeviceManager *managers;
    INDI_D *dev;
    INDI_G *grp;
    INDI_P *prop(NULL);
    INDI_E *element;

    if (indiMenu->managers.count() == 0)
        return;

    foreach ( managers, indiMenu->managers )
    {
        foreach (dev, managers->indi_dev )
        {
            if (!dev->INDIStdSupport)
                continue;

            KMenu* menuDevice = new KMenu(dev->label);
            addMenu(menuDevice);

            foreach (grp, dev->gl )
            {
                foreach (prop, grp->pl )
                {
                    //Only std are allowed to show. Movement is somewhat problematic
                    //due to an issue with the LX200 telescopes (the telescope does
                    //not update RA/DEC while moving N/W/E/S) so it's better off the
                    //skymap. It's avaiable in the INDI control panel nonetheless.
                    //CCD_EXPOSURE is an INumber property, but it's so common
                    //that it's better to include in the context menu

                    if (prop->stdID == -1 || prop->stdID == TELESCOPE_MOTION_NS || prop->stdID == TELESCOPE_MOTION_WE) continue;
                    // Only switches are shown
                    if (prop->guitype != PG_BUTTONS && prop->guitype != PG_RADIO
                            && prop->stdID !=CCD_EXPOSURE) continue;

                    menuDevice->addSeparator();

                    foreach ( element, prop->el )
                    {
                        if (prop->stdID == CCD_EXPOSURE)
                        {
                            QAction *a = menuDevice->addAction(prop->label);
                            a->setCheckable( false );
                            a->setChecked( false );
                            connect(a, SIGNAL(triggered(bool)), element, SLOT(actionTriggered()));
                            continue;
                        }

                        QAction *a = menuDevice->addAction(element->label);
                        connect(a, SIGNAL(triggered(bool)), element, SLOT(actionTriggered()));

                        // We never set ON_COORD_SET to checked
                        if (prop->stdID == ON_COORD_SET)
                            continue;

                        a->setChecked( element->state == PS_ON );
                    }
                } // end property
            } // end group

            // For telescopes, add option to center the telescope position
            if ( dev->findElem("RA") || dev->findElem("ALT"))
            {
                menuDevice->addSeparator();
                QAction *a = menuDevice->addAction(i18n("Track Crosshair"));

                connect( a, SIGNAL( triggered(bool) ), dev, SLOT(engageTracking()));
            }
        } // end device
    } // end device manager
    addSeparator();
#endif
}


void KSPopupMenu::addFancyLabel(QString name, int deltaFontSize) {
    if( name.isEmpty() )
        return;
    QLabel* label = new QLabel( "<b>"+name+"</b>", this );
    label->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
    if( deltaFontSize != 0 ) {
        QFont font = label->font();
        font.setPointSize( font.pointSize() + deltaFontSize );
        label->setFont( font );
    }
    KAction* act = new KAction( this );
    act->setDefaultWidget( label );
    addAction( act );
}
#include "kspopupmenu.moc"
