#ifndef EKODEVICE_TELESCOPE_H
#define EKODEVICE_TELESCOPE_H

#include "ekos/genericdevice.h"

class INDI_D;

namespace EkoDevice
{

class Telescope : public GenericDevice
{
public:
    Telescope(INDI_D *idp);
};

} // namespace EkoDevice

#endif // EKODEVICE_TELESCOPE_H
