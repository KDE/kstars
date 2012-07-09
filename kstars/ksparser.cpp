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
		  :m_Filename(filename),m_skipChar(skipChar),m_sequence(sequence),m_delimiter(delimiter) {
    if (!m_FileReader.open(m_Filename)) {
        kWarning() <<"Unable to open file: "<<filename;
        readFunctionPtr = &KSParser::DummyCSVRow;
    } else  {
        readFunctionPtr = &KSParser::ReadCSVRow;
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
	if (line[0] == m_skipChar) continue;
        separated = line.split(m_delimiter);
	/*
	 * 1) split along delimiter eg. comma (,)
	 * 2) check first and last characters. 
	 *    if the first letter is  '"',
	 *    then combine the nexto ones in it till
	 *    till you come across the next word which
	 *    has the last character as '"'
	 *
	*/
         //TODO: tests pending
	QString iterString;
        QList <QString> quoteCombined;
        QStringList::iterator iter;
        if (separated.length() == 0) continue;
	for (iter=separated.begin(); iter!= separated.end(); iter++) {
            QList <QString> queue;
	    if ((*iter)[0] == '"') {
                iterString = *iter;
                while (iterString[iterString.length()-1] != '"' && iter!= separated.end()) {
                    queue.append((*iter));
                    iter++;
                    iterString = *iter;
                }
                if (iterString[iterString.length()-1] == '"')
                    queue.append((*iter).remove( '"' ));
	    }
	    else queue.append(*iter);
            QString join;
            QString col_result;
            foreach (join, queue)
                col_result+=join;
            quoteCombined.append(col_result);
	}
	separated=quoteCombined;
        
        if (separated.length() != m_sequence.length())
            continue;
        for (int i=0; i<m_sequence.length(); i++) {
	    bool ok;
            switch (m_sequence[i].second){
                case D_QSTRING:
                case D_SKIP:
                    newRow[m_sequence[i].first]=separated[i];
                    break;
                case D_DOUBLE:
                    newRow[m_sequence[i].first]=separated[i].toDouble(&ok);
		    if (!ok) {
		      kDebug() <<  "toDouble Failed at field: "<< m_sequence[i].first <<" & line : " << line;
		      newRow[m_sequence[i].first] = EBROKEN_DOUBLE;
		    }
                    break;
                case D_INT:
                    newRow[m_sequence[i].first]=separated[i].toInt(&ok);
		    if (!ok) {
		      kDebug() << "toInt Failed at field: "<< m_sequence[i].first <<" & line : " << line;
		      newRow[m_sequence[i].first] = EBROKEN_INT;
		    }
                    break;
                case D_FLOAT:
                    newRow[m_sequence[i].first]=separated[i].toFloat(&ok);
		    if (!ok) {
		      kWarning() << "toFloat Failed at field: "<< m_sequence[i].first <<" & line : " << line;
		      newRow[m_sequence[i].first] = EBROKEN_FLOATS;
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
    for (int i=0; i<m_sequence.length(); i++){
           switch (m_sequence[i].second){
                case D_QSTRING:
                    newRow[m_sequence[i].first]="Null";
                    break;
                case D_DOUBLE:
                    newRow[m_sequence[i].first]=0.0;
                    break;
                case D_INT:
                    newRow[m_sequence[i].first]=0;
                    break;
                case D_FLOAT:
                    newRow[m_sequence[i].first]=0.0;
                    break;
            }
        }
    kWarning() <<"READ FWR";
    return newRow;
}

bool KSParser::hasNextRow() {
    return m_FileReader.hasMoreLines();
    
}