/*
    SPDX-FileCopyrightText: 2006 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_arglooktoward.h"
#include "ui_argfindobject.h"
#include "ui_argsetradec.h"
#include "ui_argsetaltaz.h"
#include "ui_argsetlocaltime.h"
#include "ui_argwaitfor.h"
#include "ui_argwaitforkey.h"
#include "ui_argsettrack.h"
#include "ui_argchangeviewoption.h"
#include "ui_argsetgeolocation.h"
#include "ui_argtimescale.h"
#include "ui_argzoom.h"
#include "ui_argexportimage.h"
#include "ui_argprintimage.h"
#include "ui_argsetcolor.h"
#include "ui_argloadcolorscheme.h"

class ArgLookToward : public QFrame, public Ui::ArgLookToward
{
    Q_OBJECT

  public:
    explicit ArgLookToward(QWidget *p);
};

class ArgFindObject : public QFrame, public Ui::ArgFindObject
{
    Q_OBJECT

  public:
    explicit ArgFindObject(QWidget *p);
};

class ArgSetRaDec : public QFrame, public Ui::ArgSetRaDec
{
    Q_OBJECT

  public:
    explicit ArgSetRaDec(QWidget *p);
};

class ArgSetAltAz : public QFrame, public Ui::ArgSetAltAz
{
    Q_OBJECT

  public:
    explicit ArgSetAltAz(QWidget *p);
};

class ArgSetLocalTime : public QFrame, public Ui::ArgSetLocalTime
{
    Q_OBJECT

  public:
    explicit ArgSetLocalTime(QWidget *p);
};

class ArgWaitFor : public QFrame, public Ui::ArgWaitFor
{
    Q_OBJECT

  public:
    explicit ArgWaitFor(QWidget *p);
};

class ArgWaitForKey : public QFrame, public Ui::ArgWaitForKey
{
    Q_OBJECT

  public:
    explicit ArgWaitForKey(QWidget *p);
};

class ArgSetTrack : public QFrame, public Ui::ArgSetTrack
{
    Q_OBJECT

  public:
    explicit ArgSetTrack(QWidget *p);
};

class ArgChangeViewOption : public QFrame, public Ui::ArgChangeViewOption
{
    Q_OBJECT

  public:
    explicit ArgChangeViewOption(QWidget *p);
};

class ArgSetGeoLocation : public QFrame, public Ui::ArgSetGeoLocation
{
    Q_OBJECT

  public:
    explicit ArgSetGeoLocation(QWidget *p);
};

class ArgTimeScale : public QFrame, public Ui::ArgTimeScale
{
    Q_OBJECT

  public:
    explicit ArgTimeScale(QWidget *p);
};

class ArgZoom : public QFrame, public Ui::ArgZoom
{
    Q_OBJECT

  public:
    explicit ArgZoom(QWidget *p);
};

class ArgExportImage : public QFrame, public Ui::ArgExportImage
{
    Q_OBJECT

  public:
    explicit ArgExportImage(QWidget *p);
};

class ArgPrintImage : public QFrame, public Ui::ArgPrintImage
{
    Q_OBJECT

  public:
    explicit ArgPrintImage(QWidget *p);
};

class ArgSetColor : public QFrame, public Ui::ArgSetColor
{
    Q_OBJECT

  public:
    explicit ArgSetColor(QWidget *p);
};

class ArgLoadColorScheme : public QFrame, public Ui::ArgLoadColorScheme
{
    Q_OBJECT

  public:
    explicit ArgLoadColorScheme(QWidget *p);
};
