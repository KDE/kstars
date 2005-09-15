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

#include "Options.h"

DeepSkyComponent::DeepSkyComponent(SkyComposite *parent) : SkyComponent(parent)
{
}

DeepSkyComponent::~DeepSkyComponent()
{
	while ( !deepSkyList.isEmpty() ) delete deepSkyList.takeFirst();
}

DeepSkyComponent::init(KStarsData *data)
{
	readDeepSkyData();
}

DeepSkyComponent::update(KStarsData *data, KSNumbers *num, bool needNewCoords)
{
	if ( Options::showMessier() || Options::showMessierImages() ) {
		for ( int i=0; i < deepSkyListMessier.size(); ++i ) {
			SkyObject *o = deepSkyListMessier[i];
			if (needNewCoords) o->updateCoords( &num );
			o->EquatorialToHorizontal( LST, geo->lat() );
		}
	}
	if ( Options::showNGC() ) {
		for ( int i=0; i < deepSkyListNGC.size(); ++i ) {
			SkyObject *o = deepSkyListNGC[i];
			if (needNewCoords) o->updateCoords( &num );
			o->EquatorialToHorizontal( LST, geo->lat() );
		}
	}
	if ( Options::showIC() ) {
		for ( int i=0; i < deepSkyListIC.size(); ++i ) {
			SkyObject *o = deepSkyListIC[i];
			if (needNewCoords) o->updateCoords( &num );
			o->EquatorialToHorizontal( LST, geo->lat() );
		}
	}
	if ( Options::showOther() ) {
		for ( int i=0; i < deepSkyListOther.size(); ++i ) {
			SkyObject *o = deepSkyListOther[i];
			if (needNewCoords) o->updateCoords( &num );
			o->EquatorialToHorizontal( LST, geo->lat() );
		}
	}
}


void DeepSkyComponent::draw(SkyMap *map, QPainter& psky, double scale)
{
	int Width = int( scale * map->width() );
	int Height = int( scale * map->height() );

	QImage ScaledImage;

	bool checkSlewing = ( (slewing || ( clockSlewing && data->clock()->isActive()) )
				&& Options::hideOnSlew() );

	//shortcuts to inform wheter to draw different objects
	bool drawMess( Options::showDeepSky() && ( Options::showMessier() || Options::showMessierImages() ) && !(checkSlewing && Options::hideMessier() ) );
	bool drawNGC( Options::showDeepSky() && Options::showNGC() && !(checkSlewing && Options::hideNGC() ) );
	bool drawOther( Options::showDeepSky() && Options::showOther() && !(checkSlewing && Options::hideOther() ) );
	bool drawIC( Options::showDeepSky() && Options::showIC() && !(checkSlewing && Options::hideIC() ) );
	bool drawImages( Options::showMessierImages() );

	// calculate color objects once, outside the loop
	QColor colorMess = data->colorScheme()->colorNamed( "MessColor" );
	QColor colorIC  = data->colorScheme()->colorNamed( "ICColor" );
	QColor colorNGC  = data->colorScheme()->colorNamed( "NGCColor" );

	// draw Messier catalog
	if ( drawMess ) {
		drawDeepSkyCatalog( psky, deepSkyListMessier, colorMess, Options::showMessier(), drawImages, scale );
	}

	// draw NGC Catalog
	if ( drawNGC ) {
		drawDeepSkyCatalog( psky, deepSkyListNGC, colorNGC, true, drawImages, scale );
	}

	// draw IC catalog
	if ( drawIC ) {
		drawDeepSkyCatalog( psky, deepSkyListIC, colorIC, true, drawImages, scale );
	}

	// draw the rest
	if ( drawOther ) {
		//Use NGC color for now...
		drawDeepSkyCatalog( psky, deepSkyListOther, colorNGC, true, drawImages, scale );
	}
}

