/***************************************************************************
                          stardatasink.h  -  description
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

#ifndef STARDATASINK_H
#define STARDATASINK_H


/**
  *StarDataSink receives data from an FileSource object and appends these data
  *to a QList of star data. It's an asynchronous io class.
  *@author Thomas Kabelmann
  */

#include <qobject.h>
#include <qasyncio.h>
#include <qtextstream.h>
#include <qptrlist.h>

#include "starobject.h"

class KStarsData;

class StarDataSink : public QObject, public QDataSink {
/**
	*class needs signals
	*/
	Q_OBJECT

	public:
	/** constructor	*/
		StarDataSink( KStarsData *parent, const char *name=0 );

	/** destructor */
		~StarDataSink();

	/** is this object ready to receive data? */
		int readyToReceive();

	/** end of data transmission reached */
		void eof();

	/**
		*This function receives data from FileSource and appends data
		*to some lists in KStarsData.
		*/
		void receive( const uchar *data, int entries );

	private:

		QTextStream stream;

		KStarsData *ksData;
      	
		float lastMagnitude;
		
		int magLevel;

		bool nameListChanged;
      	
	signals:

	/**
		*send signal if all data were transmitted
		*/
		void done();
		
	/**
		*send signal to update skymap time by time
		*just for long data transmissions needed.
		*/
		void updateSkymap();

	/**
		*If name list has changed emit this signal.
		*/
		void clearCache();
};

#endif
