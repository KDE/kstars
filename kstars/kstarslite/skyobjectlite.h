/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef SKYOBJECTLITE_H_
#define SKYOBJECTLITE_H_

#include "skyobject.h"
#include "skypointlite.h"
#include <QObject>

/**
* @class SkyObjectLite
* Wrapper for SkyObject to allow access of some of its properties from QML
*
* @author Artem Fedoskin
* @version 1.0
*/

class SkyObjectLite : public SkyPointLite
{
    Q_OBJECT
    Q_PROPERTY(QString translatedName READ getTranslatedName NOTIFY translatedNameChanged)
  public:
    /** Constructor **/
    SkyObjectLite();

    /** @short sets SkyObject that is needed to be wrapped **/
    void setObject(SkyObject *object);

    /** @return translated name of currently wrapped SkyObject **/
    Q_INVOKABLE QString getTranslatedName();

    /** @return SkyObject that is being wrapped **/
    SkyObject *getObject() const { return object; }
  signals:
    void translatedNameChanged(QString translatedName);

  private:
    SkyObject *object;
};
#endif
