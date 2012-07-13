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

const double EBROKEN_DOUBLE = 0.0;
const float EBROKEN_FLOATS = 0.0;
const int EBROKEN_INT = 0;

KSParser::KSParser(QString filename, char comment_char, 
                   QList< QPair<QString,DataTypes> > sequence, char delimiter)
                   :filename_(filename),comment_char_(comment_char), 
                    name_type_sequence_(sequence),delimiter_(delimiter) {
    if (!file_reader_.open(filename_)) {
        kWarning() <<"Unable to open file: "<< filename;
        readFunctionPtr = &KSParser::DummyRow;
    } else {
        readFunctionPtr = &KSParser::ReadCSVRow;
        kDebug() <<"File opened: "<< filename;
    }
}

KSParser::KSParser(QString filename, char comment_char, 
                   QList< QPair<QString,DataTypes> > sequence, QList<int> widths)
                   :filename_(filename),comment_char_(comment_char),
                    name_type_sequence_(sequence), width_sequence_(widths) {
    if (!file_reader_.open(filename_)) {
        kWarning() <<"Unable to open file: "<< filename;
        readFunctionPtr = &KSParser::DummyRow;
    } else {
        readFunctionPtr = &KSParser::ReadFixedWidthRow;
        kDebug() <<"File opened: "<< filename;
    }
}

QHash<QString,QVariant>  KSParser::ReadNextRow() {
    return (this->*readFunctionPtr)();
}

