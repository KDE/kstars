#ifndef EKODEVICE_FOCUSER_H
#define EKODEVICE_FOCUSER_H

#include "ekos/genericdevice.h"

class INDI_D;

namespace EkoDevice
{

class Focuser : public GenericDevice
{
public:
    Focuser(INDI_D *);
};

} // namespace EkoDevice

#endif // EKODEVICE_FOCUSER_H
