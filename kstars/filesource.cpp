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
#include "ksfilereader.h"
#include "kstarsdata.h"
#include "ksutils.h"

FileSource::FileSource(KStarsData *ksdata, float magnitude)
	: readingData(true), maxMagnitude(magnitude), data(ksdata) {

	kdDebug() << "new magnitude to load is " << maxMagnitude << endl;
	kdDebug() << "count()=" << data->starList.count() << endl;
	fileNumber = ksdata->starList.count() / 1000 + 1;
	lineNumber = ksdata->starList.count() % 1000;
	kdDebug() << "fileNumber=" << fileNumber << " lineNumber=" << lineNumber << endl;

	if (fileNumber <= 40) {
		data->openSAOFile(fileNumber);
		if (data->saoFileReader->setLine(lineNumber) == true)
			kdDebug() << "line reset ok" << endl;
		else
			kdDebug() << "line reset not ok" << endl;
	}
}

FileSource::~FileSource() {
}

int FileSource::readyToSend() {
	// readyToSend ==  0 -> no data but not end of stream
	// readyToSend  >  0 -> data ready to send
	// readyToSend == -1 -> end of stream, QDataPump will destroy this FileSource object
	if (readingData == true)
		return 1;
	else
		return -1;
}

void FileSource::sendTo(QDataSink *sink, int) {
	counter = 0;
	while (data->saoFileReader->hasMoreLines() && counter < maxLines) {
		QString line = data->saoFileReader->readLine();
		float mag = line.mid(33, 4).toFloat();  // check magnitude
		if (mag > maxMagnitude) {
			readingData = false;
		} else {
			stringArray[counter++] = line;
		}
	}
	// open next file if end is reached
	if (data->saoFileReader->hasMoreLines() == false && readingData == true && fileNumber < 41) {
		kdDebug() << "sendTo: open file #" << fileNumber << endl;
		data->openSAOFile(fileNumber++);
	}
	// send data to StarDataSink
	sink->receive((uchar*) &stringArray[0], counter);
}
