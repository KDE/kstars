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

#include "kstars.h"
#include "starobject.h"
#include "skyobject.h"
#include "ksplanetbase.h"
#include "kspopupmenu.h"
#include "indidevice.h"
#include "indimenu.h"

#include <qlabel.h>

KSPopupMenu::KSPopupMenu( QWidget *parent, const char *name )
 : KPopupMenu( parent, name )
{
	ksw = ( KStars* )parent;
}

KSPopupMenu::~KSPopupMenu()
{
}

void KSPopupMenu::createEmptyMenu( void ) {
	initPopupMenu( 0, i18n( "Empty sky" ), QString::null, QString::null, false, true, false );

	insertItem( i18n( "First Generation Digitized Sky Survey", "Show 1st-Gen DSS Image" ), ksw->map(), SLOT( slotDSS() ) );
	insertItem( i18n( "Second Generation Digitized Sky Survey", "Show 2nd-Gen DSS Image" ), ksw->map(), SLOT( slotDSS2() ) );
}

void KSPopupMenu::createStarMenu( StarObject *star ) {
	//Add name, rise/set time, center/track, and detail-window items
	initPopupMenu( star, star->longname(), i18n( "Spectral type: %1" ).arg(star->sptype()),
		i18n( "star" ) );

//If the star is named, add custom items to popup menu based on object's ImageList and InfoList
	if ( star->name() != i18n("star") ) {
		addLinksToMenu( star );
	} else {
		insertItem( i18n( "First Generation Digitized Sky Survey", "Show 1st-Gen DSS Image" ), ksw->map(), SLOT( slotDSS() ) );
		insertItem( i18n( "Second Generation Digitized Sky Survey", "Show 2nd-Gen DSS Image" ), ksw->map(), SLOT( slotDSS2() ) );
	}
}

void KSPopupMenu::createDeepSkyObjectMenu( SkyObject *obj ) {
	QString TypeName = ksw->data()->TypeName[ obj->type() ];
	QString secondName = obj->name2();
	if ( obj->longname() != obj->name() ) secondName = obj->longname();

	initPopupMenu( obj, obj->translatedName(), i18n( secondName.local8Bit() ), TypeName );
	addLinksToMenu( obj );
}

void KSPopupMenu::createCustomObjectMenu( SkyObject *obj ) {
	QString TypeName = ksw->data()->TypeName[ obj->type() ];
	QString secondName = obj->name2();
	if ( obj->longname() != obj->name() ) secondName = obj->longname();

	initPopupMenu( obj, obj->translatedName(), i18n( secondName.local8Bit() ), TypeName );

	addLinksToMenu( obj, true, false ); //don't allow user to add more links (temporary)
}

void KSPopupMenu::createPlanetMenu( SkyObject *p ) {
	bool addTrail( ! ((KSPlanetBase*)p)->hasTrail() );
	initPopupMenu( p, p->translatedName(), "", i18n("Solar System"), true, true, true, true, addTrail );
	addLinksToMenu( p, false ); //don't offer DSS images for planets
}

void KSPopupMenu::addLinksToMenu( SkyObject *obj, bool showDSS, bool allowCustom ) {
	QString sURL;
	QStringList::Iterator itList;
	QStringList::Iterator itTitle;

	itList  = obj->ImageList.begin();
	itTitle = obj->ImageTitle.begin();

	int id = 100;
	for ( ; itList != obj->ImageList.end(); ++itList ) {
		QString t = QString(*itTitle);
		sURL = QString(*itList);
		insertItem( i18n( "Image/info menu item (should be translated)", t.local8Bit() ), ksw->map(), SLOT( slotImage( int ) ), 0, id++ );
		++itTitle;
	}

	if ( showDSS ) {
	  insertItem( i18n( "First Generation Digitized Sky Survey", "Show 1st-Gen DSS Image" ), ksw->map(), SLOT( slotDSS() ) );
	  insertItem( i18n( "Second Generation Digitized Sky Survey", "Show 2nd-Gen DSS Image" ), ksw->map(), SLOT( slotDSS2() ) );
	  insertSeparator();
	}
	else if ( obj->ImageList.count() ) insertSeparator();

	itList  = obj->InfoList.begin();
	itTitle = obj->InfoTitle.begin();

	id = 200;
	for ( ; itList != obj->InfoList.end(); ++itList ) {
		QString t = QString(*itTitle);
		sURL = QString(*itList);
		insertItem( i18n( "Image/info menu item (should be translated)", t.local8Bit() ), ksw->map(), SLOT( slotInfo( int ) ), 0, id++ );
		++itTitle;
	}

	if ( allowCustom ) {
		insertSeparator();
		insertItem( i18n( "Add Link..." ), ksw->map(), SLOT( addLink() ), 0, id++ );
	}
}

