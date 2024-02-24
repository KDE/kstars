/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2017 Robert Lancaster <rlancaste@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <parameters.h>

namespace Ekos
{
const QString FOCUS_DEFAULT_NAME = "1-Focus-Default";
const QString FOCUS_DEFAULT_DONUT_NAME = "1-Focus-Default-Donut";
typedef enum
{
    AlignProfiles,
    FocusProfiles,
    GuideProfiles,
    HFRProfiles
} ProfileGroup;

QList<SSolver::Parameters> getDefaultFocusOptionsProfiles();
QList<SSolver::Parameters> getDefaultGuideOptionsProfiles();
QList<SSolver::Parameters> getDefaultAlignOptionsProfiles();
QList<SSolver::Parameters> getDefaultHFROptionsProfiles();
SSolver::Parameters getFocusOptionsProfileDefault();
SSolver::Parameters getFocusOptionsProfileDefaultDonut();
}
