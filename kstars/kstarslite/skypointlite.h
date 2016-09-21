/** *************************************************************************
                          skypointlite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 20/07/2016
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
#ifndef SKYPOINTLITE_H_
#define SKYPOINTLITE_H_

#include "skypoint.h"
#include <QObject>

class SkyObject;

    /**
     * @class SkyPointLite
     * Wrapper for SkyPoint to allow access of some of its properties from QML
     *
     * @author Artem Fedoskin
     * @version 1.0
     */

class SkyPointLite : public QObject {
    Q_OBJECT
public:
    /** Constructor **/
    SkyPointLite();

    /** @short sets SkyPoint that is needed to be wrapped **/
    void setPoint(SkyPoint *point);

    /** @return SkyPoint that is being wrapped **/
    Q_INVOKABLE SkyPoint *getPoint() { return point; }

private:
    SkyPoint *point;
};
#endif

