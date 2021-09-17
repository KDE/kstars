/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyobjects/skyobject.h"

#include <QList>
#include <QPointF>

class SkyLabel;
typedef QList<SkyLabel> LabelList;

class SkyLabel
{
  public:
    SkyLabel(qreal ra, qreal dec, SkyObject *obj_in) : o(ra, dec), obj(obj_in) {}

    //        SkyLabel( double ra, double dec, QString& text_in) :
    //            o( ra, dec), text(text_in)
    //        {}

    SkyLabel(const QPointF o_in, SkyObject *obj_in) : o(o_in), obj(obj_in) {}

    //~StarLabel() { delete m_p; }

    QPointF &point() { return o; }
    QPointF o;
    SkyObject *obj;
};
