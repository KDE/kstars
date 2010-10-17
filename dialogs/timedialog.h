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

#ifndef TIMEDIALOG_H_
#define TIMEDIALOG_H_

#include <kdialog.h>
#include <qvariant.h>
//Added by qt3to4:
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>

#include "kstarsdatetime.h"

class QHBoxLayout;
class QVBoxLayout;
class KDatePicker;
class QTimeEdit;
class KPushButton;
class GeoLocation;

/**@class TimeDialog
  *A class for adjusting the Time and Date.  Contains a KDatePicker widget
  *for selecting the date, and a QTimeEdit for selecting the time.  There 
  *is also a "Now" button for selecting the Time and Date from the system clock.
  *@short Dialog for adjusting the Time and Date.
  *@author Jason Harris
  *@version 1.0
  */

class TimeDialog : public KDialog {
    Q_OBJECT
public:
    /**
    	*Constructor.  Creates widgets and packs them into QLayouts.
    	*Connects	Signals and Slots.
    	*/
    TimeDialog( const KStarsDateTime &now, GeoLocation *_geo, QWidget *parent, bool UTCFrame=false );

    /**
    	*Destructor (empty)
    	*/
    ~TimeDialog() {}

    /**@returns a QTime object with the selected time
    	*/
    QTime selectedTime( void );

    /**@returns a QDate object with the selected date
    	*/
    QDate selectedDate( void );

    /**@returns a KStarsDateTime object with the selected date and time
    	*/
    KStarsDateTime selectedDateTime( void );

public slots:
    /**
    	*When the "Now" button is pressed, read the time and date
    	*from the system clock.  Change the selected date in the KDatePicker
    	*to the system's date, and the displayed time
    	*to the system time.
    	*/
    void setNow( void );

protected:
    void keyReleaseEvent( QKeyEvent* );

private:
    bool UTCNow;
    QHBoxLayout *hlay;
    QVBoxLayout *vlay;
    KDatePicker *dPicker;
    QTimeEdit *tEdit;
    KPushButton* NowButton;
    GeoLocation *geo;
};

#endif
