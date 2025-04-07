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

    if (!nvp.isValid())
        return false;

    if (index == static_cast<uint8_t>(nvp[0].getValue()))
        return true;

    nvp[0].setValue(index);

    sendNewProperty(nvp);

    return true;
}

bool FilterWheel::setLabels(const QStringList &names)
{
    auto tvp = getText("FILTER_NAME");

    if (!tvp.isValid())
        return false;

    if (names.count() != tvp.count())
        return false;

    for (uint8_t i = 0; i < tvp.count(); i++)
        tvp[i].setText(names[i].toLatin1().constData());

    sendNewProperty(tvp);
    return true;
}

bool FilterWheel::confirmFilter()
{
    auto svp = getSwitch("CONFIRM_FILTER_SET");
    if (!svp.isValid())
        return false;

    svp[0].setState(ISS_ON);
    sendNewProperty(svp);
    return true;
}

}
