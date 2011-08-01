/***************************************************************************
                          detailstable.h  -  K Desktop Planetarium
                             -------------------
    begin                : Fri Jul 29 2011
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

#ifndef DETAILSTABLE_H
#define DETAILSTABLE_H

class SkyObject;
class KStarsDateTime;
class GeoLocation;

class QTextDocument;

class DetailsTable
{
public:
    DetailsTable();
    ~DetailsTable();

    void createGeneralTable(SkyObject *obj);
    void createAsteroidCometTable(SkyObject *obj);
    void createCoordinatesTable(SkyObject *obj, const KStarsDateTime &ut, GeoLocation *geo);
    void createRSTTAble(SkyObject *obj, const KStarsDateTime &ut, GeoLocation *geo);

    void clearContents();
    inline QTextDocument* getDocument() { return m_Document; }

private:
    QTextDocument *m_Document;
};

#endif // DETAILSTABLE_H
