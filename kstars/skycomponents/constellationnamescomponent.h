/***************************************************************************
                          constellationnamescomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/10/08
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

#ifndef CONSTELLATIONNAMESCOMPONENT_H
#define CONSTELLATIONNAMESCOMPONENT_H

#include <QList>
#include "listcomponent.h"

class SkyLabeler;
class CultureList;

/**
 * @class ConstellationNamesComponent
 * Represents the constellation names on the sky map.
 *
 * @author Thomas Kabelmann
 * @version 0.1
 */
class ConstellationNamesComponent : public ListComponent
{
public:
    /** @short Constructor
     * @p parent Pointer to the parent SkyComposite object
     *
     * Reads the constellation names data from cnames.dat
     * Each line in the file is parsed according to column position:
     * @li 0-1     RA hours [int]
     * @li 2-3     RA minutes [int]
     * @li 4-5     RA seconds [int]
     * @li 6       Dec sign [char; '+' or '-']
     * @li 7-8     Dec degrees [int]
     * @li 9-10    Dec minutes [int]
     * @li 11-12   Dec seconds [int]
     * @li 13-15   IAU Abbreviation [string]  e.g., 'Ori' == Orion
     * @li 17-     Constellation name [string]
     */
    ConstellationNamesComponent(SkyComposite *parent, CultureList* cultures);

    /** @short Destructor.  Delete list members */
    virtual ~ConstellationNamesComponent();

    /** @short Draw constellation names on the sky map.
     * @p psky Reference to the QPainter on which to paint
     */
    virtual void draw( SkyPainter *skyp );

    /** @short we need a custom routine (for now) so we don't
     * precess the locations of the names.
     */
    virtual void update( KSNumbers *num );

    /** @short Return true if we are using localized constellation names */
    inline bool isLocalCNames() { return localCNames; }

    virtual bool selected();

    void loadData(CultureList* cultures);

private:
    bool localCNames;
};

#endif
