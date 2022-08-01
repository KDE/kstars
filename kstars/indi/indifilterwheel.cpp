/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "indifilterwheel.h"

namespace ISD
{

bool FilterWheel::setPosition(uint8_t index)
{
    auto nvp = getNumber("FILTER_SLOT");

    if (!nvp)
        return false;

    if (index == static_cast<uint8_t>(nvp->np[0].value))
        return true;

    nvp->at(0)->setValue(index);

    sendNewNumber(nvp);

    return true;
}

bool FilterWheel::setLabels(const QStringList &names)
{
    auto tvp = getText("FILTER_NAME");

    if (!tvp)
        return false;

    if (names.count() != tvp->count())
        return false;

    for (uint8_t i = 0; i < tvp->ntp; i++)
        tvp->at(i)->setText(names[i].toLatin1().constData());
    sendNewText(tvp);
    return true;
}

bool FilterWheel::confirmFilter()
{
    auto svp = getSwitch("CONFIRM_FILTER_SET");
    if (!svp)
        return false;

    svp->at(0)->setState(ISS_ON);
    sendNewSwitch(svp);
    return true;
}

}
