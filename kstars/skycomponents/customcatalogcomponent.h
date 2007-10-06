/***************************************************************************
                          deepskycomponent.h  -  K Desktop Planetarium
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

/**
	*@class CustomCatalogComponent
	*Represents a custom user-defined catalog.

	*@author Thomas Kabelmann
	*@version 0.1
	*/

#include "listcomponent.h"

class KStarsData;
class CustomCatalog;

//JH: TODO: this class should only contain one custom catalog.

class CustomCatalogComponent: public ListComponent
{
public:

    /**
    	*@short Constructor
    	*@p parent Pointer to the parent SkyComponent object
    	*/
    CustomCatalogComponent( SkyComponent*, const QString &fname, bool showerrs, bool (*visibleMethod)() );
    /**
    	*@short Destructor.  Delete list members
    	*/
    virtual ~CustomCatalogComponent();

    /**
    	*@short Draw custom catalog objects on the sky map.
    	*@p ks pointer to the KStars object
    	*@p psky Reference to the QPainter on which to paint
    	*/
    virtual void draw( KStars *ks, QPainter& psky );

    /**
    	*@short Initialize the Custom catalog
    	*@p data Pointer to the KStarsData object
    	*/
    virtual void init(KStarsData *data);

    /**
    	*@return the name of the catalog
    	*/
    QString name() const { return m_catName; }

private:

    /**
    	*@short Read data for existing custom catalogs from disk
    	*@return true if catalog data was successfully read
    	*/
    bool readCustomCatalogs();

    /**
    	*@short Add a custom catalog to the program
    	*@p filename the name of the file containing the data to be read
    	*@return true if catalog was successfully added
    	*/
    bool addCatalog(const QString &filename);

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
    	*@short Read metadata about the catalog from its header
    	*@p lines QStringlist containing all of the lines in the custom catalog file
    	*@p Columns QStringList containing the column descriptors (created by this function)
    	*@p CatalogName The name of th catalog, as read from the header by this function
    	*@p CatalogPrefix The prefix string for naming objects in this catalog (read by this function)
    	*@p CatalogColor The color for drawing symbols of objects in this catalog (read by this function)
    	*@p CatalogEpoch The coordinate epoch for the catalog (read by this function)
    	*@p iStart The line number of the first non-header line in the data file (determined by this function)
    	*@p showerrs if true, parse errors will be logged and reported
    	*@p errs reference to the string list containing the parse errors encountered
    	*/
    bool parseCustomDataHeader( const QStringList &lines, QStringList &Columns,
                                int &iStart, bool showerrs, QStringList &errs);

    QString m_Filename;
    QString m_catName, m_catPrefix, m_catColor;
    float m_catEpoch;
    bool m_Showerrs;

    static QStringList m_Columns;
};

#endif