//02/2003: NEW: split data files, using Heiko's new KSFileReader.
bool DeepSkyComponent::readDeepSkyData()
{
	QFile file;

	for ( unsigned int i=0; i<NNGCFILES; ++i ) {
		QString snum, fname;
		snum = QString().sprintf( "%02d", i+1 );
		fname = "ngcic" + snum + ".dat";

		emit progressText( i18n( "Loading NGC/IC Data (%1%)" ).arg( int(100.*float(i)/float(NNGCFILES)) ) );

		if ( KSUtils::openDataFile( file, fname ) ) {
			KSFileReader fileReader( file ); // close file is included
			while ( fileReader.hasMoreLines() ) {
				QString line, con, ss, name, name2, longname;
				QString cat, cat2;
				float mag(1000.0), ras, a, b;
				int type, ingc, imess(-1), rah, ram, dd, dm, ds, pa;
				int pgc, ugc;
				QChar sgn, iflag;

				line = fileReader.readLine();
				//Ignore comment lines
				while ( line.at(0) == '#' && fileReader.hasMoreLines() ) line = fileReader.readLine();
				//Ignore lines with no coordinate values
				while ( line.mid(6,8).stripWhiteSpace().isEmpty() ) line = fileReader.readLine();
				
				iflag = line.at( 0 ); //check for NGC/IC catalog flag
				if ( iflag == 'I' ) cat = "IC";
				else if ( iflag == 'N' ) cat = "NGC";

				ingc = line.mid( 1, 4 ).toInt();  // NGC/IC catalog number
				if ( ingc==0 ) cat = ""; //object is not in NGC or IC catalogs

				//coordinates
				rah = line.mid( 6, 2 ).toInt();
				ram = line.mid( 8, 2 ).toInt();
				ras = line.mid( 10, 4 ).toFloat();
				sgn = line.at( 15 );
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
				if ( line.at( 70 ) == 'M' ) {
					cat2 = cat;
					if ( ingc==0 ) cat2 = "";
					cat = "M";
					imess = line.mid( 72, 3 ).toInt();
				}

				longname = line.mid( 76, line.length() ).stripWhiteSpace();

				dms r;
				r.setH( rah, ram, int(ras) );
				dms d( dd, dm, ds );

				if ( sgn == "-" ) { d.setD( -1.0*d.Degrees() ); }

//				QString snum;
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
						name2 = "";
					}
				} else {
					if ( longname.isEmpty() ) name = i18n( "Unnamed Object" );
					else name = longname;
				}

				// create new deepskyobject
				DeepSkyObject *o = 0;
				if ( type==0 ) type = 1; //Make sure we use CATALOG_STAR, not STAR
				o = new DeepSkyObject( type, r, d, mag, name, name2, longname, cat, a, b, pa, pgc, ugc );

				// keep object in deep sky objects' list
				deepSkyList.append( o );
				// plus: keep objects separated for performance reasons. Switching the colors during
				// paint is too expensive.
				if ( o->isCatalogM()) {
					deepSkyListMessier.append( o );
				} else if (o->isCatalogNGC() ) {
					deepSkyListNGC.append( o );
				} else if ( o->isCatalogIC() ) {
					deepSkyListIC.append( o );
				} else {
					deepSkyListOther.append( o );
				}

				// list of object names
				ObjNames.append( (SkyObject*)o );

				//Add longname to objList, unless longname is the same as name
				if ( !o->longname().isEmpty() && o->name() != o->longname() && o->hasName() ) {
					ObjNames.append( o, true );  // append with longname
				}
			} //end while-filereader
		} else { //one of the files could not be opened
			return false;
		}
	} //end for-loop through files

	return true;
}

void DeepSkyComponent::drawDeepSkyCatalog( QPainter& psky, 
				QList<DeepSkyObject*>& catalog, QColor& color,
				bool drawObject, bool drawImage, double scale )
{
	if ( drawObject || drawImage ) {  //don't do anything if nothing is to be drawn!
		int Width = int( scale * width() );
		int Height = int( scale * height() );

		// Set color once
		psky.setPen( color );
		psky.setBrush( NoBrush );
		QColor colorHST  = data->colorScheme()->colorNamed( "HSTColor" );

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

		for ( int i=0; i < catalog.size(); ++i ) {
			DeepSkyObject *obj = catalog[i];
			if ( checkVisibility( obj, fov(), XRange ) ) {
				float mag = obj->mag();
				//only draw objects if flags set and its brighter than maglim (unless mag is undefined (=99.9)
				if ( mag > 90.0 || mag < (float)maglim ) {
					QPoint o = getXY( obj, Options::useAltAz(), Options::useRefraction(), scale );
					if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) {
						//PA for Deep-Sky objects is 90 + PA because major axis is horizontal at PA=0
						double PositionAngle = 90. + findPA( obj, o.x(), o.y(), scale );

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
								psky.setPen( colorHST );
								bColorChanged = true;
							} else if ( (!obj->isCatalogM()) && obj->ImageList.count() ) {
								psky.setPen( colorHST );
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
