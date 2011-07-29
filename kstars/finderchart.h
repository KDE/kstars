/***************************************************************************
                          finderchart.h  -  K Desktop Planetarium
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

#ifndef FINDERCHART_H
#define FINDERCHART_H

#include "QList"
#include "QImage"

class LoggingForm;
class SkyObject;

class QTextDocument;
class QTextFrameFormat;
class QPrinter;
class QPainter;
class QRectF;
class QImage;

class FinderChart
{
public:
    FinderChart();
    ~FinderChart();

    void insertTitle(const QString &title);
    void insertLoggingForm(LoggingForm *log);
    void insertImage(const QImage &img, const QString &description, bool descriptionBelow = true);
    void insertDetailsTable(SkyObject *obj, bool general = true, bool position = true);

    void clearContent();
    void drawContents(QPainter *p, const QRectF &rect = QRectF());
    void print(QPrinter *printer);

private:
    QTextDocument *m_Document;
    LoggingForm *m_Log;
};

#endif // FINDERCHART_H
