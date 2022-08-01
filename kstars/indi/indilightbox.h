/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indidustcap.h"
#include "indistd.h"

namespace ISD
{
/**
 * @class LightBox
 * Handles operation of a remotely controlled light box.
 *
 * @author Jasem Mutlaq
 */
class LightBox : public DustCap
{
        Q_OBJECT

    public:
        explicit LightBox(GenericDevice *parent) : DustCap(parent) {}

        virtual bool hasLight() override;
        virtual bool canPark() override;
};
}
