/***************************************************************************
                          deepskycomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/15/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QFile>
#include <QPainter>
#include <klocale.h>

#include "deepskycomponent.h"

#include "deepskyobject.h"
#include "dms.h"
#include "ksfilereader.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "skymap.h"
#include "Options.h"

DeepSkyComponent::DeepSkyComponent( SkyComponent *parent, bool (*vMethodDeepSky)(), 
	bool (*vMethodMess)(), bool (*vMethodNGC)(), 
	bool (*vMethodIC)(), bool (*vMethodOther)(), bool (*vMethodImages)() ) 
: SkyComponent(parent, vMethodDeepSky)
{
	visibleMessier = vMethodMess;
	visibleNGC = vMethodNGC;
	visibleIC = vMethodIC;
	visibleOther = vMethodOther;
	visibleImages = vMethodImages;
}

DeepSkyComponent::~DeepSkyComponent()
{
	clear();
}

void DeepSkyComponent::init(KStarsData *data)
{
	QFile file;

	for ( unsigned int i=0; i<NNGCFILES; ++i ) {
		emitProgressText( i18n( "Loading NGC/IC objects (%1%)", int(100.*float(i)/float(NNGCFILES)) ) );

		QString fname = QString().sprintf( "ngcic%02d.dat", i+1 );

		if ( KSUtils::openDataFile( file, fname ) ) {
			KSFileReader fileReader( file ); // close file is included
			while ( fileReader.hasMoreLines() ) {
				QString line, con, ss, name, name2, longname;
				QString cat, cat2, sgn;
				float mag(1000.0), ras, a, b;
				int type, ingc, imess(-1), rah, ram, dd, dm, ds, pa;
				int pgc, ugc;
				QChar iflag;

				line = fileReader.readLine();
				//Ignore comment lines
				while ( line.at(0) == '#' && fileReader.hasMoreLines() ) line = fileReader.readLine();
				//Ignore lines with no coordinate values
				while ( line.mid(6,8).trimmed().isEmpty() && fileReader.hasMoreLines() ) line = fileReader.readLine();
				
				iflag = line.at( 0 ); //check for NGC/IC catalog flag
				if ( iflag == 'I' ) cat = "IC";
				else if ( iflag == 'N' ) cat = "NGC";

				ingc = line.mid( 1, 4 ).toInt();  // NGC/IC catalog number
				if ( ingc==0 ) cat = QString(); //object is not in NGC or IC catalogs

				//coordinates
				rah = line.mid( 6, 2 ).toInt();
				ram = line.mid( 8, 2 ).toInt();
				ras = line.mid( 10, 4 ).toFloat();
				sgn = line.mid( 15, 1 ); //don't use at(), because it crashes on invalid index
				dd = line.mid( 16, 2 ).toInt();
				dm = line.mid( 18, 2 ).toInt();
				ds = line.mid( 20, 2 ).toInt();

				//B magnitude
				ss = line.mid( 23, 4 );
			  if (ss == "    " ) { mag = 99.9; } else { mag = ss.toFloat(); }

				//object type
				type = line.mid( 29, 1 ).toInt();

				//major and minor axes and position angle
				ss = line.mid( 31, 5 );
				if (ss == "      " ) { a = 0.0; } else { a = ss.toFloat(); }
				ss = line.mid( 37, 5 );
				if (ss == "     " ) { b = 0.0; } else { b = ss.toFloat(); }
				ss = line.mid( 43, 3 );
				if (ss == "   " ) { pa = 0; } else { pa = ss.toInt(); }

				//PGC number
				ss = line.mid( 47, 6 );
				if (ss == "      " ) { pgc = 0; } else { pgc = ss.toInt(); }

				//UGC number
				if ( line.mid( 54, 3 ) == "UGC" ) {
					ugc = line.mid( 58, 5 ).toInt();
				} else {
					ugc = 0;
				}

				//Messier number
				if ( line.mid( 70,1 ) == "M" ) {
					cat2 = cat;
					if ( ingc==0 ) cat2 = QString();
					cat = "M";
					imess = line.mid( 72, 3 ).toInt();
				}

				longname = line.mid( 76, line.length() ).trimmed();

				dms r;
				r.setH( rah, ram, int(ras) );
				dms d( dd, dm, ds );

				if ( sgn == "-" ) { d.setD( -1.0*d.Degrees() ); }

				QString snum;
				if ( cat=="IC" || cat=="NGC" ) {
					snum.setNum( ingc );
					name = cat + " " + snum;
				} else if ( cat=="M" ) {
					snum.setNum( imess );
					name = cat + " " + snum;
					if ( cat2=="NGC" ) {
						snum.setNum( ingc );
						name2 = cat2 + " " + snum;
					} else if ( cat2=="IC" ) {
						snum.setNum( ingc );
						name2 = cat2 + " " + snum;
					} else {
						name2 = QString();
					}
				} else {
					if ( longname.isEmpty() ) name = i18n( "Unnamed Object" );
					else name = longname;
				}

				// create new deepskyobject
				DeepSkyObject *o = 0;
				if ( type==0 ) type = 1; //Make sure we use CATALOG_STAR, not STAR
				o = new DeepSkyObject( type, r, d, mag, name, name2, longname, cat, a, b, pa, pgc, ugc );
				o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );

//FIXME: do we need a master deep sky object list?
//				// keep object in deep sky objects' list
//				deepSkyList.append( o );

				if ( o->isCatalogM()) {
					m_MessierList.append( o );
				} else if (o->isCatalogNGC() ) {
					m_NGCList.append( o );
				} else if ( o->isCatalogIC() ) {
					m_ICList.append( o );
				} else {
					m_OtherList.append( o );
				}

				//Add name to the list of object names
				if ( ! name.isEmpty() ) 
					objectNames().append( name );

				//Add long name to the list of object names
				if ( ! longname.isEmpty() && longname != name ) 
					objectNames().append( longname );

			} //end while-filereader
		}
	} //end for-loop through ngcic files
}

void DeepSkyComponent::update( KStarsData *data, KSNumbers *num )
{
	if ( visible() ) {
		foreach ( SkyObject *o, objectList() ) {
			if ( num ) o->updateCoords( num );
			o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
		}
	}
}

void DeepSkyComponent::draw(KStars *ks, QPainter& psky, double scale)
{
	if ( ! visible() ) return;

	KStarsData *data = ks->data();
	SkyMap *map = ks->map();

	QImage ScaledImage;

	// calculate color objects once, outside the loop
	QColor colorMess = data->colorScheme()->colorNamed( "MessColor" );
	QColor colorIC  = data->colorScheme()->colorNamed( "ICColor" );
	QColor colorNGC  = data->colorScheme()->colorNamed( "NGCColor" );
	QColor colorExtra  = data->colorScheme()->colorNamed( "HSTColor" );

	// draw Messier catalog
	drawDeepSkyCatalog( psky, map, m_MessierList, colorMess, colorExtra, 
			visibleMessier(), visibleImages(), scale );

	// draw NGC Catalog
	drawDeepSkyCatalog( psky, map, m_NGCList, colorNGC, colorExtra, 
			visibleNGC(), visibleImages(), scale );

	// draw IC catalog
	drawDeepSkyCatalog( psky, map, m_ICList, colorIC, colorExtra, 
			visibleIC(), visibleImages(), scale );

	// draw the rest
	//FIXME: Add a color for non NGC/IC objects ?
	drawDeepSkyCatalog( psky, map, m_OtherList, colorNGC, colorExtra, 
			visibleOther(), visibleImages(), scale );
}

void DeepSkyComponent::drawDeepSkyCatalog( QPainter& psky, SkyMap *map,
				QList<DeepSkyObject*>& catalog, QColor& color, QColor &colorExtra,
				bool drawObject, bool drawImage, double scale )
{
	if ( drawObject || drawImage ) {  //don't do anything if nothing is to be drawn!
		float Width = scale * map->width();
		float Height = scale * map->height();

		// Set color once
		psky.setPen( color );
		psky.setBrush( Qt::NoBrush );

		double maglim = Options::magLimitDrawDeepSky();

		//FIXME
		//disabling faint limits until the NGC/IC catalog has reasonable mags
		//adjust maglimit for ZoomLevel
		//double lgmin = log10(MINZOOM);
		//double lgmax = log10(MAXZOOM);
		//double lgz = log10(Options::zoomFactor());
		//if ( lgz <= 0.75*lgmax ) maglim -= (Options::magLimitDrawDeepSky() - Options::magLimitDrawDeepSkyZoomOut() )*(0.75*lgmax - lgz)/(0.75*lgmax - lgmin);
		//else
		maglim = 40.0; //show all deep-sky objects

		foreach ( DeepSkyObject *obj, catalog ) {
			if ( map->checkVisibility( obj ) ) {
				float mag = obj->mag();
				float size = scale * obj->a() * dms::PI * Options::zoomFactor() / 10800.0;

				//only draw objects if flags set, it's bigger than 1 pixel (unless 
				//zoom > 2000.), and it's brighter than maglim (unless mag is 
				//undefined (=99.9)
				if ( (size > 1.0 || Options::zoomFactor() > 2000.) && 
						 (mag > 90.0 || mag < (float)maglim) ) {
					QPointF o = map->getXY( obj, Options::useAltAz(), Options::useRefraction(), scale );
					if ( o.x() >= 0. && o.x() <= Width && o.y() >= 0. && o.y() <= Height ) {
						//PA for Deep-Sky objects is 90 + PA because major axis is horizontal at PA=0
						double PositionAngle = 90. + map->findPA( obj, o.x(), o.y(), scale );

						//Draw Image
						if ( drawImage && Options::zoomFactor() > 5.*MINZOOM ) {
							obj->drawImage( psky, o.x(), o.y(), PositionAngle, Options::zoomFactor(), scale );
						}

						//Draw Symbol
						if ( drawObject ) {
							//change color if extra images are available
							// most objects don't have those, so we only change colors temporarily
							// for the few exceptions. Changing color is expensive!!!
							bool bColorChanged = false;
							if ( obj->isCatalogM() && obj->ImageList.count() > 1 ) {
								psky.setPen( colorExtra );
								bColorChanged = true;
							} else if ( (!obj->isCatalogM()) && obj->ImageList.count() ) {
								psky.setPen( colorExtra );
								bColorChanged = true;
							}

							obj->drawSymbol( psky, o.x(), o.y(), PositionAngle, Options::zoomFactor(), scale );

							// revert temporary color change
							if ( bColorChanged ) {
								psky.setPen( color );
							}
						}
					}
				}
			} else { //Object failed checkVisible(); delete it's Image pointer, if it exists.
				if ( obj->image() ) {
					obj->deleteImage();
				}
			}
		}
	}
}

SkyObject* DeepSkyComponent::findByName( const QString &name ) {
	foreach ( SkyObject *o, m_MessierList ) 
		if ( o->name() == name || o->longname() == name || o->name2() == name )
			return o;

	foreach ( SkyObject *o, m_NGCList ) 
		if ( o->name() == name || o->longname() == name || o->name2() == name )
			return o;

	foreach ( SkyObject *o, m_ICList ) 
		if ( o->name() == name || o->longname() == name || o->name2() == name )
			return o;

	foreach ( SkyObject *o, m_OtherList ) 
		if ( o->name() == name || o->longname() == name || o->name2() == name )
			return o;

	//No object found
	return 0;
}

//we multiply each catalog's smallest angular distance by the 
//following factors before selecting the final nearest object:
// IC catalog = 0.8
// NGC catalog = 0.5
// "other" catalog = 0.4
// Messier object = 0.25
SkyObject* DeepSkyComponent::objectNearest( SkyPoint *p, double &maxrad ) {
	SkyObject *oTry = 0;
	SkyObject *oBest = 0;
	double rTry = maxrad;
	double rBest = maxrad;
	double r;

	foreach ( SkyObject *o, m_ICList )  {
		r = o->angularDistanceTo( p ).Degrees();
		if ( r < rTry ) {
			rTry = r;
			oTry = o;
		}
	}
	rTry *= 0.8;
	if ( rTry < rBest ) {
		rBest = rTry;
		oBest = oTry;
	}

	rTry = maxrad;
	foreach ( SkyObject *o, m_NGCList )  {
		r = o->angularDistanceTo( p ).Degrees();
		if ( r < rTry ) {
			rTry = r;
			oTry = o;
		}
	}
	rTry *= 0.5;
	if ( rTry < rBest ) {
		rBest = rTry;
		oBest = oTry;
	}

	rTry = maxrad;
	foreach ( SkyObject *o, m_OtherList )  {
		r = o->angularDistanceTo( p ).Degrees();
		if ( r < rTry ) {
			rTry = r;
			oTry = o;
		}
	}
	rTry *= 0.4;
	if ( rTry < rBest ) {
		rBest = rTry;
		oBest = oTry;
	}

	rTry = maxrad;
	foreach ( SkyObject *o, m_MessierList ) {
		r = o->angularDistanceTo( p ).Degrees();
		if ( r < rTry ) {
			rTry = r;
			oTry = o;
		}
	}
	rTry *= 0.25;
	if ( rTry < rBest ) {
		rBest = rTry;
		oBest = oTry;
	}

	maxrad = rBest;
	return oBest;
}

void DeepSkyComponent::clear() {
	while ( ! m_MessierList.isEmpty() ) {
		SkyObject *o = m_MessierList.takeFirst();
		int i = objectNames().indexOf( o->name() );
		if ( i >= 0 ) objectNames().removeAt( i );
		i = objectNames().indexOf( o->longname() );
		if ( i >= 0 ) objectNames().removeAt( i );

		delete o;
	}

	while ( ! m_NGCList.isEmpty() ) {
		SkyObject *o = m_NGCList.takeFirst();
		int i = objectNames().indexOf( o->name() );
		if ( i >= 0 ) objectNames().removeAt( i );
		i = objectNames().indexOf( o->longname() );
		if ( i >= 0 ) objectNames().removeAt( i );

		delete o;
	}

	while ( ! m_ICList.isEmpty() ) {
		SkyObject *o = m_ICList.takeFirst();
		int i = objectNames().indexOf( o->name() );
		if ( i >= 0 ) objectNames().removeAt( i );
		i = objectNames().indexOf( o->longname() );
		if ( i >= 0 ) objectNames().removeAt( i );

		delete o;
	}

	while ( ! m_OtherList.isEmpty() ) {
		SkyObject *o = m_OtherList.takeFirst();
		int i = objectNames().indexOf( o->name() );
		if ( i >= 0 ) objectNames().removeAt( i );
		i = objectNames().indexOf( o->longname() );
		if ( i >= 0 ) objectNames().removeAt( i );

		delete o;
	}
}
