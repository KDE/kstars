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

#include "deepskycomponent.h"

#include <QPainter>

#include <klocale.h>

#include "deepskyobject.h"
#include "dms.h"
#include "ksfilereader.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "skymap.h"
#include "Options.h"
#include "skymesh.h"

DeepSkyComponent::DeepSkyComponent( SkyComponent *parent )
: SkyComponent(parent)
{
    m_DeepSkyList = QList<DeepSkyObject*>();
	m_MessierList = QList<DeepSkyObject*>();
	m_NGCList = QList<DeepSkyObject*>();
	m_ICList = QList<DeepSkyObject*>();
	m_OtherList = QList<DeepSkyObject*>();

    m_skyMesh = SkyMesh::Instance();
}

DeepSkyComponent::~DeepSkyComponent()
{
	clear();
}

bool DeepSkyComponent::selected()
{
    return Options::showDeepSky();
}

void DeepSkyComponent::update( KStarsData *data, KSNumbers *num )
{}


void DeepSkyComponent::init(KStarsData *data)
{
    KSFileReader fileReader;
    if ( ! fileReader.open( "ngcic.dat" ) ) return; 

    fileReader.setProgress( i18n("Loading NGC/IC objects"), 13444, 10 );

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
        while ( line.mid(6,8).trimmed().isEmpty() && fileReader.hasMoreLines() ) {
            line = fileReader.readLine();
        }
        
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

        //major and minor axes
        ss = line.mid( 31, 5 );
        if (ss == "      " ) { a = 0.0; } else { a = ss.toFloat(); }
        ss = line.mid( 37, 5 );
        if (ss == "     " ) { b = 0.0; } else { b = ss.toFloat(); }
        //position angle.  The catalog PA is zero when the Major axis 
        //is horizontal.  But we want the angle measured from North, so 
        //we set PA = 90 - pa.
        ss = line.mid( 43, 3 );
        if (ss == "   " ) { pa = 90; } else { pa = 90 - ss.toInt(); }

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

        bool hasName = true;
        QString snum;
        if ( cat=="IC" || cat=="NGC" ) {
            snum.setNum( ingc );
            name = cat + ' ' + snum;
        } else if ( cat=="M" ) {
            snum.setNum( imess );
            name = cat + ' ' + snum;
            if ( cat2=="NGC" ) {
                snum.setNum( ingc );
                name2 = cat2 + ' ' + snum;
            } else if ( cat2=="IC" ) {
                snum.setNum( ingc );
                name2 = cat2 + ' ' + snum;
            } else {
                name2 = QString();
            }
        } 
        else {
            if ( ! longname.isEmpty() ) name = longname;
            else {
                hasName = false;
                name = i18n( "Unnamed Object" );
            }
        }

        // create new deepskyobject
        DeepSkyObject *o = 0;
        if ( type==0 ) type = 1; //Make sure we use CATALOG_STAR, not STAR
        o = new DeepSkyObject( type, r, d, mag, name, name2, longname, cat, a, b, pa, pgc, ugc );
        o->EquatorialToHorizontal( data->lst(), data->geo()->lat() );

        // Add the name(s) to the nameHash for fast lookup -jbb
        if ( hasName) {
            nameHash[ name ] = o;
            if ( ! longname.isEmpty() ) nameHash[ longname ] = o;
            if ( ! name2.isEmpty() ) nameHash[ name2 ] = o;
        }

        Trixel trixel = m_skyMesh->index( (SkyPoint*) o );

        //Assign object to general DeepSkyObjects list, 
        //and a secondary list based on its catalog.
        m_DeepSkyList.append( o );
        if ( o->isCatalogM()) {
            m_MessierList.append( o );
            appendIndex( o, &m_MessierIndex, trixel );
        }
        else if (o->isCatalogNGC() ) {
            m_NGCList.append( o );
             appendIndex( o, &m_NGCIndex, trixel );
        } 
        else if ( o->isCatalogIC() ) {
            m_ICList.append( o );
             appendIndex( o, &m_ICIndex, trixel );
        }
        else {
            m_OtherList.append( o );
             appendIndex( o, &m_OtherIndex, trixel );
        }

        //Add name to the list of object names
        if ( ! name.isEmpty() ) 
            objectNames(type).append( name );

        //Add long name to the list of object names
        if ( ! longname.isEmpty() && longname != name ) 
            objectNames(type).append( longname );
    
        fileReader.showProgress();
    }
}


void DeepSkyComponent::appendIndex( DeepSkyObject *o, DeepSkyIndex* dsIndex )
{
    MeshIterator region( m_skyMesh );
    while ( region.hasNext() ) {
        Trixel trixel = region.next();
        if ( ! dsIndex->contains( trixel ) ) {
            dsIndex->insert(trixel, new DeepSkyList() );
        }
        dsIndex->value( trixel )->append( o );
    }
}