bool KSPopupMenu::addINDI(void)
{
  INDIMenu *indiMenu = ksw->getINDIMenu();
  INDI_P *prop;
  bool grouped = false;
  int id=0;

  if (indiMenu->mgr.size() == 0)
   return false;

  for (uint i=0; i < indiMenu->mgr.size(); i++)
  {
    for (uint j=0; j < indiMenu->mgr[i]->indi_dev.size(); j++)
    {
        if (!indiMenu->mgr[i]->indi_dev[j]->INDIStdSupport)
	 continue;

	id = 0;
	KPopupMenu *menuDevice = new KPopupMenu();
	insertItem(QString(indiMenu->mgr[i]->indi_dev[j]->label), menuDevice);

        for (uint l=0; l < indiMenu->mgr[i]->indi_dev[j]->gl.size(); l++)
	{
	   for (uint k=0; k < indiMenu->mgr[i]->indi_dev[j]->gl[l]->pl.size(); k++)
	   {
	     prop = indiMenu->mgr[i]->indi_dev[j]->gl[l]->pl[k];

	     if (!prop->isINDIStd)
	      continue;

	   if (grouped && (prop->guitype == PG_BUTTONS || prop->guitype == PG_RADIO))
	   {
	    menuDevice->insertSeparator();
	    grouped = false;
	   }

	   grouped = true;
	   prop->parentPopup = menuDevice;

	     if (prop->guitype == PG_BUTTONS)
  	     {
                for (uint g=0; g < prop->labels.size(); g++)
		{
			menuDevice->insertItem ( QString(prop->labels[g]->label), id);

	 	if ( prop->labels[g]->state == PS_ON)
		{
		        // Slew, Track, Sync are not checked
			if ( strcmp (prop->labels[g]->name, "Slew") &&
			     strcmp (prop->labels[g]->name, "Track") &&
			     strcmp (prop->labels[g]->name, "Sync"))
		  	menuDevice->setItemChecked(id, true);
			else
			menuDevice->setItemChecked(id, false);
		}
	 	else
	        	menuDevice->setItemChecked(id, false);

		//menuDevice->connectItem(id, prop, SLOT(newSwitch(g)));
		id++;

	        }

	     QObject::connect(menuDevice, SIGNAL(activated(int)), prop, SLOT (convertSwitch(int)));

	     }
	    } // end property

	  } // end group


   } // end device
 } // end device manager


 return true;

}

void KSPopupMenu::initPopupMenu( SkyObject *obj, QString s1, QString s2, QString s3,
	bool showRiseSet, bool showCenterTrack, bool showDetails, bool showTrail, bool addTrail ) {
	clear();

	if ( s1.isEmpty() ) s1 = i18n( "Empty sky" );

	bool showLabel( true );
	if ( s1 == i18n( "star" ) || s1 == i18n( "Empty sky" ) ) showLabel = false;

	pmTitle = new QLabel( s1, this );
	pmTitle->setAlignment( AlignCenter );
	QPalette pal( pmTitle->palette() );
	pal.setColor( QPalette::Normal, QColorGroup::Background, QColor( "White" ) );
	pal.setColor( QPalette::Normal, QColorGroup::Foreground, QColor( "Black" ) );
	pal.setColor( QPalette::Inactive, QColorGroup::Foreground, QColor( "Black" ) );
	pmTitle->setPalette( pal );
	insertItem( pmTitle );

	if ( ! s2.isEmpty() ) {
		pmTitle2 = new QLabel( s2, this );
		pmTitle2->setAlignment( AlignCenter );
		pmTitle2->setPalette( pal );
		insertItem( pmTitle2 );
	}

	if ( ! s3.isEmpty() ) {
		pmType = new QLabel( s3, this );
		pmType->setAlignment( AlignCenter );
		pmType->setPalette( pal );
		insertItem( pmType );
	}

	//Insert Rise/Set/Transit labels
	if ( showRiseSet ) {
		pmRiseTime = new QLabel( i18n( "Rise time: 00:00" ), this );
		pmRiseTime->setAlignment( AlignCenter );
		pmRiseTime->setPalette( pal );
		QFont rsFont = pmRiseTime->font();
		rsFont.setPointSize( rsFont.pointSize() - 2 );
		pmRiseTime->setFont( rsFont );
		pmSetTime = new QLabel( i18n( "the time at which an object falls below the horizon", "Set time:" ) + " 00:00", this );
		pmSetTime->setAlignment( AlignCenter );
		pmSetTime->setPalette( pal );
		pmSetTime->setFont( rsFont );
		pmTransitTime = new QLabel( i18n( "Transit time: 00:00" ), this );
		pmTransitTime->setAlignment( AlignCenter );
		pmTransitTime->setPalette( pal );
		pmTransitTime->setFont( rsFont );
		insertSeparator();
		insertItem( pmRiseTime );
		insertItem( pmTransitTime );
		insertItem( pmSetTime );

		setRiseSetLabels( obj );
	}

	//Insert item for centering on object
	if ( showCenterTrack ) {
		insertSeparator();
		insertItem( i18n( "Center && Track" ), ksw->map(), SLOT( slotCenter() ) );
	}

	//Insert item for Showing details dialog
	if ( showDetails ) {
		insertItem( i18n( "Show Detailed Information Dialog", "Details" ), ksw->map(), SLOT( slotDetail() ) );
	}

	//Insert "Add/Remove Label" item
	if ( showLabel ) {
		if ( ksw->map()->isObjectLabeled( obj ) ) {
			insertItem( i18n( "Remove Label" ), ksw->map(), SLOT( slotRemoveObjectLabel() ) );
		} else {
			insertItem( i18n( "Attach Label" ), ksw->map(), SLOT( slotAddObjectLabel() ) );
		}
	}

	if ( showTrail ) {
		if ( addTrail ) {
			insertItem( i18n( "Add Trail" ), ksw->map(), SLOT( slotAddPlanetTrail() ) );
		} else {
			insertItem( i18n( "Remove Trail" ), ksw->map(), SLOT( slotRemovePlanetTrail() ) );
		}
	}

	insertSeparator();

	if (addINDI())
		insertSeparator();
}

