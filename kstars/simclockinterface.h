/***************************************************************************
                          simclockinterface.h  -  description
                             -------------------
    begin                : Mon Feb 18 2002
    copyright          : (C) 2002 by Mark Hollomon
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

#ifndef KSTARS_SIMCLOCKIF_H_
#define KSTARS_SIMCLOCKIF_H_

#include <dcopobject.h>

#include "kstarsdatetime.h"

/**@class SimclockInterface
	*DCOP functions for the KSTars simulation clock.
	*@author Mark Hollomon
	*@version 1.0
	*/

class SimClockInterface : virtual public DCOPObject {
	K_DCOP

	k_dcop:
		/**Stop the clock
			*/
		virtual ASYNC stop() = 0;
		
		/**Start the clock
			*/
		virtual ASYNC start() = 0;
		
		/**Set the clock to the given ExtDateTime value.
			*@p newtime the time/date to adopt
			*/
		virtual ASYNC setUTC(const KStarsDateTime &newtime) = 0;
		
		/**Set the clock scale (the number of simulation seconds that 
			*pass per real-time second.
			*@p s the new timescale factor
			*/
		virtual ASYNC setClockScale(float s) = 0;
};

#endif
