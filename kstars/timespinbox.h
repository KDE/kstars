/***************************************************************************
                          timespinbox.h  -  description
                             -------------------
    begin                : Sun Mar 31 2002
    copyright            : (C) 2002 by Jason Harris
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

#ifndef TIMESPINBOX_H
#define TIMESPINBOX_H

#include <qspinbox.h>
#include <qstringlist.h>

/**@class TimeSpinBox 
	*Custom spinbox to handle selection of timestep values with variable units.
	*@note this should only be used internally, embedded in a TimeStepBox widget.
	*@author Jason Harris
	*@version 1.0
	*/

class TimeSpinBox : public QSpinBox  {
Q_OBJECT
public:
/**Constructor */
	TimeSpinBox( QWidget *parent, const char* name=0, bool daysOnly = false );
/**Destructor (empty)*/
	~TimeSpinBox() {}

	/**Convert the internal value to a display string.
		*@note reimplemented from QSpinBox
		*@p value the internal value to convert to a display string
		*@return the display string
		*/
	virtual QString mapValueToText( int value );
	
	/**Convert the displayed string to an internal value.
		*@note reimplemented from QSpinBox
		*@p ok bool pointer set to TRUE if conversion was successful
		*@return internal value converted from displayed text
		*/
	virtual int mapTextToValue( bool *ok);

	/**@return the current TimeStep setting */
	float timeScale() const;

	void setDaysOnly( bool daysonly );
	bool daysOnly() const { return DaysOnly; }

signals:
	void scaleChanged( float s );

public slots:
	void changeScale( float s );

protected slots:
	void reportChange();

private:
	bool DaysOnly;
	float TimeScale[42];
	QStringList TimeString;
};

#endif
