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



KSParser::KSParser(QString filename, char skipChar, char delimiter, QList<DataTypes> pattern, QList<QString> names):filename(filename),pattern(pattern),names(names){
    if ( ! fileReader.open(filename) || pattern.length() != names.length()) {
        kWarning() <<"Unable to open file!";
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
    QStringList separated;
    bool success=false;
    QHash<QString,QVariant> newRow;
    
    //This signifies that someone tried to read a row
    //without checking if hasNextRow is true
    if (fileReader.hasMoreLines() == false)
        return DummyCSVRow(); 
    
    while (fileReader.hasMoreLines() && success==false){
        line = fileReader.readLine();
        //TODO: manage " marks
        separated = line.split(",");
        if (separated.length() != pattern.length())
            continue;
        for (int i=0; i<pattern.length(); i++){
            switch (pattern[i]){
                case D_QSTRING:
                    newRow[names[i]]=separated[i];
                    break;
                case D_DOUBLE:
                    newRow[names[i]]=separated[i].toDouble();
                    break;
                case D_INT:
                    newRow[names[i]]=separated[i].toInt();
                    break;
                case D_FLOAT:
                    newRow[names[i]]=separated[i].toFloat();
                    break;
            }
        }
        success=true;
        
    }
    
//     if (fileReader.hasMoreLines() == false)
//         fileReader.close??
    
    return newRow;
}

QHash<QString,QVariant>  KSParser::ReadFixedWidthRow() {
    kWarning() <<"READ FWR";
    QHash<QString,QVariant> newRow;
    return newRow;
}

QHash<QString,QVariant>  KSParser::DummyCSVRow() {
    //TODO: allot 0 or "null" to every position
    QHash<QString,QVariant> newRow;
    for (int i=0; i<pattern.length(); i++){
            switch (pattern[i]){
                case D_QSTRING:
                    newRow[names[i]]="Null";
                    break;
                case D_DOUBLE:
                    newRow[names[i]]=0.0;
                    break;
                case D_INT:
                    newRow[names[i]]=0;
                    break;
                case D_FLOAT:
                    newRow[names[i]]=0.0;
                    break;
            }
        }
    kWarning() <<"READ FWR";
    return newRow;
}

bool KSParser::hasNextRow() {
    return fileReader.hasMoreLines();
}

