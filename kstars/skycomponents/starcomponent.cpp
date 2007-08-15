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

#include <QPixmap>
#include <QPainter>

#include <QRectF>
#include <QFontMetricsF>

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksfilereader.h"
#include "ksutils.h"
#include "skymap.h"
#include "starobject.h"

#include "skymesh.h"
#include "skylabel.h"
#include "skylabeler.h"


StarComponent::StarComponent(SkyComponent *parent ) 
: ListComponent(parent), m_reindexNum(J2000), m_FaintMagnitude(-5.0)
{
    m_skyMesh = SkyMesh::Instance();

    m_starIndex = new StarIndex();
    for (int i = 0; i < m_skyMesh->size(); i++) {
        m_starIndex->append( new StarList() );
    }
    m_highPMStars.append( new HighPMStarList( 840.0 ) );
    m_highPMStars.append( new HighPMStarList( 304.0 ) );
    m_reindexInterval = StarObject::reindexInterval( 304.0 );

    lastFilePos = 0;
}

StarComponent::~StarComponent()
{}

bool StarComponent::selected()
{
    return Options::showStars();
}

// We use the update hook to re-index all the stars when the date has changed by
// more than 150 years.

void StarComponent::reindex( KSNumbers *num )
{
    if ( ! num ) return;

    if ( fabs( num->julianCenturies() - 
         m_reindexNum.julianCenturies() ) > m_reindexInterval ) {
        reindexAll( num );
        return;
    }

    for ( int j = 0; j < m_highPMStars.size(); j++ ) {
        m_highPMStars.at( j )->reindex( num, m_starIndex );
    }
}

