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

#include "dms.h"
#include "kstarsdata.h"

StarDataSink::StarDataSink(KStarsData *parent, const char *name) : QObject(parent, name) {
	ksData = parent;
	receivedBlocks = 0;
	nameListCount = ksData->ObjNames.count();
}

StarDataSink::~StarDataSink(){
}

int StarDataSink::readyToReceive(){
	// return a value != NULL, to show that the object is ready for receiveing data.
	return 1;
}

void StarDataSink::eof() {
	// update skymap at the end of reloading
	emit updateSkymap();
	// clear cached finddialog if new objects were appended to the list
	if (nameListCount != ksData->ObjNames.count()) emit clearCache();
	// end of data transmission
	emit done();
}

void StarDataSink::receive( const uchar *data, int entries ) {
	receivedBlocks++;
	// Pointer was send as const uchar* so it must be converted explicitly to QString*
	QString *line = (QString *) data;
	int counter = -1;
	while (++counter < entries) {  // run counter from 0 to entries -1
		ksData->processStar(line, true);
		line++;
	}

	// update the skymap every tenth block
	if (receivedBlocks % 10 == 0) emit updateSkymap();
}

#include "stardatasink.moc"
