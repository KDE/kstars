/***************************************************************************
                          KSParser.cpp  -  K Desktop Planetarium
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

#include "ksparser.h"
#include <kdebug.h>
#include <klocale.h>



KSParser::KSParser(QString filename, char skipChar, char delimiter, QList<DataTypes> pattern, QList<QString> names){
    if ( ! fileReader.open(filename) ) {
        kWarning() <<"Unable to open file!";
        moreRows = false;
        readFunctionPtr = &KSParser::DummyCSVRow;
        return;
    }
    
    readFunctionPtr = &KSParser::ReadCSVRow;

}

KSParser::KSParser(QString filename, char skipChar, QList<int> widths) {
    
        readFunctionPtr = &KSParser::ReadFixedWidthRow;

}

QHash<QString,QVariant>  KSParser::ReadNextRow() {
    return (*this.*readFunctionPtr)();
}


QHash<QString,QVariant>  KSParser::ReadCSVRow() {
    kWarning() <<"READ CSV";
    QString line;
    bool success=false;
    
    while (fileReader.hasMoreLines() && success==false){
        line = fileReader.readLine();
        //TODO: manage " marks
        line.split(",");
    }
    if (fileReader.hasMoreLines() == false)
        return DummyCSVRow();
    QHash<QString,QVariant> newRow;
    return newRow;
}

QHash<QString,QVariant>  KSParser::ReadFixedWidthRow() {
    kWarning() <<"READ FWR";
    QHash<QString,QVariant> newRow;
    return newRow;
}

QHash<QString,QVariant>  KSParser::DummyCSVRow() {
    //TODO: allot 0 or "null" to every position
    kWarning() <<"READ FWR";
    QHash<QString,QVariant> newRow;
    return newRow;
}

bool KSParser::hasNextRow() {
    return moreRows;
}

