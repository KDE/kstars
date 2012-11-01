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
    enum SkyObjectRoles { DispNameRole = Qt::UserRole + 1, CategoryRole, CategoryNameRole };

    /**
     * \enum Type
     * The type classification for the SkyObjItem
     */
    enum Type { Planet, Star, Constellation, Galaxy, Cluster, Nebula };

    /**
     * \brief Constructor
     * \param so Pointer to the SkyObject which the SkyObjItem represents.
     */
    SkyObjItem(SkyObject *so = 0);

    /**
     * \brief Get data associated with a particular role for the SkyObjItem
     * \param role User-defined role for which data is required
     * \return QVariant data associated with role
     */
    QVariant data(int role);

    /**
     * \brief Create and return a QHash<int, QByteArray> of rolenames for the SkyObjItem.
     * \return QHash<int, QByteArray> of rolenames for the SkyObjItem.
     */
    QHash<int, QByteArray> roleNames() const;

    /**
     * \brief Get name of sky-object associated with the SkyObjItem.
     * \return Name of sky-object associated with the SkyObjItem as a QString
     */
    inline QString getName() const { return m_Name; }

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
     * \brief Get sky-object associated with the SkyObjItem.
     * \return Pointer to SkyObject associated with the SkyObjItem.
     */
    inline SkyObject* getSkyObject() { return m_So; }

    /**
     * \brief Get description for the SkyObjItem.
     * \return Description for the SkyObjItem as a QString.
     */
    QString getDesc() const;

    /**
     * \brief Get source of description for the SkyObjItem.
     * \return Source of description for the SkyObjItem as a QString.
     */
    QString getDescSource();

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
    QString m_Name;      ///Name of sky-object
    QString m_LongName;  ///Longname of sky-object(if available)
    QString m_TypeName;  ///Category of sky-object
    QString m_Position;  ///Position of sky-object in the sky.
    Type m_Type;         ///Category of sky-object of type SkyObjItem::Type
    SkyObject* m_So;     ///Pointer to SkyObject represented by SkyObjItem
};

#endif
