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


KSParser::KSParser(QString filename, char skipChar, char delimiter, QList<DataTypes> pattern){

   readFunctionPtr = &KSParser::ReadCSVRow;

}

KSParser::KSParser(QString filename, char skipChar, QList<int> widths) {

    readFunctionPtr = &KSParser::ReadFixedWidthRow;

}

void KSParser::ReadNextRow() {
    (*this.*readFunctionPtr)();
}


void KSParser::ReadCSVRow() {
    kWarning() <<"READ CSV";
    
}

void KSParser::ReadFixedWidthRow() {
    kWarning() <<"READ FWR";
}

