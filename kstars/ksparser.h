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
#include <QDebug>
#include <kstandarddirs.h>
#include <QVariant>
#include "ksfilereader.h"

class KSParser
{
public:
    enum DataTypes {D_QSTRING, D_INT, D_FLOAT, D_DOUBLE};
    //Constructor to return a CSV Parser
    KSParser(QString filename, char skipChar, char delimiter, QList<DataTypes> pattern, QList<QString> names) __attribute__((cdecl));
    //Constructor to return a Fixed Width Parser
    KSParser(QString filename, char skipChar, QList<int> widths) __attribute__((cdecl));
    QHash<QString,QVariant>  ReadNextRow();
    bool hasNextRow();
private:
    //Function Pointer
    QHash<QString,QVariant> (KSParser::*readFunctionPtr)();
    
    QHash<QString,QVariant>  ReadCSVRow();
    QHash<QString,QVariant>  ReadFixedWidthRow();
    QHash<QString,QVariant>  DummyCSVRow();
    
    KSFileReader m_FileReader;
    QString m_Filename;
    int m_CurrentRowID;
    bool moreRows;
    char skipChar;
    
    QList<DataTypes> pattern;
    QList<QString> names;
    QList<int> widths;
    
};

#endif // KSPARSER_H
