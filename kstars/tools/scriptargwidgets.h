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

#ifndef SCRIPTARGWIDGETS_H
#define SCRIPTARGWIDGETS_H

#include "arglooktoward.h"
#include "argsetradec.h"
#include "argsetaltaz.h"
#include "argsetlocaltime.h"
#include "argwaitfor.h"
#include "argwaitforkey.h"
#include "argsettrack.h"
#include "argchangeviewoption.h"
#include "argsetgeolocation.h"
#include "argtimescale.h"
#include "argzoom.h"
#include "argexportimage.h"
#include "argprintimage.h"
#include "argsetcolor.h"
#include "argloadcolorscheme.h"
#include "argstartindi.h"
#include "argshutdownindi.h"
#include "argswitchindi.h"
#include "argsetportindi.h"
#include "argsettargetcoordindi.h"
#include "argsettargetnameindi.h"
#include "argsetactionindi.h"
#include "argsetfocusspeedindi.h"
#include "argstartfocusindi.h"
#include "argsetfocustimeoutindi.h"
#include "argsetgeolocationindi.h"
#include "argstartexposureindi.h"
#include "argsetutcindi.h"
#include "argsetscopeactionindi.h"
#include "argsetframetypeindi.h"
#include "argsetccdtempindi.h"
#include "argsetfilternumindi.h"

class ArgLookToward  : public QFrame, public Ui::ArgLookToward {
Q_OBJECT
  public:
 ArgLookToward ( QWidget *p );
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
 
class ArgStartINDI  : public QFrame, public Ui::ArgStartINDI {
Q_OBJECT
  public:
 ArgStartINDI ( QWidget *p );
};
 
class ArgShutdownINDI  : public QFrame, public Ui::ArgShutdownINDI {
Q_OBJECT
  public:
 ArgShutdownINDI ( QWidget *p );
};
 
class ArgSwitchINDI  : public QFrame, public Ui::ArgSwitchINDI {
Q_OBJECT
  public:
 ArgSwitchINDI ( QWidget *p );
};
 
class ArgSetPortINDI  : public QFrame, public Ui::ArgSetPortINDI {
Q_OBJECT
  public:
 ArgSetPortINDI ( QWidget *p );
};
 
class ArgSetTargetCoordINDI  : public QFrame, public Ui::ArgSetTargetCoordINDI {
Q_OBJECT
  public:
 ArgSetTargetCoordINDI ( QWidget *p );
};
 
class ArgSetTargetNameINDI  : public QFrame, public Ui::ArgSetTargetNameINDI {
Q_OBJECT
  public:
 ArgSetTargetNameINDI ( QWidget *p );
};
 
class ArgSetActionINDI  : public QFrame, public Ui::ArgSetActionINDI {
Q_OBJECT
  public:
 ArgSetActionINDI ( QWidget *p );
};
 
class ArgSetFocusSpeedINDI  : public QFrame, public Ui::ArgSetFocusSpeedINDI {
Q_OBJECT
  public:
 ArgSetFocusSpeedINDI ( QWidget *p );
};
 
class ArgStartFocusINDI  : public QFrame, public Ui::ArgStartFocusINDI {
Q_OBJECT
  public:
 ArgStartFocusINDI ( QWidget *p );
};
 
class ArgSetFocusTimeoutINDI  : public QFrame, public Ui::ArgSetFocusTimeoutINDI {
Q_OBJECT
  public:
 ArgSetFocusTimeoutINDI ( QWidget *p );
};
 
class ArgSetGeoLocationINDI  : public QFrame, public Ui::ArgSetGeoLocationINDI {
Q_OBJECT
  public:
 ArgSetGeoLocationINDI ( QWidget *p );
};
 
class ArgStartExposureINDI  : public QFrame, public Ui::ArgStartExposureINDI {
Q_OBJECT
  public:
 ArgStartExposureINDI ( QWidget *p );
};
 
class ArgSetUTCINDI  : public QFrame, public Ui::ArgSetUTCINDI {
Q_OBJECT
  public:
 ArgSetUTCINDI ( QWidget *p );
};
 
class ArgSetScopeActionINDI  : public QFrame, public Ui::ArgSetScopeActionINDI {
Q_OBJECT
  public:
 ArgSetScopeActionINDI ( QWidget *p );
};
 
class ArgSetFrameTypeINDI  : public QFrame, public Ui::ArgSetFrameTypeINDI {
Q_OBJECT
  public:
 ArgSetFrameTypeINDI ( QWidget *p );
};
 
class ArgSetCCDTempINDI  : public QFrame, public Ui::ArgSetCCDTempINDI {
Q_OBJECT
  public:
 ArgSetCCDTempINDI ( QWidget *p );
};
 
class ArgSetFilterNumINDI  : public QFrame, public Ui::ArgSetFilterNumINDI {
Q_OBJECT
  public:
 ArgSetFilterNumINDI ( QWidget *p );
};
 
#endif
