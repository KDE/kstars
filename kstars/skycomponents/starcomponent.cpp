/***************************************************************************
                          starcomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/14/08
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
#include "starcomponent.h"

#include "Options.h"
//Added by qt3to4:
#include <QPixmap>

StarComponent::StarComponent(SkyComponent *parent, bool (*visibleMethod)()) 
: ListComponent(parent, visibleMethod)
{
	starList = 0;
}

StarComponent::~StarComponent()
{
	delete starList;
}

void StarComponent::init(KStarsData *data)
{
	starList = new QList()<StarObject*>;
	
	readStarData();
}

void StarComponent::draw(KStars *ks, QPainter& psky, double scale)
{
	if ( !Options::showStars() ) return;
	
	SkyMap *map = ks->map();
	int Width = int( scale * map->width() );
	int Height = int( scale * map->height() );

	bool checkSlewing = ( map->isSlewing() && Options::hideOnSlew() );

//shortcuts to inform wheter to draw different objects
	bool hideFaintStars( checkSlewing && Options::hideStars() );

	//adjust maglimit for ZoomLevel
	double lgmin = log10(MINZOOM);
	double lgmax = log10(MAXZOOM);
	double lgz = log10(Options::zoomFactor());

	double maglim = Options::magLimitDrawStar();
	if ( lgz <= 0.75*lgmax ) maglim -= (Options::magLimitDrawStar() - Options::magLimitDrawStarZoomOut() )*(0.75*lgmax - lgz)/(0.75*lgmax - lgmin);
	float sizeFactor = 6.0 + (lgz - lgmin);

	for (StarObject *curStar = starList->first(); curStar; curStar = starList->next())
	{
		// break loop if maglim is reached
		if ( curStar->mag() > maglim || ( hideFaintStars && curStar->mag() > Options::magLimitHideStar() ) ) break;

		if ( map->checkVisibility( curStar ) )
		{
			QPoint o = map->getXY( curStar, Options::useAltAz(), Options::useRefraction(), scale );

			// draw star if currently on screen
			if (o.x() >= 0 && o.x() <= Width && o.y() >=0 && o.y() <= Height )
			{
				int size = int( scale * ( sizeFactor*( maglim - curStar->mag())/maglim ) + 1 );

				if ( size > 0 )
				{
					QChar c = curStar->color();
					QPixmap *spixmap = starpix->getPixmap( &c, size );
					curStar->draw( psky, sky, spixmap, o.x(), o.y(), true, scale );

					// now that we have drawn the star, we can display some extra info
					//don't label unnamed stars with the generic "star" name
					bool drawName = ( Options::showStarNames() && (curStar->name() != i18n("star") ) );
					if ( !checkSlewing && (curStar->mag() <= Options::magLimitDrawStarInfo() )
							&& ( drawName || Options::showStarMagnitudes() ) ) {

						psky.setPen( QColor( data->colorScheme()->colorNamed( "SNameColor" ) ) );
						QFont stdFont( psky.font() );
						QFont smallFont( stdFont );
						smallFont.setPointSize( stdFont.pointSize() - 2 );
						if ( Options::zoomFactor() < 10.*MINZOOM ) {
							psky.setFont( smallFont );
						} else {
							psky.setFont( stdFont );
						}

						curStar->drawLabel( psky, o.x(), o.y(), Options::zoomFactor(),
								drawName, Options::showStarMagnitudes(), scale );

						//reset font
						psky.setFont( stdFont );
					}
				}
			}
		}
	}
}

bool StarComponent::openStarFile(int i)
{
	if (starFileReader != 0) delete starFileReader;
	QFile file;
	QString snum, fname;
	snum = QString().sprintf("%03d", i);
	fname = "hip" + snum + ".dat";
	if (KSUtils::openDataFile(file, fname))
	{
		starFileReader = new KSFileReader(file); // close file is included
	}
	else
	{
		starFileReader = 0;
		return false;
	}
	return true;
}

bool StarComponent::readStarData()
{
	bool ready = false;

	float loadUntilMag = MINDRAWSTARMAG;
	if (Options::magLimitDrawStar() > loadUntilMag) loadUntilMag = Options::magLimitDrawStar();
	
	for (unsigned int i=1; i<NHIPFILES+1; ++i) {
		emit progressText( i18n( "Loading Star Data (%1%)" ).arg( int(100.*float(i)/float(NHIPFILES)) ) );
		
		if (openStarFile(i) == true) {
			while (starFileReader->hasMoreLines()) {
				QString line;
				float mag;

				line = starFileReader->readLine();
				if ( line.left(1) != "#" ) {  //ignore comments
					// check star magnitude
					mag = line.mid( 46,5 ).toFloat();
					if ( mag > loadUntilMag ) {
						ready = true;
						break;
					}

					processStar(&line);
				}
			}  // end of while

		} else { //one of the star files could not be read.
			if ( starList->count() ) return true;
			else return false;
		}
		// magnitude level is reached
		if (ready == true) break;
	}

//Store the max set magnitude of current session. Will increase in KStarsData::appendNewData()
	maxSetMagnitude = Options::magLimitDrawStar();
	delete starFileReader;
	starFileReader = 0;
	return true;
}

void StarComponent::processStar( QString *line, bool reloadMode ) {
	QString name, gname, SpType;
	int rah, ram, ras, ras2, dd, dm, ds, ds2;
	bool mult, var;
	QChar sgn;
	double mag, bv, dmag, vper;
	double pmra, pmdec, plx;

	name = ""; gname = "";

	//parse coordinates
	rah = line->mid( 0, 2 ).toInt();
	ram = line->mid( 2, 2 ).toInt();
	ras = int(line->mid( 4, 5 ).toDouble());
	ras2 = int(60.0*(line->mid( 4, 5 ).toDouble()-ras) + 0.5); //add 0.5 to get nearest integer with int()

	sgn = line->at(10);
	dd = line->mid(11, 2).toInt();
	dm = line->mid(13, 2).toInt();
	ds = int(line->mid(15, 4).toDouble());
	ds2 = int(60.0*(line->mid( 15, 5 ).toDouble()-ds) + 0.5); //add 0.5 to get nearest integer with int()

	//parse proper motion and parallax
	pmra = line->mid( 20, 9 ).toDouble();
	pmdec = line->mid( 29, 9 ).toDouble();
	plx = line->mid( 38, 7 ).toDouble();

	//parse magnitude, B-V color, and spectral type
	mag = line->mid( 46, 5 ).toDouble();
	bv  = line->mid( 51, 5 ).toDouble();
	SpType = line->mid(56, 2);

	//parse multiplicity
	mult = line->mid( 59, 1 ).toInt();

	//parse variablility...currently not using dmag or var
	var = false; dmag = 0.0; vper = 0.0;
	if ( line->at( 62 ) == '.' ) {
		var = true;
		dmag = line->mid( 61, 4 ).toDouble();
		vper = line->mid( 66, 6 ).toDouble();
	}

	//parse name(s)
	name = line->mid( 72 ).trimmed(); //the rest of the line
	if (name.contains( ':' )) { //genetive form exists
		gname = name.mid( name.find(':')+1 ).trimmed();
		name = name.mid( 0, name.find(':') ).trimmed();
	}

	// HEV: look up star name in internationalization filesource
	name = i18n("star name", name.local8Bit().data());

	bool starIsUnnamed( false );
	if (name.isEmpty() && gname.isEmpty()) { //both names are empty
		starIsUnnamed = true;
	}
	
	dms r;
	r.setH(rah, ram, ras, ras2);
	dms d(dd, dm, ds, ds2);

	if (sgn == "-") { d.setD( -1.0*d.Degrees() ); }

	StarObject *o = new StarObject(r, d, mag, name, gname, SpType, pmra, pmdec, plx, mult, var );
	starList->append(o);
	
	// get horizontal coordinates when object will loaded while running the application
	// first run doesn't need this because updateTime() will called after loading all data
	if (reloadMode) {
		o->EquatorialToHorizontal( LST, geo()->lat() );
	}
	
	//STAR_SIZE
//	StarObject *p = new StarObject(r, d, mag, name, gname, SpType, pmra, pmdec, plx, mult, var );
//	starList.append(p);

	// add named stars to list
	if (starIsUnnamed == false) {
		data->ObjNames.append(o);
	}
}

void StarComponent::drawNameLabel(QPainter &psky, SkyObject *obj, int x, int y, double scale)
{
	QFont stdFont( psky.font() );
	QFont smallFont( stdFont );
	smallFont.setPointSize( stdFont.pointSize() - 2 );
	if ( Options::zoomFactor() < 10.*MINZOOM ) {
		psky.setFont( smallFont );
	} else {
		psky.setFont( stdFont );
	}

	((StarObject*)obj)->drawLabel( psky, x, y, Options::zoomFactor(), true, false, scale );
	// reset font
	psky.setFont( stdFont );
}
