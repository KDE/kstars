/***************************************************************************
                          ksfilereader.cpp  -  description
                             -------------------
    begin                : Tue Jan 28 2003
    copyright            : (C) 2003 by Heiko Evermann
    email                : heiko@evermann.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qfile.h>
#include <qstring.h>

#include "ksfilereader.h"

KSFileReader::KSFileReader(QFile& file) {
	// read the whole file at once. This works well at least for the smaller files.
	QByteArray data = file.readAll();
	QString sAll = QString::fromUtf8( data.data(), data.size() );
	// split into list of lines
	lines = QStringList::split( "\n", sAll );
	// how many lines did we get?
	numLines = lines.size();
	// set index to start
	curLine = 0;
	// we do not need the file any more
	file.close();
}

KSFileReader::~KSFileReader(){
}

bool KSFileReader::hasMoreLines() {
	return (curLine < numLines);
}

QString& KSFileReader::readLine(){
	// hint: use curLine as index, after that increment curLine
	// This means that the programming language c++ should better be renamed to ++c,
	// otherwise the name means: improve the c programming language, but use it the
	// way it was before the improvements...
	return lines[curLine++];
}

bool KSFileReader::setLine(int i) {
	if (i <= numLines) {
		curLine = i;
		return true;
	} else {
		return false;
	}
}

#include "ksfilereader.moc"