void StarComponent::reindexAll( KSNumbers *num )
{

    //printf("Re-indexing Stars to year %4.1f...\n", 2000.0 + num->julianCenturies() * 100.0);

    m_reindexNum = KSNumbers( *num );
    m_skyMesh->setKSNumbers( num );

    // delete the old index 
    for ( int i = 0; i < m_starIndex->size(); i++ ) {
        delete m_starIndex->at( i );
    }
    delete m_starIndex;

    // Create a new index
    m_starIndex = new StarIndex();
    for (int i = 0; i < m_skyMesh->size(); i++) {
        m_starIndex->append( new StarList() );
    }

    // Fill it with stars from old index
    for ( int i = 0; i < objectList().size(); i++ ) {
        StarObject* star = (StarObject*) objectList()[ i ];
        Trixel trixel = m_skyMesh->indexStar( star );
        m_starIndex->at( trixel )->append( star );
    }

    // Let everyone know we have re-indexed to num
    for ( int j = 0; j < m_highPMStars.size(); j++ ) {
        m_highPMStars.at( j )->setIndexTime( num );
    }

    //printf("Done.\n");
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
	if ( ! selected() ) return;

	SkyMap *map = ks->map();
    KStarsData* data = ks->data();
    UpdateID updateID = data->updateID();

	bool checkSlewing = ( map->isSlewing() && Options::hideOnSlew() );
    bool hideLabels =  ( map->isSlewing() && Options::hideLabels() ) ||
		                ! ( Options::showStarMagnitudes() || Options::showStarNames() );

    //shortcuts to inform whether to draw different objects
	bool hideFaintStars( checkSlewing && Options::hideStars() );
	double maglim = Options::magLimitDrawStar();

    if ( ! hideFaintStars && ( m_FaintMagnitude < maglim ) ) {        // -jbb
        setFaintMagnitude( maglim );
    }

	//adjust maglimit for ZoomLevel
	double lgmin = log10(MINZOOM);
	double lgmax = log10(MAXZOOM);
	double lgz = log10(Options::zoomFactor());

	if ( lgz <= 0.75*lgmax )
        maglim -= (Options::magLimitDrawStar() - 
                   Options::magLimitDrawStarZoomOut() ) *
                   (0.75*lgmax - lgz)/(0.75*lgmax - lgmin);

	float sizeFactor = 6.0 + (lgz - lgmin);
 
	double labelMagLim = Options::magLimitDrawStarInfo();
	//labelMagLim += ( 7.9 - labelMagLim ) * ( lgz - lgmin) / (lgmax - lgmin );
	labelMagLim += ( 16.0 - labelMagLim ) * ( lgz - lgmin) / (lgmax - lgmin );
	if ( labelMagLim > 8.0 ) labelMagLim = 8.0;
	//printf("labelMagLim = %.1f\n", labelMagLim );

	//Set the brush
	QColor fillColor( Qt::white );
	if ( starColorMode() == 1 ) fillColor = Qt::red;
	if ( starColorMode() == 2 ) fillColor = Qt::black;
 	psky.setBrush( QBrush( fillColor ) );
	if ( starColorMode() > 0 )
        psky.setPen( QPen( fillColor ) );
	else
		//Reset the colors before drawing the stars.
		//Strictly speaking, we don't need to do this every time, but once per 
		//draw loop isn't too expensive.
		StarObject::updateColors( (! Options::useAntialias() || 
			map->isSlewing()), ks->data()->colorScheme()->starColorIntensity() );

    double zoom = Options::zoomFactor();

    bool drawMag = Options::showStarMagnitudes();
	bool drawName = Options::showStarNames();

	//Loop for drawing star images
    MeshIterator region(m_skyMesh, DRAW_BUF);
    while ( region.hasNext() ) {
        StarList* starList = m_starIndex->at( region.next() );
        for (int i=0; i < starList->size(); ++i) {
		    StarObject *curStar = (StarObject*) starList->at( i );

            if ( curStar->updateID != updateID ) 
                curStar->JITupdate( data );

            float mag = curStar->mag();

		    // break loop if maglim is reached
		    if ( mag > maglim || 
                 ( hideFaintStars && curStar->mag() > Options::magLimitHideStar() ) ) break;

		    if ( ! map->checkVisibility( curStar ) ) continue;
		    
		    QPointF o = map->toScreen( curStar, scale );

            if ( ! map->onScreen( o ) ) continue;
		    float size = scale * ( sizeFactor*( maglim - mag ) / maglim ) + 1.;
		    if ( size <= 0. ) continue;

			curStar->draw( psky, o.x(), o.y(), size, (starColorMode()==0), 
					       starColorIntensity(), true, scale );

            if ( hideLabels ) continue;
            if ( mag > labelMagLim ) continue;
            //if ( checkSlewing || ! (Options::showStarMagnitudes() || Options::showStarNames()) ) continue;

            float offset = scale * (6. + 0.5*( 5.0 - mag ) + 0.01*( zoom/500. ) );
			QString sName = curStar->nameLabel( drawName, drawMag );
			SkyLabeler::AddLabel( QPointF( o.x() + offset, o.y() + offset), sName, STAR_LABEL );
	    }
    }
}

 
void StarComponent::setFaintMagnitude( float newMagnitude ) {
	// only load star data if the new magnitude is fainter than we've seen so far
	if ( newMagnitude <= m_FaintMagnitude ) return;
    m_FaintMagnitude = newMagnitude;  // store new highest magnitude level
   
    //int filePos = lastFilePos;

    m_skyMesh->setKSNumbers( &m_reindexNum );
    //printf("Indexing stars for year %.1f\n",  
    //        2000.0 + m_reindexNum.julianCenturies() * 100.0 );

    float currentMag = -5.0;

	KSFileReader fileReader;
    if ( ! fileReader.open( "stars.dat" ) ) return;

    if ( lastFilePos == 0 ) {
        fileReader.setProgress( m_Data, i18n("Loading stars"), 125994, 100 );
    }

    if (lastFilePos > 0 ) fileReader.seek( lastFilePos );

    //bool first(true);
	while ( fileReader.hasMoreLines() ) {
		QString line = fileReader.readLine();

        //if ( first) { kDebug() << line << endl; first = false; }

        if ( line.isEmpty() ) continue;       // ignore blank lines
		if ( line.left(1) == "#" ) continue;  // ignore comments
			// check star magnitude
        currentMag = line.mid( 46,5 ).toFloat();
        
        StarObject* star = processStar( line );

	    objectList().append( star );
        Trixel trixel = m_skyMesh->indexStar( star );
        m_starIndex->at( trixel )->append( star );
        double pm = star->pmMagnitude();

        for (int j = 0; j < m_highPMStars.size(); j++ ) {
            HighPMStarList* list = m_highPMStars.at( j );
            if ( list->append( trixel, star, pm ) ) break;
        }

        if ( currentMag > m_FaintMagnitude ) {   // Done!
            lastFilePos = fileReader.pos();
            break; 
        }

        fileReader.showProgress();
    }

	//Options::setMagLimitDrawStar( newMagnitude );       -jbb
    //if ( lastFilePos == 0 ) emitProgressText( i18n("Loading stars done.") );

    //for (int j = 0; j < m_highPMStars.size(); j++ ) {
    //    m_highPMStars.at( j )->stats();
    //}

    //printf("star catalog reindexInterval = %6.1f years\n", 100.0 * m_reindexInterval );
}