void DeepSkyComponent::appendIndex( DeepSkyObject *o, DeepSkyIndex* dsIndex, Trixel trixel )
{
    if ( ! dsIndex->contains( trixel ) ) {
        dsIndex->insert(trixel, new DeepSkyList() );
    }
    dsIndex->value( trixel )->append( o );
}


void DeepSkyComponent::draw(KStars *ks, QPainter& psky, double scale)
{
	if ( ! selected() ) return;

	bool drawFlag;

    m_scale = scale;
    m_data  = ks->data();
    m_map   = ks->map();

	drawFlag = Options::showMessier() &&  
		! ( Options::hideOnSlew() && Options::hideMessier() && SkyMap::IsSlewing() );

    drawDeepSkyCatalog( psky, drawFlag, &m_MessierIndex, "MessColor" );

	drawFlag = Options::showNGC() &&  
		! ( Options::hideOnSlew() && Options::hideNGC() && SkyMap::IsSlewing() );

    drawDeepSkyCatalog( psky, drawFlag,     &m_NGCIndex,     "NGCColor" );

	drawFlag = Options::showIC() &&  
		! ( Options::hideOnSlew() && Options::hideIC() && SkyMap::IsSlewing() );

    drawDeepSkyCatalog( psky, drawFlag,      &m_ICIndex,      "ICColor" );

	drawFlag = Options::showOther() &&  
		! ( Options::hideOnSlew() && Options::hideOther() && SkyMap::IsSlewing() );

    drawDeepSkyCatalog( psky, drawFlag,   &m_OtherIndex,   "NGCColor" );
}

void DeepSkyComponent::drawDeepSkyCatalog( QPainter& psky, bool drawObject,
        DeepSkyIndex* dsIndex, const QString& colorString)
{
    bool drawImage =  Options::showMessierImages();
    if ( ! ( drawObject || drawImage ) ) return; 

    UpdateID updateID = m_data->updateID();
    UpdateID updateNumID = m_data->updateNumID();

	psky.setPen( m_data->colorScheme()->colorNamed( colorString ) );
	psky.setBrush( Qt::NoBrush );
    QColor color        = m_data->colorScheme()->colorNamed( colorString );
    QColor colorExtra = m_data->colorScheme()->colorNamed( "HSTColor" );

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

    //DrawID drawID = m_skyMesh->drawID();
    MeshIterator region( m_skyMesh, DRAW_BUF );

    while ( region.hasNext() ) {

        Trixel trixel = region.next();
        DeepSkyList* dsList = dsIndex->value( trixel );
        if ( dsList == 0 ) continue;
        for (int j = 0; j < dsList->size(); j++ ) {
            DeepSkyObject *obj = dsList->at( j );

            //if ( obj->drawID == drawID ) continue;  // only draw each line once
            //obj->drawID = drawID;

			if ( ! m_map->checkVisibility( obj ) ) continue;

            if ( obj->updateID != updateID ) {
                obj->updateID = updateID;
                if ( obj->updateNumID != updateNumID) {
                    obj->updateCoords( m_data->updateNum() );
                }
                obj->EquatorialToHorizontal( m_data->lst(), m_data->geo()->lat() );
            }

   			float mag = obj->mag();
   			float size = m_scale * obj->a() * dms::PI * Options::zoomFactor() / 10800.0;

   			//only draw objects if flags set, it's bigger than 1 pixel (unless 
   			//zoom > 2000.), and it's brighter than maglim (unless mag is 
   			//undefined (=99.9)
   			if ( (size > 1.0 || Options::zoomFactor() > 2000.) && 
   					 (mag > 90.0 || mag < (float)maglim) ) {
   				QPointF o = m_map->toScreen( obj, m_scale );
                   if ( ! m_map->onScreen( o ) ) continue;
   				double PositionAngle = m_map->findPA( obj, o.x(), o.y(), m_scale );

   				//Draw Image
   				if ( drawImage && Options::zoomFactor() > 5.*MINZOOM ) {
   					obj->drawImage( psky, o.x(), o.y(), PositionAngle, Options::zoomFactor(), m_scale );
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

   					obj->drawSymbol( psky, o.x(), o.y(), PositionAngle, Options::zoomFactor(), m_scale );

   					if ( bColorChanged ) {
   						psky.setPen( color );
   					}
				}
			} 
            else { //Object failed checkVisible(); delete it's Image pointer, if it exists.
				if ( obj->image() ) {
					obj->deleteImage();
				}
			}
		}
	}
}


SkyObject* DeepSkyComponent::findByName( const QString &name ) {

    return nameHash[ name ];
}

