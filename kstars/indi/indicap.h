/*  INDI Cap
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef INDICAP_H
#define INDICAP_H

#include "indistd.h"

namespace ISD
{

/**
 * @class Cap
 * Handles operation of a remotely controlled dust cover cap.
 *
 * @author Jasem Mutlaq
 */

class DustCap : public DeviceDecorator
{
    Q_OBJECT

public:
    DustCap(GDInterface *iPtr) : DeviceDecorator(iPtr) { dType = KSTARS_AUXILIARY;}

    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty* tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processLight(ILightVectorProperty *lvp);

    DeviceFamily getType() { return dType;}

    virtual bool hasLight();
    virtual bool canPark();
    virtual bool isLightOn();
    virtual bool isParked();

public slots:
    /**
     * @brief SetBrightness Set light box brightness levels if dimmable.
     * @param val Value of brightness level.
     * @return True if operation is successful, false otherwise.
     */
    bool SetBrightness(uint16_t val);

    /**
     * @brief SetLightEnabled Turn on/off light
     * @param enable true to turn on, false to turn off
     * @return True if operation is successful, false otherwise.
     */
    bool SetLightEnabled(bool enable);

    /**
     * @brief Park Close dust cap
     * @return True if operation is successful, false otherwise.
     */
    bool Park();

    /**
     * @brief UnPark Open dust cap
     * @return True if operation is successful, false otherwise.
     */
    bool UnPark();

};

}

#endif // INDICAP_H
