/***************************************************************************
                          filesource.h  -  description
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

#ifndef FILESOURCE_H
#define FILESOURCE_H


/**
  *FileSource is a asynchronous class for reloading star data while running
  *the program. It's basing on QDataSource class and implements the function
  *for reading a file step by step and send these data to an QDataSink object.
  *KStarsData uses this class for asynchronous io.
  *@author Thomas Kabelmann
  */


#include <qasyncio.h>
#include <qtextstream.h>
#include <qstring.h>

class KStarsData;
class QFile;

class FileSource : public QDataSource  {

	public:
	/**
		*constructor needs an KStarsData object, a file name and the new magnitude
		*/
		FileSource( KStarsData *parent, const char *file, float magnitude );

	/** destructor */
		~FileSource();
  	
	/**
		*send a value if ready for sending data
		*/
		int readyToSend();

	/**
		*Is this object rewindable?
		*Returs false, because it's not needed to rewind.
		*/
		bool rewindable() { return false; }
		
	/**
		*The function for sending data to an QDataSink object. Here will all data
		*operations defined.
		*/
		void sendTo( QDataSink *sink, int count );

	/**
		*returns current magnitude to load
		*/
		float magnitude() { return maxMagnitude; }
		
	private:

		bool readingData;

		int currentBlockSize;

		QFile *inputFile;

		QTextStream stream;

	/**
		*new magnitude to load
		*/
		float maxMagnitude;
		
	/**
		*Changes KStarsData::lastFileIndex to current position in data file.
		*/
		int &index;

	/**
		*maxLines defines how many lines in data file should be read and
		*send to QDataSink. This is only needed if a data block is longer
		*than the max defined lines. I figured out this value of 500
		*on a fast system, so if it is to high the value might be decreased.
		*A high value means faster reloading but perhaps on slow systems this
		*might interrupt the main event loop.
		*A low value needs longer to reload data, but it doesn't interrupt the
		*main event loop (it's smoother).
		*But it's important to know that 500 lines to read is very fast, but
		*appending to QList in StarDataSink will take the most time and this
		*will also defined with this value.
		*/
		#define maxLines 500
	/**
		*The loaded data will stored in a string array and a pointer to first
		*object in array will send to StarDataSink.
		*/
		QString stringArray[ maxLines ];

};

#endif
