#ifndef PLANETCATALOG_H
#define PLANETCATALOG_H

#include <qlist.h>
#include <qstring.h>
#include "ksplanetbase.h"
#include "ksplanet.h"
#include "kssun.h"
#include "skyobjectname.h"
#include "objectnamelist.h"
/*
 * @author Mark Hollomon
 */


class PlanetCatalog : public QObject {
	Q_OBJECT

	public:
		PlanetCatalog();
		~PlanetCatalog();
		bool initialize();
		void addObject( ObjectNameList &ObjNames ) const;
		void findPosition( const KSNumbers *num);
		const KSSun *sun() const { return Sun; };
		void EquatorialToHorizontal( const dms LST, const dms lat );
		bool isPlanet(SkyObject *so) const;
		KSPlanetBase *findByName( const QString n) const;
		SkyObject *findClosest(const SkyPoint *p, double &r) const;



	private:
		QList<KSPlanetBase> planets;
		KSPlanet *Earth;
		KSSun *Sun;

};

#endif
