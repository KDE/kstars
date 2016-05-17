/** *************************************************************************
                          skyitem.h  -  K Desktop Planetarium
                             -------------------
    begin                : 02/05/2016
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

#ifndef SKYITEM_H_
#define SKYITEM_H_

#include <QSGNode>
#include <QQuickItem>
//#include "skymaplite.h"

class SkyComponent;
class Projector;
class SkyMapLite;
class QQuickItem;

/** @class SkyItem
 *
 *This is an interface for implementing SkyItems that are used to display SkyComponent
 *derived objects on the SkyMapLite.
 *
 *@short A base class that is used for displaying SkyComponents on SkyMapLite.
 *@author Artem Fedoskin
 *@version 1.0
 */

class SkyItem : public QQuickItem {

    Q_OBJECT

protected:
   /**
    *Constructor, initializes m_parentComponent (a pointer to SkyComponent, which asked to initialize
    * this SkyItem).
    *
    * @param parent a pointer to SkyItem's data and visual parent
    */
    /* @param parentComponent a pointer to SkyComponent, which asked to initialize this SkyItem*/

    explicit SkyItem(QQuickItem* parent = 0);

public:
    /* @short Get the component that asked to instantiate this SkyItem
     *
     *@return a pointer to the parent component.

    SkyComponent* getParentComponent() const { return m_parentComponent; }
    */
    /*/** @short Set the m_parentComponent pointer to the argument.
     *@param component pointer to the SkyComponent derived object to be assigned as the m_parentComponent
    void setParentComponent( SkyComponent *component ) { m_parentComponent = component; }     */

public slots:
    /** Called whenever parent's width or height are changed.
     */
    void resizeItem();

private:
    SkyMapLite* m_skyMapLite;
    //SkyComponent* m_parentComponent;
};

#endif
