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
#include <kglobal.h>

#include "Options.h"
#include "kstarsdata.h"
#include "ksfilereader.h"
#include "ksutils.h"
#include "skymap.h"
#include "starobject.h"

#include "skymesh.h"
#include "skylabel.h"
#include "skylabeler.h"
#include "kstarssplash.h"

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

    m_lastFilePos  = 0;
    m_lastLineNum  = 0;
    m_zoomMagLimit = 0.0;
    m_reloadSplash = m_reindexSplash = 0;
    m_validLineNums = false;

    for ( int i = 0; i <= MAX_LINENUMBER_MAG; i++ ) {
        m_labelList[ i ] = new LabelList;
    }
}

StarComponent::~StarComponent()
{}

bool StarComponent::selected()
{
    return Options::showStars();
}

void StarComponent::init(KStarsData *data)
{
    emitProgressText( i18n("Loading stars" ) );
    m_Data = data;

    readLineNumbers();
    readData( Options::magLimitDrawStar() );

    //adjust maglimit for ZoomLevel
    float maglim = Options::magLimitDrawStar();
    double lgmin = log10(MINZOOM);
    double lgmax = log10(MAXZOOM);
    double lgz = log10(Options::zoomFactor());

    if ( lgz <= 0.75*lgmax )
        maglim -= (Options::magLimitDrawStar() -
                   Options::magLimitDrawStarZoomOut() ) *
                  (0.75*lgmax - lgz)/(0.75*lgmax - lgmin);

    m_zoomMagLimit = maglim;

    StarObject::initImages();
}

// We use the update hook to re-index all the stars when the date has changed by
// more than 150 years.

void StarComponent::reindex( KSNumbers *num )
{
    if ( ! num ) return;

    // for large time steps we re-index all points
    if ( fabs( num->julianCenturies() -
               m_reindexNum.julianCenturies() ) > m_reindexInterval ) {
        reindexAll( num );
        return;
    }

    // otherwise we just re-index fast movers as needed
    for ( int j = 0; j < m_highPMStars.size(); j++ ) {
        m_highPMStars.at( j )->reindex( num, m_starIndex );
    }
}

void StarComponent::reindexAll( KSNumbers *num )
{
    if (  0 && ! m_reindexSplash ) {
        m_reindexSplash = new KStarsSplash(0,
                                           i18n("Please wait while re-indexing stars ...") );
        QObject::connect( KStarsData::Instance(),
                          SIGNAL( progressText( QString ) ),
                          m_reindexSplash, SLOT( setMessage( QString ) ) );

        m_reindexSplash->show();
        m_reindexSplash->raise();
        return;
    }

    printf("Re-indexing Stars to year %4.1f...\n",
           2000.0 + num->julianCenturies() * 100.0);

    m_reindexNum = KSNumbers( *num );
    m_skyMesh->setKSNumbers( num );

    // clear out the old index
    for ( int i = 0; i < m_starIndex->size(); i++ ) {
        m_starIndex->at( i )->clear();
    }

    // re-populate it from the objectList
    int size = objectList().size();
    for ( int i = 0; i < size; i++ ) {
        StarObject* star = (StarObject*) objectList()[ i ];
        Trixel trixel = m_skyMesh->indexStar( star );
        m_starIndex->at( trixel )->append( star );
    }

    // Let everyone else know we have re-indexed to num
    for ( int j = 0; j < m_highPMStars.size(); j++ ) {
        m_highPMStars.at( j )->setIndexTime( num );
    }

    //delete m_reindexSplash;
    //m_reindexSplash = 0;

    printf("Done.\n");
}

