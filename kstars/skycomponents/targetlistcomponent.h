/*
    SPDX-FileCopyrightText: 2010 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skycomponent.h"

#include <QPen>

/**
 * @class TargetListComponent
 * @short Highlights objects present in certain lists by drawing "target" symbols around them.
 *
 * To represent lists of specific objects on the skymap (eg: A star
 * hopping route, or a list of planned observation targets), one would
 * typically like to draw over the skymap, a bunch of symbols
 * highlighting the locations of these objects. This class manages
 * drawing of such lists. Along with the list, we also associate a pen
 * to use to represent objects from that list. Once this class is made
 * aware of a list to plot (which is stored somewhere else), it does
 * so when called from the SkyMapComposite. The class may be supplied
 * with pointers to two methods that tell it whether to draw symbols /
 * labels or not. Disabling symbol drawing is equivalent to disabling
 * the list. If the pointers are nullptr, the symbols are always drawn,
 * but the labels are not drawn.
 *
 * @note This does not inherit from ListComponent because it is not
 * necessary. ListComponent has extra methods like objectNearest(),
 * which we don't want. Also, ListComponent maintains its own list,
 * whereas this class merely holds a pointer to a list that's
 * manipulated from elsewhere.
 */

class TargetListComponent : public SkyComponent
{
  public:
    /**
     * @short Default constructor.
     */
    explicit TargetListComponent(SkyComposite *parent);

    /**
     * @short Constructor that sets up this target list
     */
    TargetListComponent(SkyComposite *parent, QList<SkyObject *> *objectList, QPen _pen,
                        bool (*optionDrawSymbols)(void) = nullptr, bool (*optionDrawLabels)(void) = nullptr);

    ~TargetListComponent() override;

    /**
     * @short Draw this component by iterating over the list.
     *
     * @note This method does not bother refreshing the coordinates of
     * the objects on the list. So this must be called only after the
     * objects are drawn in a given draw cycle.
     */
    void draw(SkyPainter *skyp) override;

    // FIXME: Maybe we should make these member objects private / protected?
    /// Pointer to list of objects to draw
    std::unique_ptr<SkyObjectList> list;
    /// Pointer to list of objects to draw
    QList<QSharedPointer<SkyObject>> list2;
    /// Pen to use to draw
    QPen pen;

    /**
     * @short Pointer to static method that tells us whether to draw this list or not
     * @note If the pointer is nullptr, the list is drawn nevertheless
     */
    bool (*drawSymbols)(void);

    /**
     * @short Pointer to static method that tells us whether to draw labels for this list or not
     * @note If the pointer is nullptr, labels are not drawn
     */
    bool (*drawLabels)(void);
};
