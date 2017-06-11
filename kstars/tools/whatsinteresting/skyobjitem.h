/***************************************************************************
                          skyobjitem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/21/06
    copyright            : (C) 2012 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKYOBJITEM_H
#define SKYOBJITEM_H

#include "skyobject.h"
#include "skyobjdescription.h"

/**
 * \class SkyObjItem
 * Represents an item in the list of interesting sky-objects.
 * \author Samikshan Bairagya
 */
class SkyObjItem
{
  public:
    /**
         * \enum SkyObjectRoles
         * User-defined role for the SkyObjItem
         */
    enum SkyObjectRoles
    {
        DispNameRole = Qt::UserRole + 1,
        DispImageRole,
        DispSummaryRole,
        CategoryRole,
        CategoryNameRole
    };

    /**
         * \enum Type
         * The type classification for the SkyObjItem
         */
    enum Type
    {
        Planet,
        Star,
        Constellation,
        Galaxy,
        Cluster,
        Nebula,
        Supernova
    };

    /**
         * \brief Constructor
         * \param so Pointer to the SkyObject which the SkyObjItem represents.
         */
    SkyObjItem(SkyObject *so = 0);

    /**
         * \brief Destructor
         */
    ~SkyObjItem();

    /**
         * \brief Get data associated with a particular role for the SkyObjItem
         * \param role User-defined role for which data is required
         * \return QVariant data associated with role
         */
    QVariant data(int role);

    /**
         * \brief Get name of sky-object associated with the SkyObjItem.
         * \return Name of sky-object associated with the SkyObjItem as a QString
         */
    inline QString getName() const { return m_Name; }

    /**
         * \brief Get longname of sky-object associated with the SkyObjItem.
         * \return Longname of sky-object associated with the SkyObjItem as a QString
         */
    inline QString getDescName() const
    {
        if (m_LongName == m_Name)
            return m_LongName;
        else
            return m_LongName + "\n (" + m_Name + ")";
    }

    /**
         * \brief Get longname of sky-object associated with the SkyObjItem.
         * \return Longname of sky-object associated with the SkyObjItem as a QString
         */
    inline QString getLongName() const { return m_LongName; }

    /**
         * \brief Get category of sky-object associated with the SkyObjItem as a QString.
         * \return Category of sky-object associated with the SkyObjItem as a QString.
         */
    inline QString getTypeName() const { return m_TypeName; }

    /**
         * \brief Get category of sky-object associated with the SkyObjItem as an integer.
         * \return Category of sky-object associated with the SkyObjItem as a QString as an integer.
         */
    inline int getType() const { return m_Type; }

    /**
         * \brief Get current position of sky-object associated with the SkyObjItem.
         * \return Current position of sky-object associated with the SkyObjItem.
         */
    inline QString getPosition() const { return m_Position; }

    /**
         * \brief Get current RA/DE of sky-object associated with the SkyObjItem.
         * \return Current RA/DE of sky-object associated with the SkyObjItem.
         */
    inline QString getRADE() const
    {
        return "RA: " + m_So->ra().toHMSString() + "<BR>DE: " + m_So->dec().toDMSString();
    }

    /**
         * \brief Get current Altitute and Azimuth of sky-object associated with the SkyObjItem.
         * \return Current Altitute and Azimuth of sky-object associated with the SkyObjItem.
         */
    inline QString getAltAz() const
    {
        return "Alt: " + QString::number(m_So->alt().Degrees(), 'f', 2) +
               ", Az: " + QString::number(m_So->az().Degrees(), 'f', 2);
    }

    /**
         * \brief Get sky-object associated with the SkyObjItem.
         * \return Pointer to SkyObject associated with the SkyObjItem.
         */
    inline SkyObject *getSkyObject() { return m_So; }

    QString getImageURL(bool preferThumb) const;

    inline QString loadObjectDescription() const;

    /**
         * \brief Get Summary Description for the SkyObjItem.
         * \return Summary Description for the SkyObjItem as a QString.
         */
    QString getSummary(bool includeDescription) const;

    /**
         * \brief Get magnitude of sky-object associated with the SkyObjItem.
         * \return Magnitude of sky-object associated with the SkyObjItem.
         */
    inline float getMagnitude() const { return m_So->mag(); }

    /**
         * \brief Get surface-brightness of sky-object associated with the SkyObjItem as a QString
         * to be displayed on the details-view.
         * \return Surface-brightness of sky-object associated with the SkyObjItem as a QString.
         */
    QString getSurfaceBrightness() const;

    /**
         * \brief Get size of sky-object associated with the SkyObjItem as a QString
         * to be displayed on the details-view.
         * \return Size of sky-object associated with the SkyObjItem as a QString.
         */
    QString getSize() const;

    /**
         * \brief Set current position of the sky-object in the sky.
         * \param so Pointer to SkyObject for which position information is required.
         */
    void setPosition(SkyObject *so);

  private:
    QString m_Name;         ///Name of sky-object
    QString m_LongName;     ///Longname of sky-object(if available)
    QString m_TypeName;     ///Category of sky-object
    QString m_Position;     ///Position of sky-object in the sky.
    Type m_Type;            ///Category of sky-object of type SkyObjItem::Type
    SkyObject *m_So;        ///Pointer to SkyObject represented by SkyObjItem
    SkyObjDescription *skd; /// pointer to SkyObjDescription
};

#endif
