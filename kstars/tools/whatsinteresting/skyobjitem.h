/*
    SPDX-FileCopyrightText: 2012 Samikshan Bairagya <samikshan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QVariant>

class SkyObject;

/**
 * @class SkyObjItem
 * Represents an item in the list of interesting sky-objects.
 *
 * @author Samikshan Bairagya
 */
class SkyObjItem
{
  public:
    /**
     * @enum SkyObjectRoles
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
     * @enum Type
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
     * @brief Constructor
     * @param so Pointer to the SkyObject which the SkyObjItem represents.
     */
    explicit SkyObjItem(SkyObject *so = nullptr);
    ~SkyObjItem() = default;

    /**
     * @brief Get data associated with a particular role for the SkyObjItem
     * @param role User-defined role for which data is required
     * @return QVariant data associated with role
     */
    QVariant data(int role);

    /**
     * @brief Get name of sky-object associated with the SkyObjItem.
     * @return Name of sky-object associated with the SkyObjItem as a QString
     */
    inline QString getName() const { return m_Name; }

    /**
     * @brief Get longname of sky-object associated with the SkyObjItem.
     * @return Longname of sky-object associated with the SkyObjItem as a QString
     */
    inline QString getDescName() const
    {
        if (m_LongName == m_Name)
            return m_LongName;
        else
            return m_LongName + "\n (" + m_Name + ")";
    }

    /**
     * @brief Get longname of sky-object associated with the SkyObjItem.
     * @return Longname of sky-object associated with the SkyObjItem as a QString
     */
    inline QString getLongName() const { return m_LongName; }

    /**
     * @brief Get category of sky-object associated with the SkyObjItem as a QString.
     * @return Category of sky-object associated with the SkyObjItem as a QString.
     */
    inline QString getTypeName() const { return m_TypeName; }

    /**
     * @brief Get category of sky-object associated with the SkyObjItem as an integer.
     * @return Category of sky-object associated with the SkyObjItem as a QString as an integer.
     */
    inline int getType() const { return m_Type; }

    /**
     * @brief Get current position of sky-object associated with the SkyObjItem.
     * @return Current position of sky-object associated with the SkyObjItem.
     */
    inline QString getPosition() const { return m_Position; }

    /**
     * @brief Get current RA/DE of sky-object associated with the SkyObjItem.
     * @return Current RA/DE of sky-object associated with the SkyObjItem.
     */
    QString getRADE() const;

    /**
     * @brief Get current Altitude and Azimuth of sky-object associated with the SkyObjItem.
     * @return Current Altitude and Azimuth of sky-object associated with the SkyObjItem.
     */
    QString getAltAz() const;

    /**
     * @brief Get sky-object associated with the SkyObjItem.
     * @return Pointer to SkyObject associated with the SkyObjItem.
     */
    inline SkyObject *getSkyObject() { return m_So; }

    QString getImageURL(bool preferThumb) const;

    inline QString loadObjectDescription() const;

    /**
     * @brief Get Summary Description for the SkyObjItem.
     * @return Summary Description for the SkyObjItem as a QString.
     */
    QString getSummary(bool includeDescription) const;

    /**
     * @brief Get magnitude of sky-object associated with the SkyObjItem.
     * @return Magnitude of sky-object associated with the SkyObjItem.
     */
    float getMagnitude() const;

    /**
     * @brief Get surface-brightness of sky-object associated with the SkyObjItem as a QString
     * to be displayed on the details-view.
     * @return Surface-brightness of sky-object associated with the SkyObjItem as a QString.
     */
    QString getSurfaceBrightness() const;

    /**
     * @brief Get size of sky-object associated with the SkyObjItem as a QString
     * to be displayed on the details-view.
     * @return Size of sky-object associated with the SkyObjItem as a QString.
     */
    QString getSize() const;

    /**
     * @brief Set current position of the sky-object in the sky.
     * @param so Pointer to SkyObject for which position information is required.
     */
    void setPosition(SkyObject *so);

  private:
    /// Name of sky-object
    QString m_Name;
    /// Long name of sky-object(if available)
    QString m_LongName;
    /// Category of sky-object
    QString m_TypeName;
    /// Position of sky-object in the sky.
    QString m_Position;
    /// Category of sky-object of type SkyObjItem::Type
    Type m_Type { SkyObjItem::Planet };
    /// Pointer to SkyObject represented by SkyObjItem
    SkyObject *m_So { nullptr };
};
