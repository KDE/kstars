/*  INDI Hub Agent
    Copyright (C) 2020 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#pragma once

#include <QString>

namespace INDIHub
{
enum
{
    None,
    Solo,
    Shared,
    Robotic
};

QString toString(uint8_t mode)
{
    if (mode == Solo)
        return "solo";
    else if (mode == Shared)
        return "shared";
    else if (mode == Robotic)
        return "robotic";
    else
        return "none";
}
}
