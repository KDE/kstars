/** *************************************************************************
                          skyobjectlite.h  -  K Desktop Planetarium
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
#ifndef SKYOBJECTLITE_H_
#define SKYOBJECTLITE_H_

#include "skyobject.h"
#include "skypointlite.h"
#include <QObject>

    /**
     * @class SkyPointLite
     * Wrapper for SkyPoint to allow access of some of its properties from QML
     *
     * @author Artem Fedoskin
     * @version 1.0
     */
class SkyObjectLite : public SkyPointLite {
    Q_OBJECT
    Q_PROPERTY(QString translatedName READ getTranslatedName)
public:
    SkyObjectLite();
    void setObject(SkyObject *object);
    Q_INVOKABLE QString getTranslatedName();
private:
    SkyObject *object;
};
#endif


