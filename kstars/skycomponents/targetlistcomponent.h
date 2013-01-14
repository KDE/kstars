/***************************************************************************
               targetlistcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : Oct 14 2010 9:59 PM CDT
    copyright            : (C) 2010 Akarsh Simha
    email                : akarsh.simha@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef TARGETLISTCOMPONENT_H
#define TARGETLISTCOMPONENT_H

#include "skypainter.h"
#include "skycomponent.h"
#include "typedef.h"
#include "skyobject.h"

/**
 *@class TargetListComponent
 *@short Highlights objects present in certain lists by drawing "target" symbols around them.
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
 * the list. If the pointers are NULL, the symbols are always drawn,
 * but the labels are not drawn.
 *
 *@note This does not inherit from ListComponent because it is not
 * necessary. ListComponent has extra methods like objectNearest(),
 * which we don't want. Also, ListComponent maintains its own list,
 * whereas this class merely holds a pointer to a list that's
 * manipulated from elsewhere.
 */

class TargetListComponent : public SkyComponent {

 public:

    /**
     *@short Default constructor.
     */
    explicit TargetListComponent( SkyComposite *parent );

    /**
     *@short Constructor that sets up this target list
     */
    TargetListComponent( SkyComposite *parent, QList<SkyObject*> *objectList, QPen _pen, 
                         bool (*optionDrawSymbols)(void) = 0, bool (*optionDrawLabels)(void) = 0 );

    /**
     *@short Draw this component by iterating over the list.
     *
     *@note This method does not bother refreshing the coordinates of
     * the objects on the list. So this must be called only after the
     * objects are drawn in a given draw cycle.
     */
    virtual void draw( SkyPainter *skyp );

    // FIXME: Maybe we should make these member objects private / protected?
    SkyObjectList *list; // Pointer to list of objects to draw
    QPen pen; // Pen to use to draw

    /**
     *@short Pointer to static method that tells us whether to draw this list or not
     *@note If the pointer is NULL, the list is drawn nevertheless
     */
    bool (*drawSymbols)( void );

    /**
     *@short Pointer to static method that tells us whether to draw labels for this list or not
     *@note If the pointer is NULL, labels are not drawn
     */
    bool (*drawLabels)( void );

 protected:
    /**
     *@short Draws a target symbol around the object, and also draws labels if requested
     *@note Does not update the positions of the objects. See the note on draw() for details.
     */
    /*

    // This method is superseded by the definitions in SkyPainter
    // and might need to be reinstated only while generalizing the
    // class to draw other textures.

    virtual void drawTargetSymbol( SkyPainter *skyp, SkyObject *obj );
    */
};

#endif
