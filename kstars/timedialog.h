/***************************************************************************
                          timedialog.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef TIMEDIALOG_H
#define TIMEDIALOG_H

#include <kdialogbase.h>
#include <tqvariant.h>

#include "kstarsdatetime.h"

class QHBoxLayout;
class QVBoxLayout;
class ExtDatePicker;
class QSpinBox;
class QLabel;
class QPushButton;
class KStars;

/**@class TimeDialog 
	*A class for adjusting the Time and Date.  Contains a ExtDatePicker widget
	*for selecting the date, and three QSpinBoxes for selecting the Hour,
	*Minute and Second.  There is also a "Now" QPushbutton for selecting the
	*Time and Date from the system clock.
	*@short Dialog for adjusting the Time and Date.
	*@author Jason Harris
	*@version 0.9
	*/

class TimeDialog : public KDialogBase {
  Q_OBJECT
public:
/**
	*Constructor.  Creates widgets and packs them into QLayouts.
	*Connects	Signals and Slots.
	*/
	TimeDialog( const KStarsDateTime &now, TQWidget* parent = 0 );

/**
	*Destructor (empty)
	*/
	~TimeDialog() {}

/**@returns a TQTime object with the selected time
	*/
	TQTime selectedTime( void );

/**@returns a ExtDate object with the selected date
	*/
	ExtDate selectedDate( void );

/**@returns a KStarsDateTime object with the selected date and time
	*/
	KStarsDateTime selectedDateTime( void );

public slots:
/**
	*When the "Now" TQPushButton is pressed, read the time and date
	*from the system clock.  Change the selected date in the ExtDatePicker
	*to the system's date, and the displayed Hour, Minute and Second
	*to the system time.
	*/
  void setNow( void );

/**
	*When the value of the HourBox TQSpinBox is changed, prefix a "0" to
	*the displayed text, if the value is less than 10.
	*
	*It would be nice if I could use one slot for these three widgets;
	*my understanding is that the slot has no knowledge of which
	*widget sent the signal...
	*/
	void HourPrefix( int value );

/**
	*When the value of the MinuteBox TQSpinBox is changed, prefix a "0" to
	*the displayed text, if the value is less than 10.
	*/
	void MinutePrefix( int value );

/**
	*When the value of the SecondBox TQSpinBox is changed, prefix a "0" to
	*the displayed text, if the value is less than 10.
	*/
	void SecondPrefix( int value );

protected:
	void keyReleaseEvent( TQKeyEvent* );

private:
  KStars *ksw;
  bool UTCNow;
  TQHBoxLayout *hlay;
  TQVBoxLayout *vlay;
  ExtDatePicker *dPicker;
  TQSpinBox* HourBox;
  TQLabel* TextLabel1;
  TQSpinBox* MinuteBox;
  TQLabel* TextLabel1_2;
  TQSpinBox* SecondBox;
  TQPushButton* NowButton;
};

#endif
