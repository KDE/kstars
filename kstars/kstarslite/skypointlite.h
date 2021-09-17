/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/
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

class SkyPointLite : public QObject
{
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
