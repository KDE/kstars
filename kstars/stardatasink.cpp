/***************************************************************************
                          stardatasink.cpp  -  description
                             -------------------
    begin                : Son Feb 10 2002
    copyright            : (C) 2002 by Thomas Kabelmann
    email                : tk78@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "stardatasink.h"
#include "stardatasink.moc"

#include "dms.h"
#include "kstars.h"

#include <kapplication.h>

StarDataSink::StarDataSink( KStarsData *parent, const char *name )
	: QObject( parent, name ), ksData( parent ), lastMagnitude( 0.0 ), magLevel( 0 ),
		nameListChanged( false )
{
}

StarDataSink::~StarDataSink(){
}

int StarDataSink::readyToReceive() {
/**
	*Just return a value != NULL so this object is everytime ready for
	*receiveing data.
	*/
	return 4096;
}

void StarDataSink::eof() {
// send clearCache signal if necessary	
	if ( nameListChanged ) emit clearCache();
// end of data transmission so send signal done()
	emit done();
}

void StarDataSink::receive( const uchar *data, int entries ) {
/**
	*Pointer was send as const uchar* so it must explicite converted to QString*
	*/
	QString *line = (QString * ) data;
	int counter = -1;
	float mag(0.0);  // needed also out of while loop
	while ( ++counter < entries ) {  // run counter from 0 to entries -1

	/**
		*This part is almost identical with KStarsData::readStarData(). Just some
		*pointers are changed.
		*/
		QString name, gname, SpType;
		int rah, ram, ras, dd, dm, ds;
		QChar sgn;

		name = "star";
		rah = line->mid( 0, 2 ).toInt();
		ram = line->mid( 2, 2 ).toInt();
		ras = int( line->mid( 4, 6 ).toDouble() + 0.5 ); //add 0.5 to make int() pick nearest integer

		sgn = line->at( 17 );
		dd = line->mid( 18, 2 ).toInt();
		dm = line->mid( 20, 2 ).toInt();
		ds = int( line->mid( 22, 5 ).toDouble() + 0.5 ); //add 0.5 to make int() pick nearest integer

		mag = line->mid( 33, 4 ).toFloat();	// just check star magnitude

		SpType = line->mid( 37, 2 );
		name = line->mid( 40 ).stripWhiteSpace(); //the rest of the line
		if ( name.contains( ':' ) ) { //genetive form exists
			gname = name.mid( name.find(':')+1 );
			name = name.mid( 0, name.find(':') ).stripWhiteSpace();
		}
		if ( name.isEmpty() ) name = "star";

       dms r;
		r.setH( rah, ram, ras );
		dms d( dd, dm,  ds );

		if ( sgn == "-" ) d.setD( -1.0*d.Degrees() );

		//REVERTED...remove comments after 1/1/2003
		//ARRAY:
		StarObject *o = new StarObject( r, d, mag, name, gname, SpType );
		ksData->starList.append( o );
		//ksData->starArray[ksData->StarCount++] = StarObject( r, d, mag, name, gname, SpType );
		//StarObject *o = &(ksData->starArray[ksData->StarCount-1]);
		if ( o->name() != "star" ) {		// just add to name list if a name is given
			ksData->ObjNames.append ( o );
			nameListChanged = true;
		}

		// recompute coordinates if AltAz is used
		o->EquatorialToHorizontal( ksData->LSTh, ksData->kstars->geo()->lat() );

		line++; // go to next array field
	}  // end of while
	
/**
	*Count magLevel just if a new magnitude level is reached and magnitude level
	*is higher than 4.0. It's just needed to send an update signal on defined values.
	*/
	if ( mag > lastMagnitude && ( mag > 4.0 ) ) magLevel++;
	
/**
	*Send an update signal to skymap to refresh skymap with new reloaded data. I have decided
	*to refresh only if magnitude level is between 4.0 and 6.8 and only every 5'th magnitude level.
	*Values higher than 6.8 the user will not see a hugh different and lower than 4.0 the data
	*will reloaded in 2 steps so refreshing is not needed while reloading.
	*/
	if ( magLevel % 5 == 0 && mag >= 4.0 && mag < 6.8 ) {
		emit updateSkymap();
	}
	lastMagnitude = mag;  // store last magnitude level
}

