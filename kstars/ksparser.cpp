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

//TODO: Integrate with KSFileReader properly

KSParser::KSParser(QString filename, char skipChar, QList< QPair<QString,DataTypes> > sequence, char delimiter)
		  :m_Filename(filename),m_skipChar(skipChar),m_sequence(sequence),m_delimiter(delimiter) {
    if (!m_FileReader.open(m_Filename)) {
        kWarning() <<"Unable to open file: "<<filename;
        readFunctionPtr = &KSParser::DummyCSVRow;
    } else {
        readFunctionPtr = &KSParser::ReadCSVRow;
        kDebug() <<"File opened: "<<filename;
    }

}

KSParser::KSParser(QString filename, char skipChar, QList< QPair<QString,DataTypes> > sequence, 
             QList<int> widths): m_Filename(filename),m_skipChar(skipChar),m_sequence(sequence), m_widths(widths){
    if (!m_FileReader.open(m_Filename)) {
        kWarning() <<"Unable to open file: "<<filename;
        readFunctionPtr = &KSParser::DummyCSVRow;
    } else {
        readFunctionPtr = &KSParser::ReadFixedWidthRow;
        kDebug() <<"File opened: "<<filename;
    }
        

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
    * without checking if hasNextRow is true
    */
    if (m_FileReader.hasMoreLines() == false)
        return DummyCSVRow(); 
    
    
    /**
     * @brief Success (bool) signifies if a row has been successfully read.
     * If any problem (eg incomplete row) is encountered. The row is discarded
     * and the while loop continues till it finds a good row or the file ends.
     **/
    bool success = false;
    QString line;
    QStringList separated;
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
        
        //The following mish mash is to handle fields such as "hello, world"
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
	separated=quoteCombined; //At this point, the string has been split 
                                //taking the quote marks into account
        
        //Check if the generated list has correct size
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

    /**
    * This signifies that someone tried to read a row
    * without checking if hasNextRow is true
    */
    if (m_FileReader.hasMoreLines() == false)
        return DummyCSVRow(); 
    
    if (m_sequence.length() != (m_widths.length() + 1)) {
        kWarning() << "Unequal fields and widths! Returning dummy row!";
        return DummyCSVRow();
    }
    
    /**
    * @brief Success (bool) signifies if a row has been successfully read.
    * If any problem (eg incomplete row) is encountered. The row is discarded
    * and the while loop continues till it finds a good row or the file ends.
    **/
    bool success = false;
    QString line;
    QStringList separated;
    QHash<QString,QVariant> newRow;
    
    while (m_FileReader.hasMoreLines() && success == false) {
        line = m_FileReader.readLine();
        if (line[0] == m_skipChar) continue;
        
        int curr_width=0;
        for (int n_split=0; n_split<m_widths.length(); n_split++) {
            //Build separated stinglist. Then assign it afterwards.
            QString temp_split;
            temp_split = line.mid(curr_width, m_widths[n_split]);
                        //don't use at(), because it crashes on invalid index
            curr_width += m_widths[n_split];
            separated.append(temp_split);
        }
        separated.append(line.mid(curr_width)); //append last segment
        
        //Check if the generated list has correct size
        if (separated.length() != m_sequence.length()){
            kDebug() << "Invalid Size with line: "<< line;
            continue;
        }
        
        //Conversions
        //TODO: Redundant Code! DRY!
        for (int i=0; i<m_sequence.length(); i++) {
            bool ok;
            switch (m_sequence[i].second){
                case D_QSTRING:
                case D_SKIP:
                    newRow[m_sequence[i].first]=separated[i];
                    break;
                case D_DOUBLE:
                    newRow[m_sequence[i].first]=separated[i].trimmed().toDouble(&ok);
                    if (!ok) {
                      kDebug() <<  "toDouble Failed at field: "<< m_sequence[i].first 
                               <<" & line : " << line;
                      newRow[m_sequence[i].first] = EBROKEN_DOUBLE;
                    }
                    break;
                case D_INT:
                    newRow[m_sequence[i].first]=separated[i].trimmed().toInt(&ok);
                    if (!ok) {
                      kDebug() << "toInt Failed at field: "<< m_sequence[i].first 
                               <<" & line : " << line;
                      newRow[m_sequence[i].first] = EBROKEN_INT;
                    }
                    break;
                case D_FLOAT:
                    newRow[m_sequence[i].first]=separated[i].trimmed().toFloat(&ok);
                    if (!ok) {
                      kWarning() << "toFloat Failed at field: "<< m_sequence[i].first 
                                 <<" & line : " << line;
                      newRow[m_sequence[i].first] = EBROKEN_FLOATS;
                    }
                    break;
            }
        }
        
        success=true;
    }
    
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
    return newRow;
}

bool KSParser::hasNextRow() {
    return m_FileReader.hasMoreLines();
    
}