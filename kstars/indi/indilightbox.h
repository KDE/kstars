/*  INDI Light Box
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef INDILIGHTBOX_H
#define INDILIGHTBOX_H

#include "indistd.h"
#include "indicap.h"

namespace ISD
{

/**
 * @class Cap
 * Handles operation of a remotely controlled light box.
 *
 * @author Jasem Mutlaq
 */

class LightBox : public DustCap
{
    Q_OBJECT

public:
    LightBox(GDInterface *iPtr) : DustCap(iPtr) {}

    virtual bool hasLight();
    virtual bool canPark();

};

}

#endif // INDILIGHTBOX_H
