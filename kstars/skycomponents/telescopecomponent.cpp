/***************************************************************************
                          telescopecomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2006/02/26
    copyright            : (C) 2006 by Jasem Mutlaq
    email                : mutlaqja@ikarusteh.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QPixmap>
#include <QPainter>
#include <QFile>

#include "telescopecomponent.h"

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "skymap.h"
#include "starobject.h"
#include "indielement.h"
#include "indiproperty.h"
#include "indidevice.h"
#include "indimenu.h"
#include "devicemanager.h"

TelescopeComponent::TelescopeComponent(SkyComponent *parent, bool (*visibleMethod)()) 
: ListComponent(parent, visibleMethod)
{
}

TelescopeComponent::~TelescopeComponent()
{
}

void TelescopeComponent::init(KStarsData */*data*/)
{
  // No data initially, telescope markers are loaded and removed on demand
}

void TelescopeComponent::update( KStarsData* data, KSNumbers* /*num*/ )
{
  foreach (SkyObject *o, objectList())
	o->EquatorialToHorizontal(data->lst(), data->geo()->lat());

}

void TelescopeComponent::draw(KStars *ks, QPainter& psky, double /*scale*/)
{
	if ( ! visible() ) return;
	
	SkyMap *map = ks->map();

	INDI_P *eqNum;
	INDI_P *portConnect;
	INDI_E *lp;
	INDIMenu *devMenu = ks->getINDIMenu();
	bool useJ2000 (false), useAltAz(false);
	SkyPoint indi_sp;

	if (!Options::indiCrosshairs() || devMenu == NULL)
			return;

	psky.setPen( QPen( QColor( ks->data()->colorScheme()->colorNamed("TargetColor" ) ) ) );
	psky.setBrush( Qt::NoBrush );
	float pxperdegree = Options::zoomFactor()/57.3;

	for ( int i=0; i < devMenu->mgr.size(); i++ )
	{
		for ( int j=0; j < devMenu->mgr.at(i)->indi_dev.size(); j++ )
		{
			useAltAz = false;
			useJ2000 = false;

			// make sure the dev is on first
			if (devMenu->mgr.at(i)->indi_dev.at(j)->isOn())
			{
			        portConnect = devMenu->mgr.at(i)->indi_dev.at(j)->findProp("CONNECTION");

				if (!portConnect)
				 return;

				 if (portConnect->state == PS_BUSY)
				  return;

				eqNum = devMenu->mgr.at(i)->indi_dev.at(j)->findProp("EQUATORIAL_EOD_COORD");
					
				if (eqNum == NULL)
				{
					eqNum = devMenu->mgr.at(i)->indi_dev.at(j)->findProp("EQUATORIAL_COORD");
					if (eqNum == NULL)
					{
					eqNum = devMenu->mgr.at(i)->indi_dev.at(j)->findProp("HORIZONTAL_COORD");
						if (eqNum == NULL) continue;
						else
							useAltAz = true;
					}
					else
						useJ2000 = true;
				}

				// make sure it has RA and DEC properties
				if ( eqNum)
				{
					//fprintf(stderr, "Looking for RA label\n");
                                        if (useAltAz)
					{
						lp = eqNum->findElement("AZ");
						if (!lp)
							continue;

						dms azDMS(lp->value);
						
						lp = eqNum->findElement("ALT");
						if (!lp)
							continue;

						dms altDMS(lp->value);

						indi_sp.setAz(azDMS);
						indi_sp.setAlt(altDMS);

					}
					else
					{

						lp = eqNum->findElement("RA");
						if (!lp)
							continue;

						// express hours in degrees on the celestial sphere
						dms raDMS(lp->value);
						raDMS.setD ( raDMS.Degrees() * 15.0);

						lp = eqNum->findElement("DEC");
						if (!lp)
							continue;

						dms decDMS(lp->value);


						//kDebug() << "the KStars RA is " << raDMS.toHMSString() << endl;
						//kDebug() << "the KStars DEC is " << decDMS.toDMSString() << "\n****************" << endl;

						indi_sp.setRA(raDMS);
						indi_sp.setDec(decDMS);

						if (useJ2000)
						{
							indi_sp.setRA0(raDMS);
							indi_sp.setDec0(decDMS);
							indi_sp.apparentCoord( (double) J2000, ks->data()->ut().djd());
						}
							
						if ( Options::useAltAz() ) indi_sp.EquatorialToHorizontal( ks->LST(), ks->geo()->lat() );

					}

					QPointF P = map->getXY( &indi_sp, Options::useAltAz(), Options::useRefraction() );

					float s1 = 0.5*pxperdegree;
					float s2 = pxperdegree;
					float s3 = 2.0*pxperdegree;

					float x0 = P.x();        float y0 = P.y();
					float x1 = x0 - 0.5*s1;  float y1 = y0 - 0.5*s1;
					float x2 = x0 - 0.5*s2;  float y2 = y0 - 0.5*s2;
					float x3 = x0 - 0.5*s3;  float y3 = y0 - 0.5*s3;

					//Draw radial lines
					psky.drawLine( QPointF(x1, y0), QPointF(x3, y0) );
					psky.drawLine( QPointF(x0+s2, y0), QPointF(x0+0.5*s1, y0) );
					psky.drawLine( QPointF(x0, y1), QPointF(x0, y3) );
					psky.drawLine( QPointF(x0, y0+0.5*s1), QPointF(x0, y0+s2) );
					//Draw circles at 0.5 & 1 degrees
					psky.drawEllipse( QRectF(x1, y1, s1, s1) );
					psky.drawEllipse( QRectF(x2, y2, s2, s2) );

					psky.drawText( QPointF(x0+s2+2., y0), QString(devMenu->mgr.at(i)->indi_dev.at(j)->label) );

				}

			}
		}
	}
	
}

void TelescopeComponent::addTelescopeMarker(SkyObject *o)
{
  objectList().append(o);
}

void TelescopeComponent::removeTelescopeMarker(SkyObject *o)
{
   for (int i=0; i < objectList().count(); i++)
	if (objectList()[i] == o)
	{
  		objectList().removeAt(i);
		break;
	}
}