void KSPopupMenu::setRiseSetLabels( SkyObject *obj ) {
	QString rt, rt2, rt3;
	QTime rtime = obj->riseSetTime( ksw->data()->CurrentDate, ksw->geo(), true );
	dms rAz = obj->riseSetTimeAz( ksw->data()->CurrentDate, ksw->geo(), true );

	if ( rtime.isValid() ) {
		int hour = rtime.hour();
		int min = rtime.minute();
		// if min == 59 minutes and seconds between 30 and 59 -> minutes will be 60 after correction
		if ( rtime.second() >=30 ) ++min;
		// so correct minutes and hours if necessary
		if ( min == 60 ) {
			min = 0;
			hour++;
		}
		rt2.sprintf( "%02d:%02d", hour, min );
		rt3.sprintf( "%02d:%02d", rAz.degree(), rAz.arcmin() );
//		rt = i18n( "Rise time: " ) + rt2 +
//			i18n(", Azimuth: ") + rt3;
		rt = i18n( "Rise time: %1" ).arg( rt2 );

	} else if ( obj->alt()->Degrees() > 0 ) {
		rt = i18n( "No rise time: Circumpolar" );
	} else {
		rt = i18n( "No rise time: Never rises" );
	}

	QTime stime = obj->riseSetTime( ksw->data()->CurrentDate, ksw->geo(), false );
	QString st, st2, st3;
	dms sAz = obj->riseSetTimeAz(ksw->data()->CurrentDate,  ksw->geo(), false );
	if ( stime.isValid() ) {
		int hour = stime.hour();
		int min = stime.minute();
		// if min == 59 minutes and seconds between 30 and 59 -> minutes will be 60 after correction
		if ( stime.second() >=30 ) ++min;
		// so correct minutes and hours if necessary
		if ( min == 60 ) {
			min = 0;
			hour++;
		}
		st2.sprintf( "%02d:%02d", hour, min );
		st3.sprintf( "%02d:%02d", sAz.degree(), sAz.arcmin() );
		st = i18n( "the time at which an object falls below the horizon", "Set time: %1" ).arg( st2 );

	} else if ( obj->alt()->Degrees() > 0 ) {
		st = i18n( "No set time: Circumpolar" );
	} else {
		st = i18n( "No set time: Never rises" );
	}

	QTime ttime = obj->transitTime( ksw->data()->CurrentDate, ksw->geo() );
	QString tt, tt2, tt3;
	dms trAlt = obj->transitAltitude( ksw->data()->CurrentDate, ksw->geo() );

	if ( ttime.isValid() ) {
		int hour = ttime.hour();
		int min = ttime.minute();
		// if min == 59 minutes and seconds between 30 and 59 -> minutes will be 60 after correction
		if ( ttime.second() >=30 ) ++min;
		// so correct minutes and hours if necessary
		if ( min == 60 ) {
			min = 0;
			hour++;
		}
		tt2.sprintf( "%02d:%02d", hour, min );
		tt3.sprintf( "%02d:%02d", trAlt.degree(), trAlt.minute() );
//		tt = i18n( "Transit time: " ) + tt2 +
//			i18n(", Altitude: ") + tt3 ;
		tt = i18n( "Transit time: %1" ).arg( tt2 );
	}

	pmRiseTime->setText( rt );
	pmSetTime->setText( st );
	pmTransitTime->setText( tt ) ;
}

#include "kspopupmenu.moc"
