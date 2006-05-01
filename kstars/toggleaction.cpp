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

ToggleAction::ToggleAction( const KIcon &_onicon,  const QString& ontext,
		const KIcon &_officon, const QString& offtext,
		const KShortcut &accel, const QObject* receiver,
		const char* slot, KActionCollection* parent,
		const QString &name )
: KAction( _onicon, ontext, parent, name ),
		officon(_officon),
		onicon(_onicon),
		offcap(offtext),
		oncap(ontext),
		state(true)
{
	setShortcut( accel );
	if ( slot && receiver )
		connect( this, SIGNAL( triggered() ), receiver, slot );
}

ToggleAction::ToggleAction(const QString& ontext, const QString& offtext,
		const KShortcut &accel, const QObject* receiver,
		const char* slot, KActionCollection* parent,
		const QString &name )
: KAction(ontext, parent, name),
		officon(),
		onicon(),
		offcap(offtext),
		oncap(ontext),
		state(true)
{
	setShortcut( accel );
	if (  slot && receiver )
		connect( this, SIGNAL( triggered() ), receiver, slot );
}

void ToggleAction::turnOff() {
	// FIXME use KIcon only
	//if ( !officon.isNull() ) setIcon(officon);
	setText(offcap);
	setToolTip(offTip);
	state = false;
}

void ToggleAction::turnOn() {
	// FIXME use KIcon only
	//if ( !onicon.isNull() ) setIcon(onicon);
	setText(oncap);
	setToolTip(onTip);
	state = true;
}

void ToggleAction::setOnToolTip(const QString &tip) {
	onTip = tip;
	if (state)
		setToolTip(tip);
}

void ToggleAction::setOffToolTip(const QString &tip) {
	offTip = tip;
	if (!state)
		setToolTip(tip);
}

#include "toggleaction.moc"
