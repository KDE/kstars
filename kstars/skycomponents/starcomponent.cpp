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

#include "binfilehelper.h"

StarComponent::StarComponent(SkyComponent *parent )
    : ListComponent(parent), m_reindexNum(J2000), m_FaintMagnitude(-5.0), starsLoaded(false)
{
    m_skyMesh = SkyMesh::Instance();

    m_starIndex = new StarIndex();
    for (int i = 0; i < m_skyMesh->size(); i++) {
        m_starIndex->append( new StarList() );
        m_readOffset[i] =  0;
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

    //    readLineNumbers();
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

//This function is empty for a reason; we override the normal 
//update function in favor of JiT updates for stars.
void StarComponent::update( KStarsData *data, KSNumbers *num )   
{   
    Q_UNUSED(data)   
    Q_UNUSED(num)   
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
    //    if ( m_lastLineNum >= m_lineNumber[ MAX_LINENUMBER_MAG ] )
    //    return;
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

// TODO: Come up with a way of implementing quick magnitude searches with binary data
/*
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
*/

int StarComponent::lineNumber( float magF )
{
    if ( ! m_validLineNums ) return -1;

    int mag = int( magF * 10.0 );
    if ( mag < 0 ) mag = 0;
    if ( mag > MAX_LINENUMBER_MAG ) mag = MAX_LINENUMBER_MAG;
    return m_lineNumber[ mag ];
}

// TODO: Give this method a better name
void StarComponent::readData( float newMagnitude ) 
{
    // TODO: We may want to remove the newMagnitude argument. Currently, it is deprecated

    // We break from Qt / KDE API and use traditional file handling here, to obtain speed.
    // We also avoid C++ constructors for the same reason.
    FILE *dataFile, *nameFile;
    bool swapBytes = false;
    BinFileHelper dataReader, nameReader;
    unsigned long nrecords;
    QString name, gname, visibleName;
    StarObject *star;

    kDebug() << "Entered readData()";

    // We now load all stars the first time this method is called and load nothing during subsequent calls:

    if(starsLoaded)
        return;

    // prepare to index stars to this date
    m_skyMesh->setKSNumbers( &m_reindexNum );
        
        
    /* Open the data files */
    if((dataFile = dataReader.openFile("shallowstars.dat")) == NULL) {
        kDebug() << "Could not open data file shallowstars.dat" << endl;
        return;
    }

    if(!(nameFile = nameReader.openFile("starnames.dat"))) {
        kDebug() << "Could not open data file starnames.dat" << endl;
        return;
    }

    if(!dataReader.readHeader()) {
        kDebug() << "Error reading shallowstars.dat header : " << dataReader.getErrorNumber() << " : " << dataReader.getError() << endl;
        return;
    }

    if(!nameReader.readHeader()) {
        kDebug() << "Error reading starnames.dat header : " << nameReader.getErrorNumber() << " : " << nameReader.getError() << endl;
        return;
    }

    // TODO: Implement a solution to load nameFile completely into memory, to avoid disk seeks
    fseek(nameFile, nameReader.getDataOffset(), SEEK_SET);

    long int nstars = 0;
    Trixel expectedTrixelId = -1;
    QTime t;

    /* TODO : Remove timing code when we are done with all possible optimizations */
    t.start();

    /* Start reading the data file */
    fseek(dataFile, dataReader.getDataOffset(), SEEK_SET);

    /* Recurse over trixels */
    for(int i = 0; i < m_skyMesh -> size(); ++i) {

        // The following code is not required, because we are anyway reading the file sequentially, and hence is commented out.
        /*      
                if(fseek(dataFile, dataReader.getOffset(i), SEEK_SET))
                kDebug() << "ERROR: Could not seek to offset " << dataReader.getOffset(i) << " to find trixel #" << i << endl;
        */

        /* Recurse over stars in each trixel */
        for(unsigned long j = 0; j < (unsigned long)dataReader.getRecordCount(i); ++j) {

            /* Read star data */
            if(!fread(&stardata, sizeof(starData), 1, dataFile)){
                kDebug() << "ERROR: Could not read starData structure for star #" << j << " under trixel #" << i << endl;
            }
            
            // TODO: Add byteswapping code
            
            gname = "";
            name = "";
            visibleName = "";

            bool named = false;
            
            if(stardata.flags & 0x01) {
                /* Named Star - Read the nameFile */
                if(!fread(&starname, sizeof(starName), 1, nameFile))
                    kDebug() << "ERROR: fread() call on nameFile failed in trixel " << i << " star " << j << endl;
                name = QByteArray(starname.longName, 32);
                gname = QByteArray(starname.bayerName, 8);
                if ( ! gname.isEmpty() && gname.at(0) != '.')
                    visibleName = gname;
                if(! name.isEmpty() ) {
                    // HEV: look up star name in internationalization filesource
                    name = i18nc("star name", name.toLocal8Bit().data());
                }
                else
                    name = i18n("star");
                named = true;
            }

            /* Create the new StarObject */
            if ( named ) {
                star = new StarObject( stardata.RA/1000000.0, stardata.Dec/100000.0, stardata.mag/100.0, name, visibleName, 
                                       QByteArray(stardata.spec_type, 2), stardata.dRA/10.0, stardata.dDec/10.0, 
                                       stardata.parallax/10.0, stardata.flags & 0x02, stardata.flags & 0x04 );
            }
            else {
                /*
                 * CAUTION: We avoid trying to construct StarObjects using the constructors [The C++ way]
                 *          in order to gain speed. Instead, one template StarObject is constructed and
                 *          all other unnamed stars are created by doing a raw copy from this using memcpy()
                 *          and then calling StarObject::init() to replace the default data with the correct
                 *          data.
                 *          This means that this section of the code plays around with pointers to a great
                 *          extend and has a chance of breaking down / causing segfaults.
                 */
                    
                /* Make a copy of the star template and set up the data in it */
                star = (StarObject *)malloc(sizeof(StarObject));
                star = (StarObject *)memcpy(star, &plainStarTemplate, sizeof(StarObject));
                star -> init(stardata.RA/1000000.0, stardata.Dec/100000.0, stardata.mag/100.0,
                             QByteArray(stardata.spec_type, 2), stardata.dRA/10.0, stardata.dDec/10.0,
                             stardata.parallax/10.0, stardata.flags & 0x02, stardata.flags & 0x04);
            }
            star->EquatorialToHorizontal( data()->lst(), data()->geo()->lat() );
            ++nstars;
            
            if( named ) {
                if ( ! gname.isEmpty() ) m_genName.insert( gname, star );
                
                if ( ! name.isEmpty() ) {
                    objectNames(SkyObject::STAR).append( name );
                }
                if ( ! gname.isEmpty() && gname != name ) {
                    objectNames(SkyObject::STAR).append( star -> gname(false) );
                }
                
            }

            objectList().append( star );
            
            Trixel trixel = ((i >= 256) ? (i - 256):(i + 256));
            m_starIndex->at( trixel )->append( star );
            double pm = star->pmMagnitude();
            for (int j = 0; j < m_highPMStars.size(); j++ ) {
                HighPMStarList* list = m_highPMStars.at( j );
                if ( list->append( 512 - i, star, pm ) ) break;
            }
        }
    }
    dataReader.closeFile();
    nameReader.closeFile();
    kDebug() << "Loaded " << nstars << " stars in " << t.elapsed() << " ms" << endl;

    // TODO: Fix things:
    //   Suppose the user wants stars down to 10th mag when zoomed out, this thing lies
    //   We need to deprecate that option as well
    starsLoaded = true;
    if(m_FaintMagnitude < 8.0) 
        m_FaintMagnitude = 8.0;

}


bool StarComponent::readStarBlock(StarBlock *SB, FILE *dataFile, int nstars) {

    int i;
    starData stardata;
    QString name, gname, visibleName;


    if( SB == NULL || dataFile == NULL )
        return false;

    // Allocate the required amount of StarObjects
    if( SB ->  star )
        free( SB -> star );

    SB -> star = ( StarObject * )malloc( sizeof( StarObject ) * nstars );
    if( !SB -> star )
        return false;

    // Read in the data from dataFile and fill into the StarBlock
    for( i = 0; i < nstars; ++i ) {

        if( !fread( &stardata, sizeof( starData ), 1, dataFile ) ) {
            kDebug() << "ERROR: Premature termination of dataFile in readStarBlock. Expected "
                     << nstars << " records, but read " << i << endl;
            SB -> star = ( StarObject * )realloc( SB -> star, sizeof( StarObject ) * i );
            return false;
        }


        // TODO: Add byteswapping code
        
        if(stardata.flags & 0x01) {
            kDebug() << "WARNING: Named Star encountered while reading StarBlock. Name will not be loaded!";
            continue;
        }

        memcpy( &(SB -> star[i]), &plainStarTemplate, sizeof( StarObject ) );
        SB -> star[i].init( stardata.RA/1000000.0, stardata.Dec/100000.0, stardata.mag/100.0,
                            QByteArray( stardata.spec_type, 2 ), stardata.dRA/10.0, stardata.dDec/10.0,
                            stardata.parallax/10.0, stardata.flags & 0x02, stardata.flags & 0x04 );
    }

    return true;
}

SkyObject* StarComponent::findStarByGenetiveName( const QString name ) {
    return m_genName.value( name );
}

// Overrides ListComponent::findByName() to include genetive name also in the search
SkyObject* StarComponent::findByName( const QString &name ) {
    foreach ( SkyObject *o, objectList() )
    if ( QString::compare( o->name(), name, Qt::CaseInsensitive ) == 0 || 
        QString::compare( o->longname(), name, Qt::CaseInsensitive ) == 0 || 
         QString::compare( o->name2(), name, Qt::CaseInsensitive ) == 0 || 
         QString::compare( ((StarObject *)o)->gname(false), name, Qt::CaseInsensitive ) == 0)
        return o;

    //No object found
    return 0;
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