void StarComponent::rereadData()
{
    float magLimit =  Options::magLimitDrawStar();
    SkyMap* map = SkyMap::Instance();
    if ( ( map->isSlewing() && Options::hideOnSlew() && Options::hideStars()) ||
            m_FaintMagnitude >= magLimit )
        return;

    // bail out if there are no more lines to read
    if ( m_lastLineNum >= m_lineNumber[ MAX_LINENUMBER_MAG ] )
        return;

    m_reloadSplash = new KStarsSplash( 0,
                                       i18n("Please wait while loading faint stars ...") );

    QObject::connect( KStarsData::Instance(),
                      SIGNAL( progressText( QString ) ),
                      m_reloadSplash, SLOT( setMessage( QString ) ) );

    m_reloadSplash->show();
    m_reloadSplash->raise();
    //printf("reading data ...\n");
    readData( magLimit );
    //printf("done\n");
    delete m_reloadSplash;
    m_reloadSplash = 0;
}

void StarComponent::draw( QPainter& psky )
{
    if ( ! selected() ) return;

    SkyMap *map = SkyMap::Instance();
    KStarsData* data = KStarsData::Instance();
    UpdateID updateID = data->updateID();

    bool checkSlewing = ( map->isSlewing() && Options::hideOnSlew() );
    m_hideLabels =  ( map->isSlewing() && Options::hideLabels() ) ||
                    ! ( Options::showStarMagnitudes() || Options::showStarNames() );

    //shortcuts to inform whether to draw different objects
    bool hideFaintStars( checkSlewing && Options::hideStars() );
    double hideStarsMag = Options::magLimitHideStar();
    rereadData();

    reindex( data->updateNum() );

    //adjust maglimit for ZoomLevel
    float maglim = Options::magLimitDrawStar();
    double lgmin = log10(MINZOOM);
    double lgmax = log10(MAXZOOM);
    double lgz = log10(Options::zoomFactor());

    if ( lgz <= 0.75*lgmax )
        maglim -= (Options::magLimitDrawStar() -
                   Options::magLimitDrawStarZoomOut() ) *
                  (0.75*lgmax - lgz)/(0.75*lgmax - lgmin);

    m_zoomMagLimit = maglim;

    float sizeFactor = 10.0 + (lgz - lgmin);

    double labelMagLim = Options::starLabelDensity() / 5.0;
    labelMagLim += ( 12.0 - labelMagLim ) * ( lgz - lgmin) / (lgmax - lgmin );
    if ( labelMagLim > 8.0 ) labelMagLim = 8.0;

//REMOVE
//     //Set the brush
//     QColor fillColor( Qt::white );
//     if ( starColorMode() == 1 ) fillColor = Qt::red;
//     if ( starColorMode() == 2 ) fillColor = Qt::black;
//     psky.setBrush( QBrush( fillColor ) );
//     if ( starColorMode() > 0 )
//         psky.setPen( QPen( fillColor ) );
//     else
//        //Reset the colors before drawing the stars.
//        //Strictly speaking, we don't need to do this every time, but once per
//        //draw loop isn't too expensive.
//        StarObject::updateColors( (! Options::useAntialias() ||
//                                   map->isSlewing()), starColorIntensity() );
//END_REMOVE

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
            if ( mag > maglim || ( hideFaintStars && curStar->mag() > hideStarsMag ) )
                break;

            if ( ! map->checkVisibility( curStar ) ) continue;

            QPointF o = map->toScreen( curStar );

            if ( ! map->onScreen( o ) ) continue;
            float size = ( sizeFactor*( maglim - mag ) / maglim ) + 1.;
            if ( size <= 0. ) continue;

            curStar->draw( psky, o.x(), o.y(), size, (starColorMode()==0),
                           starColorIntensity(), true );

            if ( m_hideLabels || mag > labelMagLim ) continue;

            addLabel( o, curStar );
        }
    }
}

void StarComponent::addLabel( const QPointF& p, StarObject *star )
{
    int idx = int( star->mag() * 10.0 );
    if ( idx < 0 ) idx = 0;
    if ( idx > MAX_LINENUMBER_MAG ) idx = MAX_LINENUMBER_MAG;
    m_labelList[ idx ]->append( SkyLabel( p, star ) );
}

