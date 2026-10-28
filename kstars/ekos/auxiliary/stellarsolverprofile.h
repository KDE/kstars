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
const QString ALIGN_DEFAULT_NAME = "1-Default";
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

/**
 * @brief getLegacyAlignOptionsProfiles Align profiles shipped by earlier versions of KStars.
 */
QList<SSolver::Parameters> getLegacyAlignOptionsProfiles();

/**
 * @brief migrateAlignOptionsProfiles Replace the saved copies of built-in align profiles with the current built-in
 * profiles. Unmodified copies of current and earlier built-in profiles are replaced, keeping the user's search radius,
 * time limit and threading preferences. Modified copies are kept under a new name.
 * @param saved profiles as loaded from the saved profiles file.
 * @param selectedIndex index of the selected profile in saved, updated to its index in migrated.
 * @param migrated the profiles to use, sorted by name like the saved profiles file.
 * @return true if migrated differs from saved and the file needs to be rewritten.
 */
bool migrateAlignOptionsProfiles(const QList<SSolver::Parameters> &saved, int &selectedIndex,
                                 QList<SSolver::Parameters> &migrated);

/**
 * @brief loadAlignOptionsProfiles Load the align profiles from SavedAlignProfiles.ini, or the built-in profiles if the
 * file does not exist. Migrates the file and the selected profile option if needed, see migrateAlignOptionsProfiles().
 */
QList<SSolver::Parameters> loadAlignOptionsProfiles();

SSolver::Parameters getFocusOptionsProfileDefault();
SSolver::Parameters getFocusOptionsProfileDefaultDonut();
}
