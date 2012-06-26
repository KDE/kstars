/***************************************************************************
                          KSParser.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/24/06
    copyright            : (C) 2012 by Rishab Arora
    email                : ra.rishab@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSPARSER_H
#define KSPARSER_H
#include <QList>
#include <QFile>
#include <QHash>

class KSParser
{
public:
    //Constructor. Not to be called directly.
    void KSParser(QString filename, char skipChar, char delimiter);
    void CSVParser(QString filename, char skipChar, char delimiter, QList<DataTypes> pattern);
    void FixedWidthParser(QString filename, char skipChar, QList<int> widths);
    void ReadNextRow();
private:
    void ReadCSVRow();
    void ReadFixedWidthRow();
    enum DataTypes {D_QSTRING, D_INT, D_FLOAT, D_DOUBLE};
    QFile file;
    int currentRowID;
    QList<int> widths;
};

#endif // KSPARSER_H
