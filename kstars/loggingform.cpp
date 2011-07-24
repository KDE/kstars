/***************************************************************************
                          loggingform.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Jul 20 2011
    copyright            : (C) 2011 by Rafał Kułaga
    email                : rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "loggingform.h"

#include "QTextDocument"
#include "kstars.h"

LoggingForm::LoggingForm()
{
    m_Document = new QTextDocument(KStars::Instance());
}

LoggingForm::~LoggingForm()
{
    if(m_Document)
    {
        delete m_Document;
    }
}

void LoggingForm::createFinderChartLogger()
{

}

QTextDocument* LoggingForm::getDocument()
{
    return m_Document;
}