StarObject* StarComponent::processStar( const QString &line ) {
	QString name, gname, SpType, visibleName;
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

    if ( ! name.isEmpty() ) {
        if (name.at(0) == ',') {                  // new format: "; gname; name"
		    gname = name.mid(1, 8).trimmed();
            if ( name.length() > 10) 
		        name = (name.at(9) == ',') ? name.mid(10).trimmed() : "";
            else
                name = "";
	    }
	    else {                                    // old format: "name : gname"
		    if (name.contains( ':' )) {           //genetive form exists
			    gname = name.mid( name.indexOf(':') + 1 ).trimmed();
			    name = name.mid( 0, name.indexOf(':') ).trimmed();
		    }
        }

        if ( ! gname.isEmpty() && gname.at(0) != '.') 
		    visibleName = gname;
    }

    //if ( ! gname.isEmpty() && gname.at(0) == '.') kDebug() << gname << endl;

	// HEV: look up star name in internationalization filesource
	if ( name.isEmpty() ) name = i18n("star");
	name = i18nc("star name", name.toLocal8Bit().data());

	dms r;
	r.setH(rah, ram, ras, ras2);
	dms d(dd, dm, ds, ds2);

	if ( sgn == '-' ) { d.setD( -1.0*d.Degrees() ); }

	StarObject *o = new StarObject( r, d, mag, name, visibleName, SpType, pmra, pmdec, plx, mult, var );
	o->EquatorialToHorizontal( data()->lst(), data()->geo()->lat() );

    if ( ! gname.isEmpty() ) m_genName.insert( gname, o );

	if ( ! name.isEmpty() && name != i18n("star") ) {
        objectNames(SkyObject::STAR).append( name );
    }
	if ( ! gname.isEmpty() && gname != name ) {
        objectNames(SkyObject::STAR).append( o->gname(false) );
    }
    return o;
}

SkyObject* StarComponent::findStarByGenetiveName( const QString name ) {
    return m_genName.value( name );
}

// This uses the main star index for looking up nearby stars but then
// filters out objects with the generic name "star".  We could easily
// build an index for just the named stars which would make this go
// much faster still.  -jbb
//
SkyObject* StarComponent::objectNearest(SkyPoint *p, double &maxrad )
{
	StarObject *oBest = 0;
    MeshIterator region(m_skyMesh, OBJ_NEAREST_BUF);
    while ( region.hasNext() ) {
        StarList* starList = m_starIndex->at( region.next() );
        for (int i=0; i < starList->size(); ++i) {
		    StarObject* star =  starList->at( i );
            //if ( star->name() == i18n("star") ) continue;  // prevents "star" popups --jbb
            //kDebug() << QString("found: %1\n").arg(star->name());
		    double r = star->angularDistanceTo( p ).Degrees();
		    if ( r < maxrad ) {
			    oBest = star;
			    maxrad = r;
		    }
        }
	}

	return (SkyObject*) oBest;
}


