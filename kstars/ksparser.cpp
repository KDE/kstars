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


KSParser::KSParser(QString filename, char skipChar, QList< QPair<QString,DataTypes> > sequence, char delimiter)
		  :m_Filename(filename),skipChar(skipChar),sequence(sequence) {
    if (!m_FileReader.open(m_Filename)) {
        kWarning() <<"Unable to open file: "<<filename;
        readFunctionPtr = &KSParser::DummyCSVRow;
    } else  { readFunctionPtr = &KSParser::ReadCSVRow;
    kDebug() <<"File opened: "<<filename;
    }

}

KSParser::KSParser(QString filename, char skipChar, QList< QPair<QString,DataTypes> > sequence, 
             QList<int> widths) {
    
        readFunctionPtr = &KSParser::ReadFixedWidthRow;

}

QHash<QString,QVariant>  KSParser::ReadNextRow() {
    return (this->*readFunctionPtr)();
}

#define EBROKEN_DOUBLE 0.0
#define EBROKEN_FLOATS 0.0
#define EBROKEN_INT 0

QHash<QString,QVariant>  KSParser::ReadCSVRow() {
   
    /**
     * This signifies that someone tried to read a row
    without checking if hasNextRow is true
    */
    if (m_FileReader.hasMoreLines() == false)
        return DummyCSVRow(); 
    
    QString line;
    QStringList separated;
    bool success = false;
    QHash<QString,QVariant> newRow;
    
    while (m_FileReader.hasMoreLines() && success == false) {
        line = m_FileReader.readLine();
	if (line[0] == skipChar) continue;
        separated = line.split(",");
	/*
	 * TODO: here's how to go about the parsing
	 * 1) split along ,
	 * 2) check first and last characters. 
	 *    if the first letter is  '"',
	 *    then combine the nexto ones in it till
	 *    till you come across the next word which
	 *    has the last character as '"'
	 *
	*/
	/*
	for (QStringListIterator i; i!= separated.end(); ++i){
	    if (*i[0] == '#') {
		
	    }
	}
	
	*/
        if (separated.length() != sequence.length())
            continue;
        for (int i=0; i<sequence.length(); i++) {
	    bool ok;
            switch (sequence[i].second){
                case D_QSTRING:
                case D_SKIP:
                    newRow[sequence[i].first]=separated[i];
                    break;
                case D_DOUBLE:
                    newRow[sequence[i].first]=separated[i].toDouble(&ok);
		    if (!ok) {
		      kDebug() <<  "toDouble Failed at field: "<< sequence[i].first <<" & line : " << line;
		      newRow[sequence[i].first] = EBROKEN_DOUBLE;
		    }
                    break;
                case D_INT:
                    newRow[sequence[i].first]=separated[i].toInt(&ok);
		    if (!ok) {
		      kDebug() << "toInt Failed at field: "<< sequence[i].first <<" & line : " << line;
		      newRow[sequence[i].first] = EBROKEN_INT;
		    }
                    break;
                case D_FLOAT:
                    newRow[sequence[i].first]=separated[i].toFloat(&ok);
		    if (!ok) {
		      kWarning() << "toFloat Failed at field: "<< sequence[i].first <<" & line : " << line;
		      newRow[sequence[i].first] = EBROKEN_FLOATS;
		    }
                    break;
            }
        }
        success = true;
        
    }
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
    for (int i=0; i<sequence.length(); i++){
           switch (sequence[i].second){
                case D_QSTRING:
                    newRow[sequence[i].first]="Null";
                    break;
                case D_DOUBLE:
                    newRow[sequence[i].first]=0.0;
                    break;
                case D_INT:
                    newRow[sequence[i].first]=0;
                    break;
                case D_FLOAT:
                    newRow[sequence[i].first]=0.0;
                    break;
            }
        }
    kWarning() <<"READ FWR";
    return newRow;
}

bool KSParser::hasNextRow() {
    return m_FileReader.hasMoreLines();
    
}