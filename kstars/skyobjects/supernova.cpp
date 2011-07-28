#include "supernova.h"

Supernova::Supernova(dms ra, dms dec, QString& date ,float m, const QString& serialNo,
                     const QString& type, const QString& hostGalaxy, const QString& offset,
                     const QString& discoverer)
                    : SkyObject(SkyObject::SUPERNOVA,ra, dec, m, serialNo), RA(ra),
                    Dec(dec),date(date), Magnitude(m),serialNumber(serialNo), type(type), hostGalaxy(hostGalaxy),
                    offset(offset), discoverers(discoverer)

{}