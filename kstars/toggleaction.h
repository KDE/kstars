/***************************************************************************
                          toggleaction.h  -  description
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

#ifndef KSTARS_TOGGLEACTION_H
#define KSTARS_TOGGLEACTION_H

/**@class ToggleAction
	*Subclass of Kaction that allows for the action to have a boolean state (on/off)
	*The class stores the current state, and changes the Action's iconset to reflect
	*the state.
	*@author Mark Hollomon
	*@version 1.0
	*/

#include <tqiconset.h>
#include <kaction.h>

class ToggleAction : public KAction {
	Q_OBJECT
		
	public:
		/**Constructor. */
		ToggleAction(const TQString& ontext, const TQIconSet& onpix, const TQString& offtext, const TQIconSet& offpix, int accel, const TQObject* receiver, const char* slot, TQObject* parent = 0, const char* name = 0 ) ;
		/**Constructor. Same as above, but without icons. */
		ToggleAction(const TQString& ontext, const TQString& offtext, int accel, const TQObject* receiver, const char* slot, TQObject* parent = 0, const char* name = 0 ) ;

		/**Sets the ToolTip text for the "on" state. 
			*@p tip the tooltip string
			*/
		void setOnToolTip(TQString tip);

		/**Sets the ToolTip text for the "off" state. 
			*@p tip the tooltip string
			*/
		void setOffToolTip(TQString tip);

	public slots:
		/**Put the Action in the "off" state.  Update the icon and tooltip. */
		void turnOff();
		/**Put the Action in the "on" state.  Update the icon and tooltip. */
		void turnOn();

	private:
		TQIconSet officon;
		TQIconSet onicon;
		TQString offcap;
		TQString oncap;
		TQString onTip;
		TQString offTip;
		bool state;
};


#endif