QHash<QString,QVariant>  KSParser::ReadCSVRow() {
    if (file_reader_.hasMoreLines() == false)
        return DummyRow(); 
    //This signifies that someone tried to read a row
    //without checking if comment_char is true
    /**
     * @brief Success (bool) signifies if a row has been successfully read.
     * If any problem (eg incomplete row) is encountered. The row is discarded
     * and the while loop continues till it finds a good row or the file ends.
     **/
    bool success = false;
    QString next_line;
    QStringList separated;
    QHash<QString,QVariant> newRow;

    while (file_reader_.hasMoreLines() && success == false) {
        next_line = file_reader_.readLine();
	if (next_line[0] == comment_char_) continue;
        separated = next_line.split(delimiter_);
	/*
	 * 1) split along delimiter eg. comma (,)
	 * 2) check first and last characters.
	 *    if the first letter is  '"',
	 *    then combine the nexto ones in it till
	 *    till you come across the next word which
	 *    has the last character as '"'
	 *
	*/
         //TODO(spacetime): tests pending
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
        if (separated.length() != name_type_sequence_.length())
            continue;
        
        for (int i=0; i<name_type_sequence_.length(); i++) {
	    bool ok;
            switch (name_type_sequence_[i].second){
                case D_QSTRING:
                case D_SKIP:
                    newRow[name_type_sequence_[i].first]=separated[i];
                    break;
                case D_DOUBLE:
                    newRow[name_type_sequence_[i].first]=separated[i].toDouble(&ok);
		    if (!ok) {
		      kDebug() <<  "toDouble Failed at field: "<< name_type_sequence_[i].first <<" & next_line : " << next_line;
		      newRow[name_type_sequence_[i].first] = EBROKEN_DOUBLE;
		    }
                    break;
                case D_INT:
                    newRow[name_type_sequence_[i].first]=separated[i].toInt(&ok);
		    if (!ok) {
		      kDebug() << "toInt Failed at field: "<< name_type_sequence_[i].first <<" & next_line : " << next_line;
		      newRow[name_type_sequence_[i].first] = EBROKEN_INT;
		    }
                    break;
                case D_FLOAT:
                    newRow[name_type_sequence_[i].first]=separated[i].toFloat(&ok);
		    if (!ok) {
		      kWarning() << "toFloat Failed at field: "<< name_type_sequence_[i].first <<" & next_line : " << next_line;
		      newRow[name_type_sequence_[i].first] = EBROKEN_FLOATS;
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
    * without checking if comment_char is true
    */
    if (file_reader_.hasMoreLines() == false)
        return DummyRow(); 
    
    if (name_type_sequence_.length() != (width_sequence_.length() + 1)) {
        //line length is appendeded to width_sequence_ by default.
        kWarning() << "Unequal fields and widths! Returning dummy row!";
        return DummyRow();
    }
    
    /**
    * @brief Success (bool) signifies if a row has been successfully read.
    * If any problem (eg incomplete row) is encountered. The row is discarded
    * and the while loop continues till it finds a good row or the file ends.
    **/
    bool success = false;
    QString next_line;
    QStringList separated;
    QHash<QString,QVariant> newRow;
    
    while (file_reader_.hasMoreLines() && success == false) {
        next_line = file_reader_.readLine();
        if (next_line[0] == comment_char_) continue;
        
        int curr_width=0;
        for (int n_split=0; n_split<width_sequence_.length(); n_split++) {
            //Build separated stinglist. Then assign it afterwards.
            QString temp_split;
            temp_split = next_line.mid(curr_width, width_sequence_[n_split]);
                        //don't use at(), because it crashes on invalid index
            curr_width += width_sequence_[n_split];
            separated.append(temp_split);
        }
        separated.append(next_line.mid(curr_width)); //append last segment
        
        //Check if the generated list has correct size
        if (separated.length() != name_type_sequence_.length()){
            kDebug() << "Invalid Size with next_line: "<< next_line;
            continue;
        }
        
        //Conversions
        //TODO(spacetime): Redundant Code! DRY!
        for (int i=0; i<name_type_sequence_.length(); i++) {
            bool ok;
            switch (name_type_sequence_[i].second){
                case D_QSTRING:
                case D_SKIP:
                    newRow[name_type_sequence_[i].first]=separated[i];
                    break;
                case D_DOUBLE:
                    newRow[name_type_sequence_[i].first]=separated[i].trimmed().toDouble(&ok);
                    if (!ok) {
                      kDebug() <<  "toDouble Failed at field: "<< name_type_sequence_[i].first 
                               <<" & next_line : " << next_line;
                      newRow[name_type_sequence_[i].first] = EBROKEN_DOUBLE;
                    }
                    break;
                case D_INT:
                    newRow[name_type_sequence_[i].first]=separated[i].trimmed().toInt(&ok);
                    if (!ok) {
                      kDebug() << "toInt Failed at field: "<< name_type_sequence_[i].first 
                               <<" & next_line : " << next_line;
                      newRow[name_type_sequence_[i].first] = EBROKEN_INT;
                    }
                    break;
                case D_FLOAT:
                    newRow[name_type_sequence_[i].first]=separated[i].trimmed().toFloat(&ok);
                    if (!ok) {
                      kWarning() << "toFloat Failed at field: "<< name_type_sequence_[i].first 
                                 <<" & next_line : " << next_line;
                      newRow[name_type_sequence_[i].first] = EBROKEN_FLOATS;
                    }
                    break;
            }
        }
        
        success=true;
    }
    
    return newRow;
}

QHash<QString,QVariant>  KSParser::DummyRow() {
    //TODO: allot 0 or "null" to every position
    QHash<QString,QVariant> newRow;
    for (int i=0; i<name_type_sequence_.length(); i++){
           switch (name_type_sequence_[i].second){
                case D_QSTRING:
                    newRow[name_type_sequence_[i].first]="Null";
                    break;
                case D_DOUBLE:
                    newRow[name_type_sequence_[i].first]=0.0;
                    break;
                case D_INT:
                    newRow[name_type_sequence_[i].first]=0;
                    break;
                case D_FLOAT:
                    newRow[name_type_sequence_[i].first]=0.0;
                    break;
            }
        }
    return newRow;
}

bool KSParser::HasNextRow() {
    return file_reader_.hasMoreLines();
    
}

void KSParser::SetProgress(QString msg, int total_lines, int step_size) {
    file_reader_.setProgress(i18n("Loading NGC/IC objects"), total_lines, step_size);
}

void KSParser::ShowProgress()
{
    file_reader_.showProgress();
}