//we multiply each catalog's smallest angular distance by the 
//following factors before selecting the final nearest object:
// IC catalog = 0.8
// NGC catalog = 0.5
// "other" catalog = 0.4
// Messier object = 0.25
SkyObject* DeepSkyComponent::objectNearest( SkyPoint *p, double &maxrad ) {

	if ( ! selected() ) return 0;

	SkyObject *oTry = 0;
	SkyObject *oBest = 0;
	double rTry = maxrad;
	double rBest = maxrad;
	double r;
    DeepSkyList* dsList;
    SkyObject* obj;

    MeshIterator region( m_skyMesh, OBJ_NEAREST_BUF );
    while ( region.hasNext() ) {
        dsList = m_ICIndex[ region.next() ];
        if ( ! dsList ) continue;
        for (int i=0; i < dsList->size(); ++i) {
            obj = (SkyObject*) dsList->at( i );
		    r = obj->angularDistanceTo( p ).Degrees();
		    if ( r < rTry ) {
			    rTry = r;
			    oTry = obj;
		    }
        }
	}

	rTry *= 0.8;
	if ( rTry < rBest ) {
		rBest = rTry;
		oBest = oTry;
	}

	rTry = maxrad;
    region.reset();
    while ( region.hasNext() ) {
        dsList = m_NGCIndex[ region.next() ];
        if ( ! dsList ) continue;
        for (int i=0; i < dsList->size(); ++i) {
            obj = (SkyObject*) dsList->at( i );
		    r = obj->angularDistanceTo( p ).Degrees();
		    if ( r < rTry ) {
			    rTry = r;
			    oTry = obj;
		    }
        }
	}

	rTry *= 0.6;
	if ( rTry < rBest ) {
		rBest = rTry;
		oBest = oTry;
	}

	rTry = maxrad;

    region.reset();
    while ( region.hasNext() ) {
        dsList = m_OtherIndex[ region.next() ];
        if ( ! dsList ) continue;
        for (int i=0; i < dsList->size(); ++i) {
            obj = (SkyObject*) dsList->at( i );
		    r = obj->angularDistanceTo( p ).Degrees();
		    if ( r < rTry ) {
			    rTry = r;
			    oTry = obj;
		    }
        }
	}

	rTry *= 0.6;
	if ( rTry < rBest ) {
		rBest = rTry;
		oBest = oTry;
	}

	rTry = maxrad;

    region.reset();
    while ( region.hasNext() ) {
        dsList = m_MessierIndex[ region.next() ];
        if ( ! dsList ) continue;
        for (int i=0; i < dsList->size(); ++i) {
            obj = (SkyObject*) dsList->at( i );
		    r = obj->angularDistanceTo( p ).Degrees();
		    if ( r < rTry ) {
			    rTry = r;
			    oTry = obj;
		    }
        }
	}

    // -jbb: this is the template of the non-indexed way
    //
	//foreach ( SkyObject *o, m_MessierList ) {
	//	r = o->angularDistanceTo( p ).Degrees();
	//	if ( r < rTry ) {
	//		rTry = r;
	//		oTry = o;
	//	}
	//}

	rTry *= 0.5;
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
		int i = objectNames(o->type()).indexOf( o->name() );
		if ( i >= 0 ) objectNames(o->type()).removeAt( i );
		i = objectNames(o->type()).indexOf( o->longname() );
		if ( i >= 0 ) objectNames(o->type()).removeAt( i );

		delete o;
	}

	while ( ! m_NGCList.isEmpty() ) {
		SkyObject *o = m_NGCList.takeFirst();
		int i = objectNames(o->type()).indexOf( o->name() );
		if ( i >= 0 ) objectNames(o->type()).removeAt( i );
		i = objectNames(o->type()).indexOf( o->longname() );
		if ( i >= 0 ) objectNames(o->type()).removeAt( i );

		delete o;
	}

	while ( ! m_ICList.isEmpty() ) {
		SkyObject *o = m_ICList.takeFirst();
		int i = objectNames(o->type()).indexOf( o->name() );
		if ( i >= 0 ) objectNames(o->type()).removeAt( i );
		i = objectNames(o->type()).indexOf( o->longname() );
		if ( i >= 0 ) objectNames(o->type()).removeAt( i );

		delete o;
	}

	while ( ! m_OtherList.isEmpty() ) {
		SkyObject *o = m_OtherList.takeFirst();
		int i = objectNames(o->type()).indexOf( o->name() );
		if ( i >= 0 ) objectNames(o->type()).removeAt( i );
		i = objectNames(o->type()).indexOf( o->longname() );
		if ( i >= 0 ) objectNames(o->type()).removeAt( i );

		delete o;
	}
}
