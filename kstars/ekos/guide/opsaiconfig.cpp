/*
    SPDX-FileCopyrightText: 2026 Pavan <pk6122004@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsaiconfig.h"
#include "kstars.h"


namespace Ekos
{

OpsAIConfig::OpsAIConfig() : QFrame(KStars::Instance())
{
    setupUi(this);
}

}
