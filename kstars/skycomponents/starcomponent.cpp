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
#include <QPixmap>
#include <QPainter>
#include <QFile>

#include "starcomponent.h"

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksfilereader.h"
#include "ksutils.h"
#include "skymap.h"
#include "starobject.h"

StarComponent::StarComponent(SkyComponent *parent, bool (*visibleMethod)()) 
: ListComponent(parent, visibleMethod), starFileReader(0), m_FaintMagnitude(-5.0)
{
}

StarComponent::~StarComponent()
{
}

void StarComponent::init(KStarsData *data)
{
	emitProgressText( i18n("Loading stars" ) );

	m_ColorMode = data->colorScheme()->starColorMode(); 
	m_ColorIntensity = data->colorScheme()->starColorIntensity();
	m_Data = data;

	setFaintMagnitude( Options::magLimitDrawStar() );
}

void StarComponent::draw(KStars *ks, QPainter& psky, double scale)
{
	if ( ! visible() ) return;
	
	SkyMap *map = ks->map();

	float Width = scale * map->width();
	float Height = scale * map->height();

	bool checkSlewing = ( map->isSlewing() && Options::hideOnSlew() );

//shortcuts to inform whether to draw different objects
	bool hideFaintStars( checkSlewing && Options::hideStars() );

	//adjust maglimit for ZoomLevel
	double lgmin = log10(MINZOOM);
	double lgmax = log10(MAXZOOM);
	double lgz = log10(Options::zoomFactor());

	double maglim = Options::magLimitDrawStar();
	if ( lgz <= 0.75*lgmax ) maglim -= (Options::magLimitDrawStar() - Options::magLimitDrawStarZoomOut() )*(0.75*lgmax - lgz)/(0.75*lgmax - lgmin);
	float sizeFactor = 6.0 + (lgz - lgmin);

	foreach ( SkyObject *o, objectList() ) {
		StarObject *curStar = (StarObject*)o;

		// break loop if maglim is reached
		if ( curStar->mag() > maglim || ( hideFaintStars && curStar->mag() > Options::magLimitHideStar() ) ) break;

		if ( map->checkVisibility( curStar ) )
		{
			QPointF o = map->getXY( curStar, Options::useAltAz(), Options::useRefraction(), scale );

			// draw star if currently on screen
			if (o.x() >= 0. && o.x() <= Width && o.y() >=0. && o.y() <= Height )
			{
				float size = scale * ( sizeFactor*( maglim - curStar->mag())/maglim ) + 1.;

				if ( size > 0. )
				{
					curStar->draw( psky, o.x(), o.y(), size, starColorMode(), 
							starColorIntensity(), true, scale );

					// now that we have drawn the star, we can display some extra info
					//don't label unnamed stars with the generic "star" name
					bool drawName = ( Options::showStarNames() && (curStar->name() != i18n("star") ) );
					if ( !checkSlewing && (curStar->mag() <= Options::magLimitDrawStarInfo() )
							&& ( drawName || Options::showStarMagnitudes() ) ) {

						psky.setPen( QColor( data()->colorScheme()->colorNamed( "SNameColor" ) ) );
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
	QFile file;
	QString snum, fname;
	snum.sprintf("%03d", i);
	fname = "hip" + snum + ".dat";
	if (!KSUtils::openDataFile(file, fname))
	{
		delete starFileReader;
		starFileReader = 0;
		return false;
	}
	
	delete starFileReader;
	starFileReader = new KSFileReader(file); // close file is included
	return true;
}

void StarComponent::setFaintMagnitude( float newMagnitude ) {
	// only load star data if the new magnitude is fainter than we've seen so far
	if ( newMagnitude > m_FaintMagnitude ) {
		m_FaintMagnitude = newMagnitude;  // store new highest magnitude level

		//Identify which data file needs to be opened (there are 1000 stars per file)
		int iStarFile = objectList().size()/1000 + 1;
		int iStarLine = objectList().size()%1000;
		float currentMag = -5.0;
		uint nLinesRead = 0;
		if ( objectList().size() ) 
			currentMag = objectList().last()->mag();

		//Skip 12 header lines in first file
		if ( iStarFile == 1 ) iStarLine += 12;

		//Begin reading new star data
		while ( iStarFile <= NHIPFILES && currentMag <= m_FaintMagnitude ) {
			emitProgressText( i18n( "Loading stars (%1%)", 
								int(100.*float(iStarFile)/float(NHIPFILES)) ) );

			if ( ! openStarFile( iStarFile++ ) ) {
				kDebug() << "Could not open star data file: " << iStarFile << endl;
			}

			if ( iStarLine && ! starFileReader->setLine( iStarLine ) ) {
				kDebug() << i18n( "Could not set line number %1 in star data file." , iStarLine) << endl;
			} else {
				iStarLine = 0; //start at the begnning of the next file

				while ( starFileReader->hasMoreLines() ) {
					QString line = starFileReader->readLine();
					++nLinesRead;

					if ( line.left(1) != "#" ) {  //ignore comments
						// check star magnitude
						currentMag = line.mid( 46,5 ).toFloat();
						if ( currentMag > m_FaintMagnitude ) break; //Done!

						if ( ! line.isEmpty() )
							processStar( line );
					}

//Can't process events here, because we need the stars to finish loading on startup
//					//Process events every 2500 lines read
//					if ( nLinesRead % 2500 == 0 ) kapp->processEvents();
				}
			}
		}
	}

	Options::setMagLimitDrawStar( newMagnitude );
}

void StarComponent::processStar( const QString &line ) {
	QString name, gname, SpType;
	int rah, ram, ras, ras2, dd, dm, ds, ds2;
	bool mult(false), var(false);
	QChar sgn;
	double mag, bv, dmag, vper;
	double pmra, pmdec, plx;

	//parse coordinates
	rah = line.mid( 0, 2 ).toInt();
	ram = line.mid( 2, 2 ).toInt();
	ras = int(line.mid( 4, 5 ).toDouble());
	ras2 = int(60.0*(line.mid( 4, 5 ).toDouble()-ras) + 0.5); //add 0.5 to get nearest integer with int()

	sgn = line.at(10);
	dd = line.mid(11, 2).toInt();
	dm = line.mid(13, 2).toInt();
	ds = int(line.mid(15, 4).toDouble());
	ds2 = int(60.0*(line.mid( 15, 5 ).toDouble()-ds) + 0.5); //add 0.5 to get nearest integer with int()

	//parse proper motion and parallax
	pmra = line.mid( 20, 9 ).toDouble();
	pmdec = line.mid( 29, 9 ).toDouble();
	plx = line.mid( 38, 7 ).toDouble();

	//parse magnitude, B-V color, and spectral type
	mag = line.mid( 46, 5 ).toDouble();
	bv  = line.mid( 51, 5 ).toDouble();
	SpType = line.mid(56, 2);

	//parse multiplicity
	mult = line.mid( 59, 1 ).toInt();

	//parse variablility...currently not using dmag or var
	var = false; dmag = 0.0; vper = 0.0;
	if ( line.length() > 60 && line.at( 62 ) == '.' ) {
		var = true;
		dmag = line.mid( 61, 4 ).toDouble();
		vper = line.mid( 66, 6 ).toDouble();
	}

	//parse name(s)
	if ( line.length() > 72 )
		name = line.mid( 72 ).trimmed(); //the rest of the line

	if ( ! name.isEmpty() && name.contains( ':' )) { //genetive form exists
		gname = name.mid( name.indexOf(':')+1 ).trimmed();
		name = name.mid( 0, name.indexOf(':') ).trimmed();
	}

	// HEV: look up star name in internationalization filesource
	if ( name.isEmpty() ) name = i18n("star");
	name = i18nc("star name", name.toLocal8Bit().data());

	dms r;
	r.setH(rah, ram, ras, ras2);
	dms d(dd, dm, ds, ds2);

	if ( sgn == '-' ) { d.setD( -1.0*d.Degrees() ); }

	StarObject *o = new StarObject( r, d, mag, name, gname, SpType, pmra, pmdec, plx, mult, var );
	o->EquatorialToHorizontal( data()->lst(), data()->geo()->lat() );
	objectList().append(o);

	if ( ! name.isEmpty() && name != i18n("star") ) objectNames().append( name );
	if ( ! gname.isEmpty() && gname != name ) objectNames().append( o->gname(false) );
}
