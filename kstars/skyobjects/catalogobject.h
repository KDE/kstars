/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>
    SPDX-FileCopyrightText: 2021 Valentin Boettcher <hiro at protagon.space; @hiro98:tchncs.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "dms.h"
#include "skyobject.h"
#include "nan.h"
#include "texturemanager.h"
#include <QString>
#include <QByteArray>
#include <QImage>
#include <array>
#include <utility>

class KSPopupMenu;
class KStarsData;
class CatalogComponent;
class CatalogEntryData;
namespace CatalogsDB
{
struct Catalog;
}

/**
 * A simple container object to hold the minimum information for a Deeb
 * Sky Object to be drawn on the skymap.  Detailed information about the
 * object (like `Catalog` information) will be loaded from it's birthing
 * database when needed. Based on the original DeepSkyObject by Jason
 * Harris.
 *
 * You shouldn't create a CatalogObject manually!
 *
 * \todo maybe just turn the const members public
 */
class CatalogObject : public SkyObject
{
    friend class AddCatalogObject;

  public:
    using oid = QByteArray;

    /**
     * @param id oid (hash) of the object
     * @param t Type of object
     * @param r Right Ascension
     * @param d Declination
     * @param m magnitude (brightness)
     * @param n Primary name
     * @param lname Long name (common name)
     * @param catalog_identifier a catalog specific identifier
     * @param catalog_id catalog id
     * @param a major axis (arcminutes)
     * @param b minor axis (arcminutes)
     * @param pa position angle (degrees)
     * @param flux integrated flux (optional)
     * @param database_path the path to the birthing database of this
     * object, used to load additional information on-demand
     */
    CatalogObject(oid id = {}, const SkyObject::TYPE t = SkyObject::STAR,
                  const dms &r = dms(0.0), const dms &d = dms(0.0),
                  const float m = NaN::f, const QString &n = "unnamed",
                  const QString &lname              = QString(),
                  const QString &catalog_identifier = QString(),
                  const int catalog_id = -1, const float a = 0.0, const float b = 0.0,
                  const double pa = 0.0, const float flux = 0,
                  const QString &database_path = "")
        : SkyObject(t, r, d, m, n, catalog_identifier, lname),
          m_catalog_identifier{ catalog_identifier }, m_catalog_id{ catalog_id },
          m_database_path{ database_path }, m_major_axis{ a }, m_minor_axis{ b },
          m_position_angle{ pa }, m_flux{ flux }
    {
        if (id.length() == 0)
            m_object_id = getId();
        else
            m_object_id = std::move(id);
    };

    ~CatalogObject() override = default;

    /**
     * @return the string for the skymap label of the object.
     */
    QString labelString() const override;

    /**
     * Clones the object and returns a pointer to it. This is legacy
     * code.
     *
     * @deprecated do not use in new code
     */
    CatalogObject *clone() const override;

    /**
     * Generates a KStars internal UID from the object id.
     */
    SkyObject::UID getUID() const override;

    /**
     * @return the object's integrated flux, unit value is stored in
     * the custom catalog component.
     */
    inline float flux() const { return m_flux; }

    /**
     * @return the object's major axis length, in arcminutes.
     */
    inline float a() const { return m_major_axis; }

    /**
     * @return the object's minor axis length, in arcminutes.
     */
    inline float b() const { return m_minor_axis; }

    /**
     * @return the object's aspect ratio (MinorAxis/MajorAxis). Returns 1.0
     * if the object's MinorAxis=0.0.
     */
    float e() const;

    /**
     * @return the object's position angle in degrees, measured clockwise from North.
     */
    inline double pa() const override { return m_position_angle; }

    /**
     * Get information about the catalog that this objects stems from.
     *
     * The catalog information will be loaded from the catalog
     * database on demand.
     */
    const CatalogsDB::Catalog getCatalog() const;

    /**
     * Get the id of the catalog this object belongs to.
     */
    int catalogId() const { return m_catalog_id; }

    /**
     * @return the pixel distance for offseting the object's name label
     */
    double labelOffset() const override;

    /**
     * Update the cooridnates and the horizontal coordinates if
     * updateID or updateNumID have changed (global).
     */
    void JITupdate();

    /**
     * Initialize the popup menu for a `CatalogObject`.
     */
    void initPopupMenu(KSPopupMenu *pmenu) override;

    /**
     * \return the catalog specific identifier. (For example UGC ...)
     */

    const QString &catalogIdentifier() const { return m_catalog_identifier; };

    friend bool operator==(const CatalogObject &c1, const CatalogObject &c2)
    {
        return c1.m_object_id == c2.m_object_id;
    }

    friend bool operator!=(const CatalogObject &c1, const CatalogObject &c2)
    {
        return !(c1 == c2);
    }

    /**
     * \returns the unique ID of this object (not the physical ID =
     * m_object_id) by hashing unique properties of the object.
     *
     * This method provides the reference implementation for the oid hash.
     */
    const oid getId() const;
    static const oid getId(const SkyObject::TYPE type, const double ra, const double dec,
                           const QString &name, const QString &catalog_identifier);

    /**
     * \returns the physical object id of the catalogobject
     */
    const oid getObjectId() const { return m_object_id; };

    /**
     * Set the catalog identifier to \p `cat_ident`.
     */
    void setCatalogIdentifier(const QString &cat_ident)
    {
        m_catalog_identifier = cat_ident;
    }

    /**
     * Set the major axis to \p `a`.
     */
    void setMaj(const float a) { m_major_axis = a; }

    /**
     * Set the minor axis to \p `b`.
     */
    void setMin(const float b) { m_minor_axis = b; }

    /**
     * Set the flux to \p `flux`.
     */
    void setFlux(const float flux) { m_flux = flux; }

    /**
     * Set the position angle to \p `pa`.
     */
    void setPA(const double pa) { m_position_angle = pa; }

    /**
     * Set the magnitude of the object.
     */
    void setMag(const double mag) { SkyObject::setMag(mag); };

    /**
     * Load the image for this object.
     */
    void load_image();

    /**
     * Get the image for this object.
     *
     * @returns [has image?, the image]
     */
    inline std::pair<bool, const QImage &> image() const
    {
        return { !m_image.isNull(), m_image };
    }

  private:
    /**
     * The unique physical object identifier (hash).
     */
    oid m_object_id;

    /**
     * A catalog specific identifier.
     */
    QString m_catalog_identifier;

    /**
     * The id of the catalog, the object is originating from.
     *
     * A value of -1 means that the catalog is unknown. This member is
     * not exposed publicly because it is only useful along with
     * `m_database_path`.
     */
    int m_catalog_id;

    /**
     * The database path which this object was loaded from.
     */
    std::reference_wrapper<const QString> m_database_path;

    /**
     * Whether the image for this catalog object has been loaded.
     */
    bool m_image_loaded{ false };

    /**
     * The image for this object (if any).
     *
     * @todo use std::optional
     */
    QImage m_image;

    //@{
    /**
     * Astronical metadata.
     *
     * Those values may make sense depending on the type of the
     * object. A value of `-1` always means: "it doesn't make sense
     * here".
     */
    float m_major_axis;
    float m_minor_axis;

    double m_position_angle;
    float m_flux;
    //@}

    //@{
    /**
     * Update id counters.
     */
    quint64 m_updateID{ 0 };
    quint64 m_updateNumID{ 0 };
    //@}
};
