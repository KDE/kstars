/***************************************************************************
                          ksfilereader.h  -  description
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

#ifndef KSFILEREADER_H
#define KSFILEREADER_H

#include <qobject.h>
#include <qfile.h>
#include <qstring.h>
#include <qstringlist.h>

/**
  *@author Heiko Evermann
  */

class KSFileReader : public QObject  {
	Q_OBJECT
public:
	KSFileReader( QFile& file);
	~KSFileReader();

  bool hasMoreLines();
  QString& readLine();

private:
  /** After loading the whole file, we split it into lines and keep them here. */
  QStringList lines;
  /** How many lines do we have in the file? */
  int numLines;
  /** Which line are we at? */
  int curLine;
};

#endif
