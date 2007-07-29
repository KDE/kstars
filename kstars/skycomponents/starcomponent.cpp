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
: ListComponent(parent), m_FaintMagnitude(-5.0)
{
    m_skyMesh = ((SkyMapComposite*) parent)->skyMesh();
    m_skyLabeler = ((SkyMapComposite*) parent)->skyLabeler();

    for (int i = 0; i < m_skyMesh->size(); i++) {
        m_starIndex.append( new StarList() );
    }

    lastFilePos = 0;
}

StarComponent::~StarComponent()
{}

bool StarComponent::selected()
{
    return Options::showStars();
}

void StarComponent::update( KStarsData *data, KSNumbers *num )
{}

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
    UpdateID updateNumID = data->updateNumID();

    QList<SkyLabel> labelList;
    double labelMagLim = Options::magLimitDrawStarInfo();

	bool checkSlewing = ( map->isSlewing() && Options::hideOnSlew() );
    bool noLabels =  checkSlewing || ! ( Options::showStarMagnitudes() || Options::showStarNames() );

    //shortcuts to inform whether to draw different objects
	bool hideFaintStars( checkSlewing && Options::hideStars() );

	//adjust maglimit for ZoomLevel
	double lgmin = log10(MINZOOM);
	double lgmax = log10(MAXZOOM);
	double lgz = log10(Options::zoomFactor());

	double maglim = Options::magLimitDrawStar();
	if ( lgz <= 0.75*lgmax )
        maglim -= (Options::magLimitDrawStar() - 
                   Options::magLimitDrawStarZoomOut() ) *
                   (0.75*lgmax - lgz)/(0.75*lgmax - lgmin);

	float sizeFactor = 6.0 + (lgz - lgmin);

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

	//Loop for drawing star images
    MeshIterator region(m_skyMesh, DRAW_BUF);
    while ( region.hasNext() ) {
        StarList* starList = m_starIndex[ region.next() ];
        for (int i=0; i < starList->size(); ++i) {
		    StarObject *curStar = (StarObject*) starList->at( i );

            if ( curStar->updateID != updateID ) {
                curStar->updateID = updateID;
                if ( curStar->updateNumID != updateNumID ) {
                    curStar->updateCoords( data->updateNum() );
                }
                curStar->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
            }

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

            if ( noLabels ) continue;
            if ( mag > labelMagLim ) continue;
            if ( curStar->name() == i18n("star") ) continue;

            // NOTE: the code below was copied here from StarObject.  Perhaps
            // there is a cleaner way. -jbb
            float offset = scale * (6. + 0.5*( 5.0 - mag ) + 0.01*( zoom/500. ) );

            bool drawName = ( Options::showStarNames() && curStar->name() != i18n("star") );

            QString sName( i18n("star") + ' ' );
            if ( drawName ) {
                if ( curStar->translatedName() != i18n("star") && ! curStar->translatedName().isEmpty() )
                    sName = curStar->translatedName() + ' ';
                else if ( ! curStar->gname().trimmed().isEmpty() ) sName = curStar->gname( true ) + ' ';
            }
            if ( drawMag ) {
                if ( drawName )
                    sName += QString().sprintf("%.1f", curStar->mag() );
                else
                    sName.sprintf("%.1f", curStar->mag() );
            }
    
		    labelList.append( SkyLabel( o.x() + offset, o.y() + offset, sName) );
	    }
    }

	//Loop for drawing star labels
	if ( checkSlewing || ! (Options::showStarMagnitudes() || Options::showStarNames()) ) return;

	maglim = Options::magLimitDrawStarInfo();
	psky.setPen( QColor( data->colorScheme()->colorNamed( "SNameColor" ) ) );
	QFont stdFont( psky.font() );
	QFont smallFont( stdFont );
	smallFont.setPointSize( stdFont.pointSize() - 2 );
	if ( Options::zoomFactor() < 10.*MINZOOM ) {
		psky.setFont( smallFont );
	} else {
		psky.setFont( stdFont );
	}

    m_skyLabeler->setFont( psky.font() );

    // -jbb: please bear with me.  This is a work in progress to trouble shoot
    // the idea of keeping labels from overlapping ...

    for (int i =0; i < labelList.size(); i++) {
        SkyLabel label = labelList[ i ];
        //kDebug() << starLabel.star->name() << endl;
       
        QPointF o = label.point();
        if ( ! m_skyLabeler->mark( o, label.text ) ) continue; 

        if ( Options::useAntialias() )  {
            psky.drawText( o, label.text );
         }
        else
            psky.drawText( QPoint( int(o.x()), int(o.y()) ), label.text );
    
    }

	psky.setFont( stdFont );
}

 
void StarComponent::setFaintMagnitude( float newMagnitude ) {
	// only load star data if the new magnitude is fainter than we've seen so far
	if ( newMagnitude <= m_FaintMagnitude ) return;
    m_FaintMagnitude = newMagnitude;  // store new highest magnitude level
   
    //int filePos = lastFilePos;

    float currentMag = -5.0;

	KSFileReader fileReader;
    if ( ! fileReader.open( "stars.dat" ) ) return;

    if (lastFilePos == 0 ) {
        fileReader.setProgress( m_Data, QString("Loading stars (%1%)"), 
                                125994, 100);
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
        
        processStar( line );

        if ( currentMag > m_FaintMagnitude ) {   // Done!
            lastFilePos = fileReader.pos();
            break; 
        }

        fileReader.showProgress();
    }

	Options::setMagLimitDrawStar( newMagnitude );
}


void StarComponent::processStar( const QString &line ) {
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
	objectList().append(o);

    int starID = m_skyMesh->index( (SkyPoint*) o );
    m_starIndex[starID]->append( o );

    if ( ! gname.isEmpty() ) m_genName.insert( gname, o );

	if ( ! name.isEmpty() && name != i18n("star") ) {
        objectNames(SkyObject::STAR).append( name );
    }
	if ( ! gname.isEmpty() && gname != name ) {
        objectNames(SkyObject::STAR).append( o->gname(false) );
    }
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
        StarList* starList = m_starIndex[ region.next() ];
        for (int i=0; i < starList->size(); ++i) {
		    StarObject* star =  starList->at( i );
            if ( star->name() == i18n("star") ) continue;  // prevents "star" popups --jbb
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


