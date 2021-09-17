/*
    SPDX-FileCopyrightText: 2011 Rafał Kułaga <rl.kulaga@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef DETAILSTABLE_H
#define DETAILSTABLE_H

#include "QTextTableFormat"
#include "QTextCharFormat"

class SkyObject;
class KStarsDateTime;
class GeoLocation;
class QTextDocument;

/**
 * \class DetailsTable
 * \brief Represents details tables that can be inserted to finder charts and logging forms.
 * DetailsTable class is used for creating QTextTables filled with details about objects of all types.
 * Created tables are stored inside QTextDocument, which can be obtained by DetailsTable::getDocument() and
 * inserted into other QTextDocument as fragment.
 * Four types of details tables are supported: general details, position details, Rise/Set/Transit details and
 * Asteroid/Comet details.
 * \author Rafał Kułaga
 */
class DetailsTable
{
  public:
    /**
         * \short Default constructor - creates empty details table.
         */
    DetailsTable();

    /**
         * \short Destructor.
         */
    ~DetailsTable();

    /**
          * \short Get table format.
          * \return Current table format.
          */
    inline QTextTableFormat getTableFormat() { return m_TableFormat; }

    /**
          * \short Get table title character format.
          * \return Current table title character format.
          */
    inline QTextCharFormat getTableTitleCharFormat() { return m_TableTitleCharFormat; }

    /**
          * \short Get table item name character format.
          * \return Current table item name character format.
          */
    inline QTextCharFormat getItemNameCharFormat() { return m_ItemNameCharFormat; }

    /**
          * \short Get table item value character format.
          * \return Current table item value character format.
          */
    inline QTextCharFormat getItemValueCharFormat() { return m_ItemValueCharFormat; }

    /**
          * \short Set table format.
          * \param format New table format.
          */
    inline void setTableFormat(const QTextTableFormat &format) { m_TableFormat = format; }

    /**
          * \short Set table title character format.
          * \param format New table title character format.
          */
    inline void setTableTitleCharFormat(const QTextCharFormat &format) { m_TableTitleCharFormat = format; }

    /**
          * \short Set table item name character format.
          * \param format New table item name character format.
          */
    inline void setItemNameCharFormat(const QTextCharFormat &format) { m_ItemNameCharFormat = format; }

    /**
          * \short Set table item value character format.
          * \param format New table item value character format.
          */
    inline void setItemValueCharFormat(const QTextCharFormat &format) { m_ItemValueCharFormat = format; }

    /**
          * \short Create general details table.
          * \param obj SkyObject for which table will be created.
          */
    void createGeneralTable(SkyObject *obj);

    /**
          * \short Create Asteroid/Comet details table.
          * \param obj Sky object (Asteroid/Comet) for which table will be created.
          */
    void createAsteroidCometTable(SkyObject *obj);

    /**
          * \short Create coordinates details table.
          * \param obj Sky object for which table will be created.
          * \param ut Date and time.
          * \param geo Geographic location.
          */
    void createCoordinatesTable(SkyObject *obj, const KStarsDateTime &ut, GeoLocation *geo);

    /**
          * \short Create Rise/Set/Transit details table.
          * \param obj Sky object for which table will be created.
          * \param ut Date and time.
          * \param geo Geographic location.
          */
    void createRSTTAble(SkyObject *obj, const KStarsDateTime &ut, GeoLocation *geo);

    /**
          * \short Clear current table.
          */
    void clearContents();

    /**
          * \short Get table document.
          * \return Table document.
          */
    inline QTextDocument *getDocument() { return m_Document; }

  private:
    /**
          * \short Sets default table formatting.
          */
    void setDefaultFormatting();

    QTextDocument *m_Document;

    QTextTableFormat m_TableFormat;
    QTextCharFormat m_TableTitleCharFormat;
    QTextCharFormat m_ItemNameCharFormat;
    QTextCharFormat m_ItemValueCharFormat;
};

#endif // DETAILSTABLE_H
