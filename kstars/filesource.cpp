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
	: maxMagnitude(magnitude), data(ksdata) {

//	kdDebug() << "new magnitude to load is " << maxMagnitude << endl;
//	kdDebug() << "count()=" << data->starList.count() << endl;
	fileNumber = ksdata->starList.count() / 1000 + 1;
	lineNumber = ksdata->starList.count() % 1000;
	// the first file contains 12 comment lines at the beginning which must skipped
	if (fileNumber == 1) { lineNumber += 12; }
//	kdDebug() << "fileNumber=" << fileNumber << " lineNumber=" << lineNumber << endl;

	if (fileNumber <= NHIPFILES) {
		// if file opened it's true else false
		readingData = data->openStarFile(fileNumber);
		if (data->starFileReader->setLine(lineNumber) == true) {
//			kdDebug() << "line reset ok" << endl;
		} else {
//			kdDebug() << "line reset not ok" << endl;
		}
	} else {
		readingData = false;
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
	while (data->starFileReader->hasMoreLines() && counter < maxLines) {
		QString line = data->starFileReader->readLine();
		float mag = line.mid( 46, 5 ).toFloat();  // check magnitude
//		kdDebug() << "mag=" << mag << " maxmag=" << maxMagnitude << endl;
		if (mag > maxMagnitude) {
			readingData = false;
			break;
		} else {
			stringArray[counter++] = line;
		}
	}
	// open next file if end is reached
	if (data->starFileReader->hasMoreLines() == false && readingData == true && fileNumber < NHIPFILES) {
		fileNumber++;
//		kdDebug() << "sendTo: open file #" << fileNumber << endl;
		data->openStarFile(fileNumber);
	}
	// check if more data are available
	if (readingData == true && data->starFileReader != 0 && data->starFileReader->hasMoreLines() == true) {
		readingData = true;
	} else {
		readingData = false;
	}
	// send data to StarDataSink
	sink->receive((uchar*) &stringArray[0], counter);
}
