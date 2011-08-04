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

#include "QTextTableFormat"
#include "QTextCharFormat"

class SkyObject;
class KStarsDateTime;
class GeoLocation;
class QTextDocument;

class DetailsTable
{
public:
    DetailsTable();
    ~DetailsTable();

    inline QTextTableFormat getTableFormat() { return m_TableFormat; }
    inline QTextCharFormat getTableTitleCharFormat() { return m_TableTitleCharFormat; }
    inline QTextCharFormat getItemNameCharFormat() { return m_ItemNameCharFormat; }
    inline QTextCharFormat getItemValueCharFormat() { return m_ItemValueCharFormat; }

    inline void setTableFormat(const QTextTableFormat &format) { m_TableFormat = format; }
    inline void setTableTitleCharFormat(const QTextCharFormat &format) { m_TableTitleCharFormat = format; }
    inline void setItemNameCharFormat(const QTextCharFormat &format) { m_ItemNameCharFormat = format; }
    inline void setItemValueCharFormat(const QTextCharFormat &format) { m_ItemValueCharFormat = format; }

    void createGeneralTable(SkyObject *obj);
    void createAsteroidCometTable(SkyObject *obj);
    void createCoordinatesTable(SkyObject *obj, const KStarsDateTime &ut, GeoLocation *geo);
    void createRSTTAble(SkyObject *obj, const KStarsDateTime &ut, GeoLocation *geo);

    void clearContents();
    inline QTextDocument* getDocument() { return m_Document; }

private:
    void setDefaultFormatting();

    QTextDocument *m_Document;

    QTextTableFormat m_TableFormat;
    QTextCharFormat m_TableTitleCharFormat;
    QTextCharFormat m_ItemNameCharFormat;
    QTextCharFormat m_ItemValueCharFormat;
};

#endif // DETAILSTABLE_H
