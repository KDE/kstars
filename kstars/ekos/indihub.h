/*
    SPDX-FileCopyrightText: 2020 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

namespace INDIHub
{
enum
{
    None,
    Solo,
    Share,
    Robotic
};

QString toString(uint8_t mode)
{
    if (mode == Solo)
        return "solo";
    else if (mode == Share)
        return "share";
    else if (mode == Robotic)
        return "robotic";
    else
        return "none";
}
}
