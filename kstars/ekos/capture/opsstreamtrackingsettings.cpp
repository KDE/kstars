/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsstreamtrackingsettings.h"
#include "kstars.h"

namespace Ekos
{
OpsStreamTrackingSettings::OpsStreamTrackingSettings() : QWidget(KStars::Instance())
{
    setupUi(this);
}
}
