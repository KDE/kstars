/** *************************************************************************
                          constellationnamesitem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 10/06/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef CONSTELLATIONNAMESITEM_H_
#define CONSTELLATIONNAMESITEM_H_

#include <QSGOpacityNode>
#include "skyitem.h"

class ConstellationNamesComponent;

/**
 * @short The ConstellationName struct. It holds SkyObject (coordinate of label) and 2 labels (latin and
 * secondary names)
 */

struct ConstellationName {
    ConstellationName(SkyObject *skyObj);

    SkyObject *obj;
    LabelNode *latin;
    LabelNode *secondary;

    /**
     * @short hides both latin and secondary labels
     */
    void hide();
};

/** @class ConstellationNamesItem
 *
 * This class handles representation of ConstellationNamesComponent in SkyMapLite
 * @author Artem Fedoskin
 * @version 1.0
 */

class ConstellationNamesItem : public SkyItem {
public:
   /**
    * Constructor.
    * @param constComp a pointer to ConstellationNamesComponent that this object has to represent in
    * SkyMapLite
    * @param parent a pointer to SkyItem's parent node
    */
    explicit ConstellationNamesItem(ConstellationNamesComponent *constComp, RootNode *rootNode = 0);

    /**
     * Updates positions of labels of ConstellationName based on user settings and labels
     * visibility. If the user chose latin names then label with secondary name won't be created until
     * settings are not changed.
     *
     * @short updates positions of labels based on their visibility.
     */
    virtual void update();

    /**
     * @short deletes all labels and recreates m_names according to object list of m_constelNamesComp
     */
    void recreateList();
private:
    ConstellationNamesComponent *m_constelNamesComp;
    QVector<ConstellationName *> m_names;
};

#endif
