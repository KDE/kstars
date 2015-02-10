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

#ifndef CUSTOMCATALOGCOMPONENT_H
#define CUSTOMCATALOGCOMPONENT_H


#include "listcomponent.h"
#include "Options.h"
#include "datahandlers/catalogdb.h"

struct stat;

/**
*@class CatalogComponent
*Represents a custom user-defined catalog.
Code adapted from CustomCatalogComponent.cpp originally authored
by Thomas Kabelmann --spacetime

*@author Thomas Kabelmann
         Rishab Arora (spacetime)
*@version 0.2
*/

class CatalogComponent: public ListComponent
{
public:

    /**
     *@short Constructor
     *@p parent Pointer to the parent SkyComposite object
     */
    CatalogComponent( SkyComposite*, const QString &fname, bool showerrs, int index );

    /**
     *@short Destructor.  Delete list members
     */
    virtual ~CatalogComponent();

    /**
     *@short Draw custom catalog objects on the sky map.
     *@p psky Reference to the QPainter on which to paint
     */
    virtual void draw( SkyPainter *skyp );

    virtual void update( KSNumbers *num );

    /** @return the name of the catalog */
    QString name() const { return m_catName; }

    /** @return the frequency of the flux readings in the catalog, if any */
    QString fluxFrequency() const { return m_catFluxFreq; }

    /** @return the unit of the flux measurements in the catalog, if any */
    QString fluxUnit() const { return m_catFluxUnit; }

    /**
     *@return true if visibility Option is set for this catalog
     *@note this is complicated for custom catalogs, because
     *Options::showCatalog() returns a QList<int>, not a bool.
     *This function extracts the correct visibility value and
     *returns the appropriate bool value
     */
    inline bool getVisibility() { return (Options::showCatalog()[m_ccIndex] > 0) ? true : false; }

private:

    /** @short Load data into custom catalog */
    void loadData();

    /** @short Read data for existing custom catalogs from disk
     * @return true if catalog data was successfully read
     */
    bool readCustomCatalogs();

    /**
     *@short Remove a catalog rom the program
     *@p i The index of the catalog to be removed
     *@return true if catalog was successfully removed
     */
    bool removeCatalog( int i );

    /**
     *@short Process a line from a custom data file
     *@p lnum the line number being processed (used for error reporting)
     *@p d QStringList containing the data fields in the current line
     *@p Columns QStringList containing the column descriptors for the catalog (read from header)
     *@p Prefix The prefix string for naming objects in this catalog (read from header)
     *@p objList reference to the QList of SkyObjects to which we will add the parsed object
     *@p showerrs if true, parse errors will be logged and reported
     *@p errs reference to the string list containing the parse errors encountered
     */
    bool processCustomDataLine(int lnum, const QStringList &d, const QStringList &Columns,
                               bool showerrs, QStringList &errs);

    /**
     * @brief Returns true if this catalog is to be drawn
     * Overridden from SkyComponent::selected
     * @return bool
     **/
    bool selected();

    QString m_catName, m_catPrefix, m_catColor, m_catFluxFreq, m_catFluxUnit;
    float m_catEpoch;
    bool m_Showerrs;
    int m_ccIndex;
    quint32 updateID;

    static QStringList m_Columns;
};

#endif
