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
#include "starobject.h"
#include "ksmoon.h"
#include "skyobject.h"
#include "trailobject.h"
#include "skymap.h"

#include "indimenu.h"
#include "devicemanager.h"
#include "indidevice.h"
#include "indigroup.h"
#include "indiproperty.h"
#include "skycomponents/constellationboundary.h"

#include <kactioncollection.h>

KSPopupMenu::KSPopupMenu( KStars *_ks )
        : KMenu( _ks ), ks(_ks)
{}

KSPopupMenu::~KSPopupMenu()
{
    //DEBUG
    //kDebug() << "aName: " << aName;
    //if ( aName )            delete aName;
    //if ( aName2 )           delete aName2;
    //if ( aType )            delete aType;
    //if ( aConstellation )   delete aConstellation;
    //if ( aRiseTime )        delete aRiseTime;
    //if ( aSetTime )         delete aSetTime;
    //if ( aTransitTime )     delete aTransitTime;
    //DEBUG
    //kDebug() << "labName: " << labName;
    //if ( labName )          labName->deleteLater();
    //if ( labName2 )         labName2->deleteLater();
    //if ( labType )          labType->deleteLater();
    //if ( labConstellation ) labConstellation->deleteLater();
    //if ( labRiseTime )      labRiseTime->deleteLater();
    //if ( labSetTime )       labSetTime->deleteLater();
    //if ( labTransitTime )   labTransitTime->deleteLater();
    ////DEBUG
    //kDebug() << "menuDevice: " << menuDevice;
    //if ( menuDevice )       delete menuDevice;
}

void KSPopupMenu::createEmptyMenu( SkyObject *nullObj ) {
    initPopupMenu( nullObj, i18n( "Empty sky" ), QString(), QString(), true, true, false, false, false, true, false );

    addAction( i18nc( "Sloan Digital Sky Survey", "Show SDSS Image" ), ks->map(), SLOT( slotSDSS() ) );
    addAction( i18nc( "Digitized Sky Survey", "Show DSS Image" ), ks->map(), SLOT( slotDSS() ) );
}

void KSPopupMenu::createStarMenu( StarObject *star ) {
    //Add name, rise/set time, center/track, and detail-window items
    initPopupMenu( star, star->translatedLongName(), i18n( "Spectral type: %1" , star->sptype()),
                   i18n( "star" ) );

    //If the star is named, add custom items to popup menu based on object's ImageList and InfoList
    if ( star->name() != "star" ) {
        addLinksToMenu( star );
    } else {
        addAction( i18nc( "Sloan Digital Sky Survey", "Show SDSS Image" ), ks->map(), SLOT( slotSDSS() ) );
        addAction( i18nc( "Digitized Sky Survey", "Show DSS Image" ), ks->map(), SLOT( slotDSS() ) );
    }
}

void KSPopupMenu::createDeepSkyObjectMenu( SkyObject *obj ) {
    QString TypeName = ks->data()->typeName( obj->type() );
    QString secondName = obj->translatedName2();
    if ( obj->longname() != obj->name() ) secondName = obj->translatedLongName();

    initPopupMenu( obj, obj->translatedName(), secondName, TypeName );
    addLinksToMenu( obj );
}

void KSPopupMenu::createCustomObjectMenu( SkyObject *obj ) {
    QString TypeName = ks->data()->typeName( obj->type() );
    QString secondName = obj->translatedName2();
    if ( obj->longname() != obj->name() ) secondName = obj->translatedLongName();

    initPopupMenu( obj, obj->translatedName(), secondName, TypeName );

    addLinksToMenu( obj, true );
}

void KSPopupMenu::createPlanetMenu( SkyObject *p ) {
    bool addTrail( ! ((TrailObject*)p)->hasTrail() );
    QString oname;
    if ( p->name() == "Moon" ) {
        oname = ((KSMoon *)p)->phaseName();
    }
    initPopupMenu( p, p->translatedName(), oname, i18n("Solar System"), true, true, true, true, addTrail );
    addLinksToMenu( p, false ); //don't offer DSS images for planets
}

