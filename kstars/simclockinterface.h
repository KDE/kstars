#ifndef KSTARS_SIMCLOCKIF_H_
#define KSTARS_SIMCLOCKIF_H_


#include <qdatetime.h>
#include <dcopobject.h>

class SimClockInterface : virtual public DCOPObject {
	K_DCOP

	k_dcop:
		virtual ASYNC stop() = 0;
		virtual ASYNC start() = 0;
		virtual ASYNC setUTC(const QDateTime &newtime) = 0;
		virtual ASYNC setClockScale(float s) = 0;
};

#endif
