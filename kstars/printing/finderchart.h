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

#include "kstarsdocument.h"
#include "QRectF"

class LoggingForm;
class DetailsTable;
class KStarsDateTime;
class GeoLocation;
class QString;
class QTextDocument;
class QTextFrameFormat;
class QPrinter;
class QPainter;
class QRectF;
class QImage;

class FinderChart : public KStarsDocument
{
public:
    FinderChart();
    ~FinderChart();

    void insertTitleSubtitle(const QString &title, const QString &subtitle);
    void insertDescription(const QString &description);
    void insertGeoTimeInfo(const KStarsDateTime &ut, GeoLocation *geo);
    void insertLoggingForm(LoggingForm *log);
    void insertImage(const QImage &img, const QString &description, bool descriptionBelow = true);
    void insertDetailsTable(DetailsTable *table);
};

#endif // FINDERCHART_H