void KSPopupMenu::initPopupMenu( SkyObject *obj, const QString &_s1, const QString &s2, const QString &s3,
                                 bool showRiseSet, bool showCenterTrack, bool showDetails, bool showTrail, bool addTrail,
                                 bool showAngularDistance, bool showObsList ) {

    clear();
    QString s1 = _s1;
    if ( s1.isEmpty() ) s1 = i18n( "Empty sky" );

    bool showLabel( true );
    if ( s1 == i18n( "star" ) || s1 == i18n( "Empty sky" ) ) showLabel = false;

    labName = new QLabel( "<b>"+s1+"</b>", this );
    labName->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
    aName = new KAction( this );
    ks->actionCollection()->addAction( "title_name1", aName );
    aName->setDefaultWidget( labName );
    addAction( aName );

    if ( ! s2.isEmpty() ) {
        labName2 = new QLabel( "<b>"+s2+"</b>", this );
        labName2->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        aName2 = new KAction( this );
        ks->actionCollection()->addAction( "title_name2", aName2 );
        aName2->setDefaultWidget( labName2 );
        addAction( aName2 );
    }

    if ( ! s3.isEmpty() ) {
        labType = new QLabel( "<b>"+s3+"</b>", this );
        labType->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        aType = new KAction( this );
        ks->actionCollection()->addAction( "title_type", aType );
        aType->setDefaultWidget( labType );
        addAction( aType );
    }

    labConstellation = new QLabel( "<b>"+
                                   ConstellationBoundary::Instance()->constellationName( obj )+"</b>", this );
    labConstellation->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
    aConstellation = new KAction( this );
    ks->actionCollection()->addAction( "title_constellation", aConstellation );
    aConstellation->setDefaultWidget( labConstellation );
    addAction( aConstellation );

    //Insert Rise/Set/Transit labels
    if ( showRiseSet && obj ) {
        addSeparator();

        QString sRiseTime( i18n( "Rise time: %1" , QString("00:00") ) );
        QString sSetTime( i18nc( "the time at which an object falls below the horizon", "Set time: %1" , QString("00:00") ) );
        QString sTransitTime( i18n( "Transit time: %1" , QString("00:00") ) );

        labRiseTime = new QLabel( "<b>"+sRiseTime+"</b>", this );
        labRiseTime->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        QFont smallFont = labRiseTime->font();
        smallFont.setPointSize( smallFont.pointSize() - 2 );
        labRiseTime->setFont( smallFont );
        aRiseTime = new KAction( this );
        ks->actionCollection()->addAction( "title_risetime", aRiseTime );
        aRiseTime->setDefaultWidget( labRiseTime );
        addAction( aRiseTime );

        labSetTime = new QLabel( "<b>"+sSetTime+"</b>", this );
        labSetTime->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        labSetTime->setFont( smallFont );
        aSetTime = new KAction( this );
        ks->actionCollection()->addAction( "title_settime", aSetTime );
        aSetTime->setDefaultWidget( labSetTime );
        addAction( aSetTime );

        labTransitTime = new QLabel( "<b>"+sTransitTime+"</b>", this );
        labTransitTime->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
        labTransitTime->setFont( smallFont );
        aTransitTime = new KAction( this );
        ks->actionCollection()->addAction( "title_transittime", aTransitTime );
        aTransitTime->setDefaultWidget( labTransitTime );
        addAction( aTransitTime );

        setRiseSetLabels( obj );
    }

    //Insert item for centering on object
    if ( showCenterTrack && obj ) {
        addSeparator();
        addAction( i18n( "Center && Track" ), ks->map(), SLOT( slotCenter() ) );
    }

    //Insert item for measuring distances
    //FIXME: add key shortcut to menu items properly!
    if ( showAngularDistance && obj ) {
        if (! (ks->map()->isAngleMode()) ) {
            addAction( i18n( "Angular Distance To...            [" ), ks->map(),
                       SLOT( slotBeginAngularDistance() ) );
        } else {
            addAction( i18n( "Compute Angular Distance          ]" ), ks->map(),
                       SLOT( slotEndAngularDistance() ) );
        }
    }


    //Insert item for Showing details dialog
    if ( showDetails && obj ) {
        addAction( i18nc( "Show Detailed Information Dialog", "Details" ), ks->map(),
                   SLOT( slotDetail() ) );
    }

    //Insert "Add/Remove Label" item
    if ( showLabel && obj ) {
        if ( ks->map()->isObjectLabeled( obj ) ) {
            addAction( i18n( "Remove Label" ), ks->map(), SLOT( slotRemoveObjectLabel() ) );
        } else {
            addAction( i18n( "Attach Label" ), ks->map(), SLOT( slotAddObjectLabel() ) );
        }
    }

    if ( showObsList && obj ) {
        if ( ks->observingList()->contains( obj ) )
            addAction( i18n("Remove From List"), ks->observingList(), SLOT( slotRemoveObject() ) );
        else
            addAction( i18n("Add to List"), ks->observingList(), SLOT( slotAddObject() ) );
    }

    if ( showTrail && obj && obj->isSolarSystem() ) {
        if ( addTrail ) {
            addAction( i18n( "Add Trail" ), ks->map(), SLOT( slotAddPlanetTrail() ) );
        } else {
            addAction( i18n( "Remove Trail" ), ks->map(), SLOT( slotRemovePlanetTrail() ) );
        }
    }

    addSeparator();

    if (addINDI())
        addSeparator();
}

