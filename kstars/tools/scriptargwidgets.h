/***************************************************************************
                          scriptargwidgets.h  -  description
                             -------------------
    begin                : Wed 04 January 2006
    copyright            : (C) 2006 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SCRIPTARGWIDGETS_H_
#define SCRIPTARGWIDGETS_H_

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

class ArgLookToward  : public QFrame, public Ui::ArgLookToward {
    Q_OBJECT
public:
    ArgLookToward ( QWidget *p );
};

class ArgFindObject  : public QFrame, public Ui::ArgFindObject {
    Q_OBJECT
public:
    ArgFindObject ( QWidget *p );
};

class ArgSetRaDec  : public QFrame, public Ui::ArgSetRaDec {
    Q_OBJECT
public:
    ArgSetRaDec ( QWidget *p );
};

class ArgSetAltAz  : public QFrame, public Ui::ArgSetAltAz {
    Q_OBJECT
public:
    ArgSetAltAz ( QWidget *p );
};

class ArgSetLocalTime  : public QFrame, public Ui::ArgSetLocalTime {
    Q_OBJECT
public:
    ArgSetLocalTime ( QWidget *p );
};

class ArgWaitFor  : public QFrame, public Ui::ArgWaitFor {
    Q_OBJECT
public:
    ArgWaitFor ( QWidget *p );
};

class ArgWaitForKey  : public QFrame, public Ui::ArgWaitForKey {
    Q_OBJECT
public:
    ArgWaitForKey ( QWidget *p );
};

class ArgSetTrack  : public QFrame, public Ui::ArgSetTrack {
    Q_OBJECT
public:
    ArgSetTrack ( QWidget *p );
};

class ArgChangeViewOption  : public QFrame, public Ui::ArgChangeViewOption {
    Q_OBJECT
public:
    ArgChangeViewOption ( QWidget *p );
};

class ArgSetGeoLocation  : public QFrame, public Ui::ArgSetGeoLocation {
    Q_OBJECT
public:
    ArgSetGeoLocation ( QWidget *p );
};

class ArgTimeScale  : public QFrame, public Ui::ArgTimeScale {
    Q_OBJECT
public:
    ArgTimeScale ( QWidget *p );
};

class ArgZoom  : public QFrame, public Ui::ArgZoom {
    Q_OBJECT
public:
    ArgZoom ( QWidget *p );
};

class ArgExportImage  : public QFrame, public Ui::ArgExportImage {
    Q_OBJECT
public:
    ArgExportImage ( QWidget *p );
};

class ArgPrintImage  : public QFrame, public Ui::ArgPrintImage {
    Q_OBJECT
public:
    ArgPrintImage ( QWidget *p );
};

class ArgSetColor  : public QFrame, public Ui::ArgSetColor {
    Q_OBJECT
public:
    ArgSetColor ( QWidget *p );
};

class ArgLoadColorScheme  : public QFrame, public Ui::ArgLoadColorScheme {
    Q_OBJECT
public:
    ArgLoadColorScheme ( QWidget *p );
};

#endif
