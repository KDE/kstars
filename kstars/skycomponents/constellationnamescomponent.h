/*
    SPDX-FileCopyrightText: 2005 Thomas Kabelmann <thomas.kabelmann@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "listcomponent.h"

class CultureList;
class KSNumbers;
class SkyComposite;
class SkyPainter;

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
    /**
     * @short Constructor
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
    ConstellationNamesComponent(SkyComposite *parent, CultureList *cultures);

    virtual ~ConstellationNamesComponent() override = default;

    /**
     * @short Draw constellation names on the sky map.
     * @p psky Reference to the QPainter on which to paint
     */
    void draw(SkyPainter *skyp) override;

    /**
     * @short we need a custom routine (for now) so we don't
     * precess the locations of the names.
     */
    void update(KSNumbers *num) override;

    /** @short Return true if we are using localized constellation names */
    inline bool isLocalCNames() { return localCNames; }

    bool selected() override;

    void loadData(CultureList *cultures);

  private:
    bool localCNames { false };
};
