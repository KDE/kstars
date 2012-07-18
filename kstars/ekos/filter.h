#ifndef EKODEVICE_FILTER_H
#define EKODEVICE_FILTER_H

#include "ekos/genericdevice.h"

class INDI_D;

namespace EkoDevice
{

class Filter : public GenericDevice
{
public:
    Filter(INDI_D *dp);
};

} // namespace EkoDevice

#endif // EKODEVICE_FILTER_H
