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

#include <QSignalMapper>
#include <QWidgetAction>

#include <KLocalizedString>

#include "kstars.h"
#include "kstarsdata.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/starobject.h"
#include "skyobjects/trailobject.h"
#include "skyobjects/deepskyobject.h"
#include "skyobjects/ksmoon.h"
#include "skyobjects/satellite.h"
#include "skyobjects/supernova.h"
#include "skycomponents/skymapcomposite.h"
#include "skycomponents/flagcomponent.h"
#include "skymap.h"

#include "observinglist.h"

#include <config-kstars.h>

#ifdef HAVE_INDI
#include "indi/indilistener.h"
#include "indi/guimanager.h"
#include "indi/driverinfo.h"
#include "indi/indistd.h"
#include "indi/indidevice.h"
#include "indi/indigroup.h"
#include "indi/indiproperty.h"
#include "indi/indielement.h"
#include <basedevice.h>
#endif

#include "skycomponents/constellationboundarylines.h"


namespace {
    // Convert magnitude to string representation for QLabel
    QString magToStr(double m) {
        if (m > -30 && m < 90)
            return QString("%1<sup>m</sup>").arg(m, 0, 'f', 2);
        else
            return QString();
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
            QString time = QLocale().toString( t.addSecs(30), QLocale::ShortFormat );
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
            return i18n( "Transit time: %1", QLocale().toString( t.addSecs(30), QLocale::ShortFormat ) );
        else
            return "--:--";
    }

}


KSPopupMenu::KSPopupMenu()
    : QMenu( KStars::Instance() ), m_CurrentFlagIdx(-1), m_EditActionMapping(0), m_DeleteActionMapping(0)
{}

KSPopupMenu::~KSPopupMenu()
{
    if(m_EditActionMapping) {
        delete m_EditActionMapping;
    }

    if(m_DeleteActionMapping) {
        delete m_DeleteActionMapping;
    }
}

void KSPopupMenu::createEmptyMenu( SkyPoint *nullObj ) {
    KStars* ks = KStars::Instance();
    SkyObject o( SkyObject::TYPE_UNKNOWN, nullObj->ra(), nullObj->dec() );
    o.setAlt( nullObj->alt() );
    o.setAz( nullObj->az() );
    initPopupMenu( &o, i18n( "Empty sky" ), QString(), QString(), false, false );
    addAction( i18nc( "Sloan Digital Sky Survey", "Show SDSS Image" ), ks->map(), SLOT( slotSDSS() ) );
    addAction( i18nc( "Digitized Sky Survey", "Show DSS Image" ),      ks->map(), SLOT( slotDSS()  ) );
}

void KSPopupMenu::slotEditFlag() {
    if ( m_CurrentFlagIdx != -1 ) {
        KStars::Instance()->map()->slotEditFlag( m_CurrentFlagIdx );
    }
}

void KSPopupMenu::slotDeleteFlag() {
    if ( m_CurrentFlagIdx != -1 ) {
        KStars::Instance()->map()->slotDeleteFlag( m_CurrentFlagIdx );
    }
}

void KSPopupMenu::slotEditFlag( QAction *action ) {
    int idx = m_EditActionMapping->value( action, -1 );

    if ( idx == -1 ) {
        return;
    }

    else {
        KStars::Instance()->map()->slotEditFlag( idx );
    }
}

