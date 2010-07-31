/***************************************************************************
                          toggleaction.cpp  -  description
                             -------------------
    begin                : Son Feb 10 2002
    copyright            : (C) 2002 by Mark Hollomon
    email                : mhh@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "toggleaction.h"

ToggleAction::ToggleAction(const TQString& ontext, const TQIconSet& onpix,
	const TQString& offtext, const TQIconSet& offpix,
	int accel, const TQObject* receiver, const char* slot, TQObject* parent, const char* name ) :
		KAction(ontext, onpix, accel, receiver, slot, parent, name),
		officon(offpix),
		onicon(onpix),
		offcap(offtext),
		oncap(ontext),
		state(true)
{}

ToggleAction::ToggleAction(const TQString& ontext, const TQString& offtext,
	int accel, const TQObject* receiver, const char* slot, TQObject* parent, const char* name ) :
		KAction(ontext, accel, receiver, slot, parent, name),
		officon(),
		onicon(),
		offcap(offtext),
		oncap(ontext),
		state(true)
{}

void ToggleAction::turnOff() {
	if ( !officon.isNull() ) setIconSet(officon);
	setText(offcap);
	setToolTip(offTip);
	state = false;
}

void ToggleAction::turnOn() {
	if ( !onicon.isNull() ) setIconSet(onicon);
	setText(oncap);
	setToolTip(onTip);
	state = true;
}

void ToggleAction::setOnToolTip(TQString tip) {
	onTip = tip;
	if (state)
		setToolTip(tip);
}

void ToggleAction::setOffToolTip(TQString tip) {
	offTip = tip;
	if (!state)
		setToolTip(tip);
}

#include "toggleaction.moc"
