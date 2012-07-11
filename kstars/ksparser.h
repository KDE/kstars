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

/**
 * @brief Generic class for text file parsers used in KStars.
 * Read rows using ReadCSVRow() regardless of the type of parser.
 **/
class KSParser
{
public:
    /**
     * @brief DataTypes for building sequence
     * D_QSTRING QString Type
     * D_INT Integer  Type
     * D_FLOAT Floating Point Type
     * D_DOUBLE Double PRecision Type
     * D_SKIP Not Needed. This string is not converted from QString
     * 
     **/
    enum DataTypes {D_QSTRING, D_INT, D_FLOAT, D_DOUBLE, D_SKIP};
    
    
    /**
     * @brief Constructor to return a CSV parsing instance of a KSParser type object. 
     *
     * @param filename Filename of source file
     * @param skipChar Character signifying a comment line
     * @param sequence QList of QPairs of the form "field name,data type" 
     * @param delimiter separate on which character. default ','
     **/
    KSParser(QString filename, char skipChar, QList< QPair<QString,DataTypes> > sequence,
             char delimiter = ',') __attribute__((cdecl));
             

    /**
     * @brief onstructor to return a Fixed Width parsing instance of a KSParser type object. 
     *
     * @param filename Filename of source file
     * @param skipChar Character signifying a comment line
     * @param sequence QList of QPairs of the form "field name,data type" 
     * @param widths QList of the width sequence eg 4,5,10
     **/
    KSParser(QString filename, char skipChar, QList< QPair<QString,DataTypes> > sequence, 
             QList<int> widths) __attribute__((cdecl));

             
    /**
     * @brief ReadNextRow is a generic function used to read the next row of a text file.
     * The contructor changes the function pointer to the appropriate function.
     * Returns the row as <"column name", value>
     *
     * @return QHash< QString, QVariant >
     **/
    QHash<QString,QVariant>  ReadNextRow();
    
    /**
     * @brief Returns True if there are more rows to be read
     *
     * @return bool
     **/
    bool hasNextRow();
private:

    /**
     * @brief The Function Pointer used by ReadNextRow to call the appropriate function
     *
     * @return QHash< QString, QVariant >
     **/
    QHash<QString,QVariant> (KSParser::*readFunctionPtr)();
    
    /**
     * @brief Returns a single row from CSV. If hasNextRow is false, returns a
     * row with default values.
     *
     * @return QHash< QString, QVariant >
     **/
    QHash<QString,QVariant>  ReadCSVRow();
    
    /**
     * @brief Returns a single row from Fized Width File. If hasNextRow is false, returns a
     * row with default values.
     *
     * @return QHash< QString, QVariant >
     **/
    QHash<QString,QVariant>  ReadFixedWidthRow();
    
    /**
     * @brief Returns a default value row based on the currently assigned sequence.
     *
     * @return QHash< QString, QVariant >
     **/
    QHash<QString,QVariant>  DummyCSVRow();
    
    KSFileReader m_FileReader;
    QString m_Filename;
    int m_CurrentRowID;
    bool moreRows;
    char m_skipChar;
    
    QList< QPair<QString,DataTypes> > m_sequence;
    QList<int> m_widths;
    char m_delimiter;
    
};

#endif // KSPARSER_H
