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

/**Custom spinbox to handle selection of timestep values with variable units.
	*@author Jason Harris
	*@version 0.9
  */

class TimeSpinBox : public QSpinBox  {
Q_OBJECT
public:
/**Constructor */
	TimeSpinBox( QWidget *parent, const char* name=0 );
/**Destructor (empty)*/
	~TimeSpinBox() {};

	virtual QString mapValueToText( int value );
	virtual int mapTextToValue( bool *ok);

	/**@returns the current TimeStep setting */
	float timeScale() const;

signals:
	void scaleChanged( float s );

public slots:
	void changeScale( float s );

protected slots:
	void reportChange();

private:
	float TimeScale[42];
	QStringList TimeString;
};

#endif
