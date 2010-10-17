/***************************************************************************
                          scriptargwidgets.cpp  -  description
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

#include "scriptargwidgets.h"

ArgLookToward::ArgLookToward ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgFindObject::ArgFindObject ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetRaDec::ArgSetRaDec ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetAltAz::ArgSetAltAz ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetLocalTime::ArgSetLocalTime ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgWaitFor::ArgWaitFor ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgWaitForKey::ArgWaitForKey ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetTrack::ArgSetTrack ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgChangeViewOption::ArgChangeViewOption ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetGeoLocation::ArgSetGeoLocation ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgTimeScale::ArgTimeScale ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgZoom::ArgZoom ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgExportImage::ArgExportImage ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgPrintImage::ArgPrintImage ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetColor::ArgSetColor ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgLoadColorScheme::ArgLoadColorScheme ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgStartINDI::ArgStartINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetDeviceINDI::ArgSetDeviceINDI(QWidget *p) : QFrame( p ) {
    setupUi( this);
}

ArgShutdownINDI::ArgShutdownINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSwitchINDI::ArgSwitchINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetPortINDI::ArgSetPortINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetTargetCoordINDI::ArgSetTargetCoordINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetTargetNameINDI::ArgSetTargetNameINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetActionINDI::ArgSetActionINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetFocusSpeedINDI::ArgSetFocusSpeedINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgStartFocusINDI::ArgStartFocusINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetFocusTimeoutINDI::ArgSetFocusTimeoutINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetGeoLocationINDI::ArgSetGeoLocationINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgStartExposureINDI::ArgStartExposureINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetUTCINDI::ArgSetUTCINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetScopeActionINDI::ArgSetScopeActionINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetFrameTypeINDI::ArgSetFrameTypeINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetCCDTempINDI::ArgSetCCDTempINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ArgSetFilterNumINDI::ArgSetFilterNumINDI ( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

#include "scriptargwidgets.moc"

