/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2017 Robert Lancaster <rlancaste@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <parameters.h>

#include <ki18n_version.h>
#if KI18N_VERSION >= QT_VERSION_CHECK(5, 89, 0)
#include <KLazyLocalizedString>
#define kde_translate kli18n
#else
#include <KLocalizedString>
#define kde_translate ki18n
#endif

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

#if KI18N_VERSION >= QT_VERSION_CHECK(5, 89, 0)
static const QList<KLazyLocalizedString> ProfileGroupNames =
#else
static const QList<KLocalizedString> ProfileGroupNames =
#endif
{
    kde_translate("Align"),
    kde_translate("Focus"),
    kde_translate("Guide"),
    kde_translate("HFR")
};

QList<SSolver::Parameters> getDefaultFocusOptionsProfiles();
QList<SSolver::Parameters> getDefaultGuideOptionsProfiles();
QList<SSolver::Parameters> getDefaultAlignOptionsProfiles();
QList<SSolver::Parameters> getDefaultHFROptionsProfiles();
SSolver::Parameters getFocusOptionsProfileDefault();
SSolver::Parameters getFocusOptionsProfileDefaultDonut();
}
