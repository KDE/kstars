#include "toggleaction.h"

ToggleAction::ToggleAction(const QString& ontext, const QIconSet& onpix,
	const QString& offtext, const QIconSet& offpix,
	int accel, const QObject* receiver, const char* slot, QObject* parent, const char* name ) :
		KAction(ontext, onpix, accel, receiver, slot, parent, name),
		officon(offpix),
		onicon(onpix),
		offcap(offtext),
		oncap(ontext),
		state(true)
{}

ToggleAction::ToggleAction(const QString& ontext, const QString& offtext,
	int accel, const QObject* receiver, const char* slot, QObject* parent, const char* name ) :
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

void ToggleAction::setOnToolTip(QString tip) {
	onTip = tip;
	if (state)
		setToolTip(tip);
}

void ToggleAction::setOffToolTip(QString tip) {
	offTip = tip;
	if (!state)
		setToolTip(tip);
}

#include "toggleaction.moc"
