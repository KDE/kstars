/***************************************************************************
                          loggingform.h  -  K Desktop Planetarium
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

#ifndef LOGGINGFORM_H
#define LOGGINGFORM_H

class QTextDocument;
class QPrinter;

class LoggingForm
{
public:
    LoggingForm();
    ~LoggingForm();

    void createFinderChartLogger();

    void print(QPrinter *printer);

    QTextDocument* getDocument();

private:
    QTextDocument *m_Document;
};

#endif // LOGGINGFORM_H
