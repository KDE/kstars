/***************************************************************************
                          kstarsinterface.h  -  K Desktop Planetarium
                             -------------------
    begin                : Thu Jan 3 2002
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




#ifndef KSTARSINTERFACE_H
#define KSTARSINTERFACE_H

#include <dcopobject.h>

/**@short the DCOP interface functions related to the KStars class.
  *@author Mark Hollomon
	*@version 0.9
  */


class KStarsInterface : virtual public DCOPObject
{
	K_DCOP

	k_dcop:
	/**DCOP interface function.
		*Point in the direction described by the string argument.  
		*@param direction the object or direction to foxus on
		*/
		virtual ASYNC lookTowards(QString direction) = 0;
		
	/**DCOP interface function.  Zoom in. */
		virtual ASYNC zoomIn() = 0;
		
	/**DCOP interface function.  Zoom out. */
		virtual ASYNC zoomOut() = 0;
		
	/**DCOP interface function.  Set focus to given Alt/Az coordinates. 
		*@param alt new focus Altiude 
		*@param az  new focus Azimuth 
		*/
		virtual ASYNC setAltAz(double alt, double az) = 0;
		
	/**DCOP interface function.  Set local time and date. 
		*@param yr Year portion of new date
		*@param mth Month portion of new date
		*@param day Day portion of new date
		*@param hr Hour portion of new time
		*@param min Minute portion of new time
		*@param sec Second portion of new time
		*/
		virtual ASYNC setLocalTime(int yr, int mth, int day, int hr, int min, int sec) = 0;
};

#endif