void StarComponent::drawLabels( QPainter& psky )
{
    if ( m_hideLabels ) return;

    psky.setPen( QColor( KStarsData::Instance()->colorScheme()->colorNamed( "SNameColor" ) ) );

    int max = int( m_zoomMagLimit * 10.0 );
    if ( max < 0 ) max = 0;
    if ( max > MAX_LINENUMBER_MAG ) max = MAX_LINENUMBER_MAG;

    for ( int i = 0; i <= max; i++ ) {
        LabelList* list = m_labelList[ i ];
        for ( int j = 0; j < list->size(); j++ ) {
            list->at(j).obj->drawNameLabel( psky, list->at(j).o );
        }
        list->clear();
    }

}

void StarComponent::readLineNumbers()
{
    KSFileReader fileReader;
    if ( ! fileReader.open( "starlnum.idx" ) ) return;

    while ( fileReader.hasMoreLines() ) {
        QString line = fileReader.readLine();
        if ( line.at(0) == '#' ) continue;  // ignore comments
        int mag = line.mid( 0, 2 ).toInt();
        int lineNumber = line.mid( 3 ).toInt();
        if ( mag < 0 || mag > MAX_LINENUMBER_MAG ) {
            fprintf(stderr, "Waring: mag %d, out of range\n", mag );
            continue;
        }
        m_lineNumber[ mag ] = lineNumber;
    }
    m_validLineNums = true;
}

int StarComponent::lineNumber( float magF )
{
    if ( ! m_validLineNums ) return -1;

    int mag = int( magF * 10.0 );
    if ( mag < 0 ) mag = 0;
    if ( mag > MAX_LINENUMBER_MAG ) mag = MAX_LINENUMBER_MAG;
    return m_lineNumber[ mag ];
}

void StarComponent::readData( float newMagnitude )
{
    // only load star data if the new magnitude is fainter than we've seen so far
    if ( newMagnitude <= m_FaintMagnitude ) return;
    float currentMag = m_FaintMagnitude;
    m_FaintMagnitude = newMagnitude;  // store new highest magnitude level

    // prepare to index stars to this date
    m_skyMesh->setKSNumbers( &m_reindexNum );

    KSFileReader fileReader;
    if ( ! fileReader.open( "stars.dat" ) ) return;

    int totalLines = lineNumber( newMagnitude );
    totalLines -= lineNumber( currentMag );
    int updates;
    if ( totalLines > 50000 )
        updates = 100;
    else if ( totalLines > 20000 )
        updates =  50;
    else if ( totalLines > 10000 )
        updates =  10;
    else
        updates =   5;

    // only show progress with valid line numbers
    if ( m_validLineNums )
        fileReader.setProgress( i18n("Loading stars"), totalLines, updates );

    if (m_lastFilePos > 0 ) fileReader.seek( m_lastFilePos );

    while ( fileReader.hasMoreLines() ) {
        QString line = fileReader.readLine();

        // DIY because QTextStream::pos() can take many seconds!
        m_lastFilePos += line.length() + 1;

        if ( line.isEmpty() ) continue;       // ignore blank lines
        if ( line.at(0) == '#' ) continue;    // ignore comments

        StarObject* star = processStar( line );
        currentMag = star->mag();

        objectList().append( star );
        Trixel trixel = m_skyMesh->indexStar( star );
        m_starIndex->at( trixel )->append( star );
        double pm = star->pmMagnitude();

        for (int j = 0; j < m_highPMStars.size(); j++ ) {
            HighPMStarList* list = m_highPMStars.at( j );
            if ( list->append( trixel, star, pm ) ) break;
        }

        fileReader.showProgress();

        if ( currentMag > m_FaintMagnitude )   // Done!
            break;
    }

    m_lastLineNum += fileReader.lineNumber();
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

    //if ( ! gname.isEmpty() && gname.at(0) == '.') kDebug() << gname;

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
            if ( star->mag() > m_zoomMagLimit ) continue;

            double r = star->angularDistanceTo( p ).Degrees();
            if ( r < maxrad ) {
                oBest = star;
                maxrad = r;
            }
        }
    }

    return (SkyObject*) oBest;
}

int StarComponent::starColorMode( void ) const {
    return m_Data->colorScheme()->starColorMode();
}

int StarComponent::starColorIntensity( void ) const {
    return m_Data->colorScheme()->starColorIntensity();
}