void KSPopupMenu::addLinksToMenu( SkyObject *obj, bool showDSS ) {
    QString sURL;
    QStringList::Iterator itList, itTitle, itListEnd;

    itList  = obj->ImageList.begin();
    itTitle = obj->ImageTitle.begin();
    itListEnd = obj->ImageList.end();

    int id = 100;
    for ( ; itList != itListEnd; ++itList ) {
        QString t = QString(*itTitle);
        sURL = QString(*itList);
        addAction( i18nc( "Image/info menu item (should be translated)", t.toLocal8Bit() ), ks->map(), SLOT( slotImage() ) );
        ++itTitle;
    }

    if ( showDSS ) {
        addAction( i18nc( "Sloan Digital Sky Survey", "Show SDSS Image" ), ks->map(), SLOT( slotSDSS() ) );
        addAction( i18nc( "Digitized Sky Survey", "Show DSS Image" ), ks->map(), SLOT( slotDSS() ) );
        addSeparator();
    }
    else if ( obj->ImageList.count() ) addSeparator();

    itList  = obj->InfoList.begin();
    itTitle = obj->InfoTitle.begin();
    itListEnd = obj->InfoList.end();

    id = 200;
    for ( ; itList != itListEnd; ++itList ) {
        QString t = QString(*itTitle);
        sURL = QString(*itList);
        addAction( i18nc( "Image/info menu item (should be translated)", t.toLocal8Bit() ), ks->map(), SLOT( slotInfo() ) );
        ++itTitle;
    }
}

bool KSPopupMenu::addINDI(void)
{
    INDIMenu *indiMenu = ks->getINDIMenu();
    DeviceManager *managers;
    INDI_D *dev;
    INDI_G *grp;
    INDI_P *prop(NULL);
    INDI_E *element;

    if (indiMenu->managers.count() == 0)
        return false;

    foreach ( managers, indiMenu->managers )
    {
        foreach (dev, managers->indi_dev )
        {
            if (!dev->INDIStdSupport)
                continue;

            menuDevice = new KMenu(dev->label);
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

                        if (element->state == PS_ON)
                            a->setChecked(true);
                        else
                            a->setChecked(false);
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

    return true;
}

void KSPopupMenu::setRiseSetLabels( SkyObject *obj ) {
    if ( ! obj ) return;

    QString rt;
    QTime rtime = obj->riseSetTime( ks->data()->ut(), ks->geo(), true );
    dms rAz = obj->riseSetTimeAz( ks->data()->ut(), ks->geo(), true );

    if ( rtime.isValid() ) {
        //We can round to the nearest minute by simply adding 30 seconds to the time.
        rt = i18n( "Rise time: %1", KGlobal::locale()->formatTime( rtime.addSecs(30) ) );

    } else if ( obj->alt()->Degrees() > 0 ) {
        rt = i18n( "No rise time: Circumpolar" );
    } else {
        rt = i18n( "No rise time: Never rises" );
    }

    KStarsDateTime dt = ks->data()->ut();
    QTime stime = obj->riseSetTime( dt, ks->geo(), false );

    QString st;
    dms sAz = obj->riseSetTimeAz( dt,  ks->geo(), false );

    if ( stime.isValid() ) {
        //We can round to the nearest minute by simply adding 30 seconds to the time.
        st = i18nc( "the time at which an object falls below the horizon", "Set time: %1", KGlobal::locale()->formatTime( stime.addSecs(30) ) );

    } else if ( obj->alt()->Degrees() > 0 ) {
        st = i18n( "No set time: Circumpolar" );
    } else {
        st = i18n( "No set time: Never rises" );
    }

    QTime ttime = obj->transitTime( dt, ks->geo() );
    dms trAlt = obj->transitAltitude( dt, ks->geo() );
    QString tt;

    if ( ttime.isValid() ) {
        //We can round to the nearest minute by simply adding 30 seconds to the time.
        tt = i18n( "Transit time: %1", KGlobal::locale()->formatTime( ttime.addSecs(30) ) );
    } else {
        tt = "--:--";
    }

    labRiseTime->setText( "<b>"+rt+"</b>" );
    labSetTime->setText( "<b>"+st+"</b>" );
    labTransitTime->setText( "<b>"+tt+"</b>" ) ;
}

#include "kspopupmenu.moc"
