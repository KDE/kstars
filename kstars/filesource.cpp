/***************************************************************************
                          filesource.cpp  -  description
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

#include "filesource.h"


#include "kstarsdata.h"
#include "ksutils.h"

FileSource::FileSource( KStarsData *parent, const char *file, float magnitude )
	: readingData( true ), currentBlockSize( 4096 ), inputFile( 0 ), maxMagnitude( magnitude ), index( parent->lastFileIndex )
{
	if ( file ) {  // if file != NULL
		inputFile = new QFile();  // create new file

		if ( inputFile ) {  // check if new file is allocated
				if ( !KSUtils::openDataFile( *inputFile, file ) ) {
					qDebug( "FileSource::FileSource : open file failed" );  // if failed
			}
			else {  // file is opened
				if ( inputFile->at( index ) ) {  // seek to last position in data file
					stream.setDevice( inputFile );  // set device for stream
					if ( !stream.device() ) qDebug( "FileSource::FileSource : no device set" );
				}
				else
					qDebug( "FileSource::FileSource : could not set fileindex" );
			}

		}
		else {
			qDebug( "FileSource::FileSource : file name is NULL" );  // file == NULL
			readingData = false;  // no data to read and send
		}
	}  // end of if ( file )
}

FileSource::~FileSource() {
// delete file variable if exists
	if ( inputFile ) delete inputFile;
}

int FileSource::readyToSend() {
/**
	*readyToSend ==  0 -> no data but not end of stream
	*readyToSend  >  0 -> data ready to send
	*readyToSend == -1 -> end of stream, QDataPump will destroy this FileSource object
	*/
	return readingData ? currentBlockSize : -1;
}

void FileSource::sendTo( QDataSink *sink, int ) {
	int counter = 0;
	float mag(0.0);
	while ( !stream.atEnd() && readingData ) {  // read stream until eof or readingData == false
		stringArray[ counter ] = stream.readLine();  // read 1 line into array
		mag = stringArray[ counter ].mid( 33, 4 ).toFloat();  // check magnitude
		if ( mag > maxMagnitude || counter == ( maxLines - 1 ) ) {
			if ( !( mag > maxMagnitude ) ) counter++;  // break of while due to counter == ( maxLines - 1)
			readingData = false;
		}
		else {
			counter++;  // count entries in array
			index = inputFile->at();  // store file index to KStarsData::lastFileIndex
		}
	}
	// send data to StarDataSink
	sink->receive( ( uchar* ) &stringArray[0], counter );
	
	if ( !( mag > maxMagnitude ) && counter )
		readingData = true;  // if just a block was read set true and load again
	else
		readingData = false; // end of block is reached
}
