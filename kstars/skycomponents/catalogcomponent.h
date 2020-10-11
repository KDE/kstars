/***************************************************************************
                    catalogcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/17/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#pragma once

#include "listcomponent.h"
#include "Options.h"

struct stat;

/**
 * @class CatalogComponent
 * Represents a custom user-defined catalog.
 * Code adapted from CustomCatalogComponent.cpp originally authored by Thomas Kabelmann --spacetime
 *
 * @author Thomas Kabelmann
 *         Rishab Arora (spacetime)
 * @version 0.2
 */
class CatalogComponent : public ListComponent
{
  public:
    /**
     * @short Constructor
     * @p parent Pointer to the parent SkyComposite object
     */
    CatalogComponent(SkyComposite *, const QString &fname, bool showerrs, int index, bool callLoadData = true);

    /**
     * @short Destructor.  Delete list members
     */
    ~CatalogComponent() override;

    /**
     * @short Draw custom catalog objects on the sky map.
     * @p psky Reference to the QPainter on which to paint
     */
    void draw(SkyPainter *skyp) override;

    void update(KSNumbers *num) override;

    SkyObject *findByName(const QString &name) override;

    /** @return the name of the catalog */
    inline QString name() const { return m_catName; }

    /** @return the frequency of the flux readings in the catalog, if any */
    inline QString fluxFrequency() const { return m_catFluxFreq; }

    /** @return the unit of the flux measurements in the catalog, if any */
    inline QString fluxUnit() const { return m_catFluxUnit; }

    /** @return color, which should be used for drawing objects in this catalog **/
    inline QString catColor() { return m_catColor; }

    /**
     * @return true if visibility Option is set for this catalog
     * @note this is complicated for custom catalogs, because
     * Options::showCatalog() returns a QList<int>, not a bool.
     * This function extracts the correct visibility value and
     * returns the appropriate bool value
     */
    bool getVisibility();

    /** @see SyncedCatalogItem */
    quint32 getUpdateID() { return updateID; }

    /**
     * @brief Returns true if this catalog is to be drawn
     * Overridden from SkyComponent::selected
     * @return bool
     **/
    bool selected() override;

  protected:
    /** @short Load data into custom catalog */
    virtual void loadData() { _loadData(true); }

    /** @short Load data into custom catalog */
    virtual void _loadData(bool includeCatalogDesignation);

    // FIXME: There seems to be no way to remove catalogs from the program. -- asimha

    QString m_catName, m_catColor, m_catFluxFreq, m_catFluxUnit;
    bool m_Showerrs { false };
    int m_ccIndex { 0 };
    quint32 updateID { 0 };

  private:
    // FIXME: Keeping both the objectNames data structure and the
    // nameHash seems overkill -- asimha
    QHash<QString, SkyObject *> nameHash;

};
