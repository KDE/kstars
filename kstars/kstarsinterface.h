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

/**
  *@author Mark Hollomon
  */


class KStarsInterface : virtual public DCOPObject
{
	K_DCOP

	k_dcop:
		virtual ASYNC lookTowards(QString direction) = 0;
		virtual ASYNC zoomIn() = 0;
		virtual ASYNC zoomOut() = 0;
		virtual ASYNC setAltAz(double alt, double az) = 0;
		virtual ASYNC setLocalTime(int yr, int mth, int day, int hr, int min, int sec) = 0;
};

#endif
