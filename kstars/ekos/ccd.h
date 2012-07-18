#ifndef EKODEVICE_CCD_H
#define EKODEVICE_CCD_H

#include "ekos/genericdevice.h"

class INDI_D;

namespace EkoDevice
{

class CCD : public GenericDevice
{
    Q_OBJECT

public:
    CCD(INDI_D *dp);
};

} // namespace EkoDevice

#endif // EKODEVICE_CCD_H