void KSPopupMenu::slotDeleteFlag( QAction *action ) {
    int idx = m_DeleteActionMapping->value( action, -1 );

    if ( idx == -1 ) {
        return;
    }

    else {
        KStars::Instance()->map()->slotDeleteFlag( idx );
    }
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
    QString name     = getObjectName(obj);
    QString typeName = obj->typeName();
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

void KSPopupMenu::createSatelliteMenu( Satellite *satellite ) {
    KStars* ks = KStars::Instance();
    QString velocity, altitude, range;
    velocity.setNum( satellite->velocity() );
    altitude.setNum( satellite->altitude() );
    range.setNum( satellite->range() );

    clear();

    addFancyLabel( satellite->name() );
    addFancyLabel( satellite->id() );
    addFancyLabel( i18n( "satellite" ) );
    addFancyLabel( KStarsData::Instance()->skyComposite()->constellationBoundary()->constellationName( satellite ) );

    addSeparator();

    addFancyLabel( i18n( "Velocity: %1 km/s", velocity ), -2 );
    addFancyLabel( i18n( "Altitude: %1 km", altitude ), -2 );
    addFancyLabel( i18n( "Range: %1 km", range ), -2 );

    addSeparator();

    //Insert item for centering on object
    addAction( i18n( "Center && Track" ), ks->map(), SLOT( slotCenter() ) );
    //Insert item for measuring distances
    //FIXME: add key shortcut to menu items properly!
    addAction( i18n( "Angular Distance To...            [" ), ks->map(),
               SLOT(slotBeginAngularDistance()) );
    addAction( i18n( "Starhop from here to...            " ), ks->map(),
               SLOT(slotBeginStarHop()) );

    //Insert "Add/Remove Label" item
    if ( ks->map()->isObjectLabeled( satellite ) )
        addAction( i18n( "Remove Label" ), ks->map(), SLOT( slotRemoveObjectLabel() ) );
    else
        addAction( i18n( "Attach Label" ), ks->map(), SLOT( slotAddObjectLabel() ) );
}

void KSPopupMenu::createSupernovaMenu(Supernova* supernova)
{
    QString name=supernova->name();
    float mag = supernova->mag();
    QString type = supernova->getType();
    initPopupMenu( supernova, name, i18n( "supernova" ), i18n("%1<sup>m</sup>, %2", mag, type) );
}

void KSPopupMenu::initPopupMenu( SkyObject *obj, QString name, QString type, QString info,
                                 bool showDetails, bool showObsList, bool showFlags )
{
    KStarsData* data = KStarsData::Instance();
    SkyMap * map = SkyMap::Instance();

    clear();
    bool showLabel = name != i18n("star") && !name.isEmpty();
    if( name.isEmpty() )
        name = i18n( "Empty sky" );

    addFancyLabel( name );
    addFancyLabel( type );
    addFancyLabel( info );
    addFancyLabel( KStarsData::Instance()->skyComposite()->constellationBoundary()->constellationName( obj ) );

    //Insert Rise/Set/Transit labels
    SkyObject* o = obj->clone();
    addSeparator();
    addFancyLabel( riseSetTimeLabel(o, true),  -2 );
    addFancyLabel( riseSetTimeLabel(o, false), -2 );
    addFancyLabel( transitTimeLabel(o),        -2 );
    addSeparator();
    delete o;

    // Show 'Select this object' item when in object pointing mode and when obj is not empty sky
    if(map->isInObjectPointingMode() && obj->type() != 21)
    {
        addAction( i18n( "Select this object"), map, SLOT(slotObjectSelected()));
    }

    //Insert item for centering on object
    addAction( i18n( "Center && Track" ), map, SLOT( slotCenter() ) );

    if ( showFlags )
    {
        //Insert actions for flag operations
        initFlagActions( obj );
    }

    //Insert item for measuring distances
    //FIXME: add key shortcut to menu items properly!
    addAction( i18n( "Angular Distance To...            [" ), map, SLOT(slotBeginAngularDistance()) );
    addAction( i18n( "Starhop from here to...            " ), map, SLOT(slotBeginStarHop()) );

    //Insert item for Showing details dialog
    if ( showDetails )
        addAction( i18nc( "Show Detailed Information Dialog", "Details" ), map, SLOT( slotDetail() ) );
    //Insert "Add/Remove Label" item
    if ( showLabel )
    {
        if ( map->isObjectLabeled( obj ) ) {
            addAction( i18n( "Remove Label" ), map, SLOT( slotRemoveObjectLabel() ) );
        } else {
            addAction( i18n( "Attach Label" ), map, SLOT( slotAddObjectLabel() ) );
        }
    }
    // Should show observing list
    if( showObsList ) {
        if ( data->observingList()->contains( obj ) )
            addAction( i18n("Remove From Observing WishList"), data->observingList(), SLOT( slotRemoveObject() ) );
        else
            addAction( i18n("Add to Observing WishList"), data->observingList(), SLOT( slotAddObject() ) );
    }
    // Should we show trail actions
    TrailObject* t = dynamic_cast<TrailObject*>( obj );
    if( t ) {
        if( t->hasTrail() )
            addAction( i18n( "Remove Trail" ), map, SLOT( slotRemovePlanetTrail() ) );
        else
            addAction( i18n( "Add Trail" ), map, SLOT( slotAddPlanetTrail() ) );
    }

    addAction( i18n("Simulate eyepiece view"), map, SLOT( slotEyepieceView() ) );

    addSeparator();
#ifdef HAVE_XPLANET
    if ( obj->isSolarSystem() && obj->type() != SkyObject::COMET ) { // FIXME: We now have asteroids -- so should this not be isMajorPlanet() || Pluto?
        //QMenu *xplanetSubmenu = new QMenu();
        //xplanetSubmenu->setTitle( i18n( "Print Xplanet view" ) );
        addAction( i18n( "View in XPlanet" ), map, SLOT( slotXplanetToWindow() ) );
        //xplanetSubmenu->addAction( i18n( "To file..." ), map, SLOT( slotXplanetToFile() ) );
        //addMenu( xplanetSubmenu );
    }
#endif
    addSeparator();
    addINDI();
}

void KSPopupMenu::initFlagActions( SkyObject *obj ) {
    KStars *ks = KStars::Instance();

    QList<int> flags = ks->data()->skyComposite()->flags()->getFlagsNearPix( obj, 5 );

    if ( flags.size() == 0 ) {
        // There is no flag around clicked SkyObject
        addAction( i18n( "Add flag..."), ks->map(), SLOT( slotAddFlag() ) );
    }

    else if( flags.size() == 1) {
        // There is only one flag around clicked SkyObject
        addAction ( i18n ("Edit flag"), this, SLOT( slotEditFlag() ) );
        addAction ( i18n ("Delete flag"), this, SLOT( slotDeleteFlag() ) );

        m_CurrentFlagIdx = flags.first();
    }

    else {
        // There are more than one flags around clicked SkyObject - we need to create submenus
        QMenu *editMenu = new QMenu ( i18n ( "Edit flag..."), KStars::Instance() );
        QMenu *deleteMenu = new QMenu ( i18n ( "Delete flag..."), KStars::Instance() );

        connect( editMenu, SIGNAL( triggered(QAction*) ), this, SLOT( slotEditFlag(QAction*) ) );
        connect( deleteMenu, SIGNAL( triggered(QAction*) ), this, SLOT( slotDeleteFlag(QAction*) ) );

        if ( m_EditActionMapping ) {
            delete m_EditActionMapping;
        }

        if ( m_DeleteActionMapping ) {
            delete m_DeleteActionMapping;
        }

        m_EditActionMapping = new QHash<QAction*, int>;
        m_DeleteActionMapping = new QHash<QAction*, int>;

        foreach ( int idx, flags ) {
            QIcon flagIcon( QPixmap::fromImage( ks->data()->skyComposite()->flags()->image( idx ) ) );

            QString flagLabel = ks->data()->skyComposite()->flags()->label( idx );
            if ( flagLabel.size() > 35 ) {
                flagLabel = flagLabel.left( 35 );
                flagLabel.append( "..." );
            }

            QAction *editAction = new QAction( flagIcon, flagLabel, ks );
            editAction->setIconVisibleInMenu( true );
            editMenu->addAction( editAction );
            m_EditActionMapping->insert( editAction, idx );

            QAction *deleteAction = new QAction( flagIcon, flagLabel, ks );
            deleteAction->setIconVisibleInMenu( true );
            deleteMenu->addAction( deleteAction );
            m_DeleteActionMapping->insert( deleteAction, idx);
        }

        addMenu( editMenu );
        addMenu( deleteMenu );
    }
}

void KSPopupMenu::addLinksToMenu( SkyObject *obj, bool showDSS ) {
    KStars* ks = KStars::Instance();
    QString sURL;
    QStringList::ConstIterator itList, itTitle, itListEnd;

    itList  = obj->ImageList().constBegin();
    itTitle = obj->ImageTitle().constBegin();
    itListEnd = obj->ImageList().constEnd();
    if( ! obj->ImageList().isEmpty() ) {
        QMenu *imageLinkSubMenu= new QMenu();
        imageLinkSubMenu->setTitle( i18n( "Image Resources" ) );
        for ( ; itList != itListEnd; ++itList ) {
            QString t = QString(*itTitle);
            sURL = QString(*itList);
            imageLinkSubMenu->addAction( i18nc( "Image/info menu item (should be translated)", t.toLocal8Bit() ), ks->map(), SLOT( slotImage() ) );
            ++itTitle;
        }
        addMenu( imageLinkSubMenu );
    }

    if ( showDSS ) {
        addAction( i18nc( "Sloan Digital Sky Survey", "Show SDSS Image" ), ks->map(), SLOT( slotSDSS() ) );
        addAction( i18nc( "Digitized Sky Survey", "Show DSS Image" ), ks->map(), SLOT( slotDSS() ) );
    }

    if( showDSS )
        addSeparator();

    itList  = obj->InfoList().constBegin();
    itTitle = obj->InfoTitle().constBegin();
    itListEnd = obj->InfoList().constEnd();

    if( ! obj->InfoList().isEmpty() ) {
        QMenu *infoLinkSubMenu= new QMenu();
        infoLinkSubMenu->setTitle( i18n( "Information Resources" ) );
        for ( ; itList != itListEnd; ++itList ) {
            QString t = QString(*itTitle);
            sURL = QString(*itList);
            infoLinkSubMenu->addAction( i18nc( "Image/info menu item (should be translated)", t.toLocal8Bit() ), ks->map(), SLOT( slotInfo() ) );
            ++itTitle;
        }
        addMenu( infoLinkSubMenu );
    }
}

void KSPopupMenu::addINDI()
{
#ifdef HAVE_INDI

    if (INDIListener::Instance()->size() == 0)
        return;

    foreach(ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *bd = gd->getBaseDevice();

        if (bd == NULL)
            continue;

        //if (bd->isConnected() == false)
        //    continue;

        QMenu* menuDevice = NULL;
        ISD::GDInterface *telescope = NULL;

       foreach(INDI::Property *pp, gd->getProperties())
       {
                if (pp->getType() != INDI_SWITCH || INDIListener::Instance()->isStandardProperty(pp->getName()) == false)
                    continue;

                QSignalMapper *sMapper = new QSignalMapper(this);

                ISwitchVectorProperty *svp = pp->getSwitch();

                for (int i=0; i < svp->nsp; i++)
                {
                    if (menuDevice == NULL)
                    {
                        menuDevice = new QMenu(gd->getDeviceName());
                        addMenu(menuDevice);
                    }

                    QAction *a = menuDevice->addAction(svp->sp[i].label);

                    ISD::GDSetCommand *cmd = new ISD::GDSetCommand(INDI_SWITCH, pp->getName(), svp->sp[i].name, ISS_ON, this);

                    sMapper->setMapping(a, cmd);

                    connect(a, SIGNAL(triggered()), sMapper, SLOT(map()));

                    if (!strcmp(svp->sp[i].name, "SLEW") || !strcmp(svp->sp[i].name, "SYNC") || !strcmp(svp->sp[i].name, "TRACK"))
                    {
                        telescope = INDIListener::Instance()->getDevice(gd->getDeviceName());

                        if (telescope)
                        {

                            QSignalMapper *scopeMapper = new QSignalMapper(this);
                            scopeMapper->setMapping(a, INDI_SEND_COORDS);

                            connect(a, SIGNAL(triggered()), scopeMapper, SLOT(map()));

                            connect(scopeMapper, SIGNAL(mapped(int)), telescope, SLOT(runCommand(int)));
                        }

                    }

                }

                connect(sMapper, SIGNAL(mapped(QObject*)), gd, SLOT(setProperty(QObject*)));

                menuDevice->addSeparator();

        }


        if (telescope && menuDevice)
        {
            menuDevice->addSeparator();

            QAction *a = menuDevice->addAction(i18n("Center Crosshair"));

            QSignalMapper *scopeMapper = new QSignalMapper(this);
            scopeMapper->setMapping(a, INDI_ENGAGE_TRACKING);
            connect(a, SIGNAL(triggered()), scopeMapper, SLOT(map()));
            connect(scopeMapper, SIGNAL(mapped(int)), telescope, SLOT(runCommand(int)));
        }
    }

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
    QWidgetAction * act = new QWidgetAction( this );
    act->setDefaultWidget(label);
    addAction( act );
}
