/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2017 Robert Lancaster <rlancaste@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "stellarsolverprofile.h"

#include "ekos_debug.h"
#include "kspaths.h"
#include "Options.h"

#include <stellarsolver.h>
#undef Const
#include <KLocalizedString>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <algorithm>

namespace Ekos
{

QList<Parameters> getDefaultFocusOptionsProfiles()
{
    QList<Parameters> profileList;

    Parameters focusDefault = getFocusOptionsProfileDefault();
    profileList.append(focusDefault);

    Parameters focusDefaultDonut = getFocusOptionsProfileDefaultDonut();
    profileList.append(focusDefaultDonut);

    Parameters stars;
    stars.listName = "2-AllStars";
    stars.description = i18n("Profile for the source extraction of all the stars in an image.");
    stars.maxEllipse = 1.5;
    stars.convFilterType = SSolver::CONV_GAUSSIAN;
    stars.fwhm = 1;
    stars.r_min = 2;
    profileList.append(stars);

    Parameters smallStars;
    smallStars.listName = "3-SmallSizedStars";
    smallStars.description = i18n("Profile optimized for source extraction of smaller stars.");
    smallStars.maxEllipse = 1.5;
    smallStars.convFilterType = SSolver::CONV_GAUSSIAN;
    smallStars.fwhm = 1;
    smallStars.r_min = 2;
    smallStars.maxSize = 5;
    smallStars.initialKeep = 500;
    smallStars.saturationLimit = 80;
    profileList.append(smallStars);

    Parameters mid;
    mid.listName = "4-MidSizedStars";
    mid.description = i18n("Profile optimized for source extraction of medium sized stars.");
    mid.maxEllipse = 1.5;
    mid.minarea = 20;
    mid.convFilterType = SSolver::CONV_GAUSSIAN;
    mid.fwhm = 4;
    mid.r_min = 5;
    mid.removeDimmest = 20;
    mid.minSize = 2;
    mid.maxSize = 10;
    mid.initialKeep = 500;
    mid.saturationLimit = 80;
    profileList.append(mid);

    Parameters big;
    big.listName = "5-BigSizedStars";
    big.description = i18n("Profile optimized for source extraction of larger stars.");
    big.maxEllipse = 1.5;
    big.minarea = 40;
    big.convFilterType = SSolver::CONV_GAUSSIAN;
    big.fwhm = 8;
    big.r_min = 20;
    big.minSize = 5;
    big.initialKeep = 500;
    big.removeDimmest = 50;
    profileList.append(big);

    return profileList;
}

SSolver::Parameters getFocusOptionsProfileDefault()
{
    Parameters focusDefault;
    focusDefault.listName = FOCUS_DEFAULT_NAME;
    focusDefault.description = i18n("Default focus star-extraction.");
    focusDefault.initialKeep = 250;
    focusDefault.keepNum = 100;
    focusDefault.minarea = 20;
    focusDefault.maxEllipse = 1.5;
    focusDefault.convFilterType = SSolver::CONV_DEFAULT;
    focusDefault.deblend_contrast = 1.0; // Turns off deblending - no benefit for focus and sometimes causes issues
    focusDefault.fwhm = 1;
    focusDefault.r_min = 5;
    focusDefault.maxSize = 10;
    focusDefault.removeBrightest = 10;
    focusDefault.removeDimmest = 20;
    focusDefault.saturationLimit = 90;
    return focusDefault;
}

SSolver::Parameters getFocusOptionsProfileDefaultDonut()
{
    Parameters focusDefaultDonut;
    focusDefaultDonut.listName = FOCUS_DEFAULT_DONUT_NAME;
    focusDefaultDonut.description = i18n("Profile optimized for source extraction of stars including Donuts.");
    focusDefaultDonut.initialKeep = 10000;
    focusDefaultDonut.keepNum = 1000;
    focusDefaultDonut.minarea = 20;
    focusDefaultDonut.maxEllipse = 1.5;
    focusDefaultDonut.convFilterType = SSolver::CONV_DEFAULT;
    focusDefaultDonut.deblend_contrast = 1.0;
    focusDefaultDonut.r_min = 5;
    focusDefaultDonut.maxSize = 0.0;
    focusDefaultDonut.removeBrightest = 0.0;
    focusDefaultDonut.removeDimmest = 0.0;
    focusDefaultDonut.saturationLimit = 90.0;
    return focusDefaultDonut;
}

QList<SSolver::Parameters> getDefaultGuideOptionsProfiles()
{
    QList<SSolver::Parameters> profileList;

    Parameters guideDefault;
    guideDefault.listName = "1-Guide-Default";
    guideDefault.description = i18n("Default guider star-extraction.");
    guideDefault.initialKeep = 250;
    guideDefault.keepNum = 100;
    guideDefault.minarea = 10;
    guideDefault.maxSize = 8;
    guideDefault.saturationLimit = 98;
    guideDefault.removeBrightest = 0;
    guideDefault.removeDimmest = 0;
    profileList.append(guideDefault);

    SSolver::Parameters stars;
    stars.listName = "2-AllStars";
    stars.description = i18n("Profile for the source extraction of all the stars in an image.");
    stars.maxEllipse = 1.5;
    stars.convFilterType = SSolver::CONV_GAUSSIAN;
    stars.fwhm = 1;
    stars.r_min = 2;
    profileList.append(stars);

    SSolver::Parameters smallStars;
    smallStars.listName = "3-SmallSizedStars";
    smallStars.description = i18n("Profile optimized for source extraction of smaller stars.");
    smallStars.maxEllipse = 1.5;
    smallStars.convFilterType = SSolver::CONV_GAUSSIAN;
    smallStars.fwhm = 1;
    smallStars.r_min = 2;
    smallStars.maxSize = 5;
    smallStars.initialKeep = 500;
    smallStars.saturationLimit = 80;
    profileList.append(smallStars);

    SSolver::Parameters mid;
    mid.listName = "4-MidSizedStars";
    mid.description = i18n("Profile optimized for source extraction of medium sized stars.");
    mid.maxEllipse = 1.5;
    mid.minarea = 20;
    mid.convFilterType = SSolver::CONV_GAUSSIAN;
    mid.fwhm = 4;
    mid.r_min = 5;
    mid.removeDimmest = 20;
    mid.minSize = 2;
    mid.maxSize = 10;
    mid.initialKeep = 500;
    mid.saturationLimit = 80;
    profileList.append(mid);

    SSolver::Parameters big;
    big.listName = "5-BigSizedStars";
    big.description = i18n("Profile optimized for source extraction of larger stars.");
    big.maxEllipse = 1.5;
    big.minarea = 40;
    big.convFilterType = SSolver::CONV_GAUSSIAN;
    big.fwhm = 8;
    big.r_min = 20;
    big.minSize = 5;
    big.initialKeep = 500;
    big.removeDimmest = 50;
    profileList.append(big);

    return profileList;
}

// One profile for all plate solving. Benchmarks on real frames (telescope, guide scope and OSC, mono and bayered,
// on a desktop and a Raspberry Pi) found the former SingleThread, LargeScale and SmallScale profiles slower or
// less reliable: initialKeep and a wide fwhm dropped stars in nebulous or faint frames. Threading is chosen from
// the available hints and the blind search width from the expected field of view, see SolverUtils.
QList<SSolver::Parameters> getDefaultAlignOptionsProfiles()
{
    QList<SSolver::Parameters> profileList;

    SSolver::Parameters defaultProfile;
    defaultProfile.listName = ALIGN_DEFAULT_NAME;
    defaultProfile.description = i18n("Default profile. Generic and not optimized for any specific purpose.");
    defaultProfile.minwidth = 0.1;
    defaultProfile.maxwidth = 10;
    defaultProfile.keepNum = 50;
    defaultProfile.convFilterType = SSolver::CONV_GAUSSIAN;
    defaultProfile.fwhm = 1;
    profileList.append(defaultProfile);

    return profileList;
}

// The align profiles shipped by earlier versions. Copies of them saved in SavedAlignProfiles.ini are replaced
// by the current built-in profiles. Add the current built-in profiles here when they change.
QList<SSolver::Parameters> getLegacyAlignOptionsProfiles()
{
    QList<SSolver::Parameters> profileList;

    // 1-Default from 2022 (StellarSolver 2.0) until 2026
    SSolver::Parameters defaultProfile;
    defaultProfile.listName = ALIGN_DEFAULT_NAME;
    defaultProfile.convFilterType = SSolver::CONV_GAUSSIAN;
    defaultProfile.fwhm = 1;
    profileList.append(defaultProfile);

    SSolver::Parameters fastSolving;
    fastSolving.listName = "2-SingleThreadSolving";
    fastSolving.description = i18n("Profile intended for Plate Solving telescopic sized images in a single CPU Thread");
    fastSolving.multiAlgorithm = NOT_MULTI;
    fastSolving.minwidth = 0.1;
    fastSolving.maxwidth = 10;
    fastSolving.keepNum = 50;
    fastSolving.initialKeep = 500;
    fastSolving.maxEllipse = 1.5;
    fastSolving.convFilterType = SSolver::CONV_GAUSSIAN;
    fastSolving.fwhm = 4;
    profileList.append(fastSolving);

    SSolver::Parameters parLargeSolving;
    parLargeSolving.listName = "3-LargeScaleSolving";
    parLargeSolving.description = i18n("Profile intended for Plate Solving camera lens sized images");
    parLargeSolving.minwidth = 10;
    parLargeSolving.maxwidth = 180;
    parLargeSolving.keepNum = 50;
    parLargeSolving.initialKeep = 500;
    parLargeSolving.maxEllipse = 1.5;
    parLargeSolving.convFilterType = SSolver::CONV_GAUSSIAN;
    parLargeSolving.fwhm = 4;
    profileList.append(parLargeSolving);

    SSolver::Parameters fastSmallSolving;
    fastSmallSolving.listName = "4-SmallScaleSolving";
    fastSmallSolving.description = i18n("Profile intended for Plate Solving telescopic sized images");
    fastSmallSolving.minwidth = 0.1;
    fastSmallSolving.maxwidth = 10;
    fastSmallSolving.keepNum = 50;
    fastSmallSolving.initialKeep = 500;
    fastSmallSolving.maxEllipse = 1.5;
    fastSmallSolving.convFilterType = SSolver::CONV_GAUSSIAN;
    fastSmallSolving.fwhm = 4;
    profileList.append(fastSmallSolving);

    // 1-Default from MR 1711 (2026)
    SSolver::Parameters defaultProfile1711 = defaultProfile;
    defaultProfile1711.minwidth = 0.1;
    defaultProfile1711.maxwidth = 10;
    defaultProfile1711.keepNum = 50;
    profileList.append(defaultProfile1711);

    return profileList;
}

namespace
{
// Settings that users adjust to their setup. They are ignored when checking whether a saved profile is an
// unmodified copy of a built-in profile, and they are kept when such a copy is replaced.
void copyPreferences(const SSolver::Parameters &from, SSolver::Parameters &to)
{
    to.search_radius = from.search_radius;
    to.solverTimeLimit = from.solverTimeLimit;
    to.partition = from.partition;
    to.inParallel = from.inParallel;
}

// Older StellarSolver versions stored some settings as float, so saved values may differ from the
// built-in values by float rounding, e.g. 0.004999999888241291 instead of 0.005.
void ignoreFloatRounding(const SSolver::Parameters &profile, SSolver::Parameters &stock)
{
    for (auto member :
            {
                &SSolver::Parameters::kron_fact, &SSolver::Parameters::r_min, &SSolver::Parameters::magzero,
                &SSolver::Parameters::minarea, &SSolver::Parameters::deblend_contrast, &SSolver::Parameters::clean_param,
                &SSolver::Parameters::fwhm, &SSolver::Parameters::threshold_offset, &SSolver::Parameters::threshold_bg_multiple,
                &SSolver::Parameters::maxSize, &SSolver::Parameters::minSize, &SSolver::Parameters::maxEllipse,
                &SSolver::Parameters::removeBrightest, &SSolver::Parameters::removeDimmest,
                &SSolver::Parameters::saturationLimit, &SSolver::Parameters::minwidth, &SSolver::Parameters::maxwidth
            })
    {
        if (static_cast<float>(profile.*member) == static_cast<float>(stock.*member))
            stock.*member = profile.*member;
    }
}

bool isStockProfile(const SSolver::Parameters &profile, const QList<SSolver::Parameters> &stockProfiles)
{
    for (auto stock : stockProfiles)
    {
        if (stock.listName != profile.listName)
            continue;
        copyPreferences(profile, stock);
        ignoreFloatRounding(profile, stock);
        if (stock == profile)
            return true;
    }
    return false;
}

bool sameProfiles(QList<SSolver::Parameters> a, const QList<SSolver::Parameters> &b)
{
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); i++)
    {
        if (a[i].listName != b[i].listName || !(a[i] == b[i]))
            return false;
    }
    return true;
}
}

bool migrateAlignOptionsProfiles(const QList<SSolver::Parameters> &saved, int &selectedIndex,
                                 QList<SSolver::Parameters> &migrated)
{
    const QList<SSolver::Parameters> builtIns = getDefaultAlignOptionsProfiles();
    QList<SSolver::Parameters> stockProfiles = getLegacyAlignOptionsProfiles();
    stockProfiles.append(builtIns);

    QStringList stockNames;
    for (const auto &profile : stockProfiles)
        stockNames << profile.listName;

    QStringList savedNames;
    for (const auto &profile : saved)
        savedNames << profile.listName;

    QString selectedName = (selectedIndex >= 0 && selectedIndex < saved.size()) ? saved[selectedIndex].listName : QString();

    migrated = builtIns;
    for (const auto &profile : saved)
    {
        if (stockNames.contains(profile.listName) && isStockProfile(profile, stockProfiles))
        {
            // Replace the copy with the current built-in profile, keeping the user's preferences.
            // Copies of retired built-in profiles are dropped and their users moved to the default profile.
            bool replaced = false;
            for (auto &builtIn : migrated)
            {
                if (builtIn.listName == profile.listName)
                {
                    copyPreferences(profile, builtIn);
                    replaced = true;
                }
            }
            if (!replaced && selectedName == profile.listName)
                selectedName = ALIGN_DEFAULT_NAME;
            continue;
        }

        SSolver::Parameters kept = profile;
        if (stockNames.contains(profile.listName))
        {
            // A modified built-in profile is kept as the user's own profile, since built-in profiles are
            // always taken from the code.
            QString name = i18nc("Name of a modified built-in plate solving profile, %1 is the original name", "%1 (Custom)",
                                 profile.listName);
            for (int i = 2; savedNames.contains(name) || stockNames.contains(name); i++)
                name = i18nc("Name of a modified built-in plate solving profile, %1 is the original name, %2 a number",
                             "%1 (Custom %2)", profile.listName, i);
            kept.listName = name;
            if (selectedName == profile.listName)
                selectedName = name;
        }
        migrated.append(kept);
    }

    // Saved profiles are read back sorted by name, keep that order so the selected index stays valid
    std::sort(migrated.begin(), migrated.end(), [](const SSolver::Parameters & a, const SSolver::Parameters & b)
    {
        return a.listName < b.listName;
    });

    selectedIndex = 0;
    for (int i = 0; i < migrated.size(); i++)
    {
        if (migrated[i].listName == selectedName)
            selectedIndex = i;
    }

    return !sameProfiles(migrated, saved);
}

QList<SSolver::Parameters> loadAlignOptionsProfiles()
{
    const QString savedOptionsProfiles = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(
            "SavedAlignProfiles.ini");
    if (!QFile(savedOptionsProfiles).exists())
    {
        const QList<SSolver::Parameters> builtIns = getDefaultAlignOptionsProfiles();
        // A retired built-in profile may be selected
        if (Options::solveOptionsProfile() >= static_cast<uint>(builtIns.size()))
            Options::setSolveOptionsProfile(0);
        return builtIns;
    }

    const QList<SSolver::Parameters> saved = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
    int selectedIndex = Options::solveOptionsProfile();
    QList<SSolver::Parameters> migrated;
    if (!migrateAlignOptionsProfiles(saved, selectedIndex, migrated))
        return saved;

    QSettings settings(savedOptionsProfiles, QSettings::IniFormat);
    settings.clear();
    for (const auto &profile : migrated)
    {
        settings.beginGroup(profile.listName);
        const QMap<QString, QVariant> map = SSolver::Parameters::convertToMap(profile);
        for (auto it = map.constBegin(); it != map.constEnd(); ++it)
            settings.setValue(it.key(), it.value());
        settings.endGroup();
    }
    settings.sync();

    if (static_cast<int>(Options::solveOptionsProfile()) != selectedIndex)
        Options::setSolveOptionsProfile(selectedIndex);
    qCInfo(KSTARS_EKOS) << "Updated the saved align profiles to the current built-in profiles, selected profile"
                        << migrated[selectedIndex].listName;

    return migrated;
}

QList<Parameters> getDefaultHFROptionsProfiles()
{
    QList<Parameters> profileList;

    Parameters hfrDefault;
    hfrDefault.listName = "1-HFR-Default";
    hfrDefault.description = i18n("Default. Set for typical HFR estimation.");
    hfrDefault.initialKeep = 250;
    hfrDefault.keepNum = 100;

    hfrDefault.minarea = 20;
    hfrDefault.maxEllipse = 1.5;
    hfrDefault.convFilterType = SSolver::CONV_GAUSSIAN;
    hfrDefault.fwhm = 2;
    hfrDefault.r_min = 5;
    hfrDefault.maxSize = 10;

    hfrDefault.removeBrightest = 10;
    hfrDefault.removeDimmest = 20;
    hfrDefault.saturationLimit = 90;

    profileList.append(hfrDefault);


    Parameters big;
    big.listName = "2-BigSizedStars";
    big.description = i18n("Set for typical HFR estimation on big stars.");
    big.initialKeep = 250;
    big.keepNum = 100;

    big.minarea = 40;
    big.maxEllipse = 1.5;
    big.convFilterType = SSolver::CONV_GAUSSIAN;
    big.fwhm = 8;
    big.r_min = 20;
    big.maxSize = 0;

    big.removeBrightest = 0;
    big.removeDimmest = 50;
    big.saturationLimit = 99;

    profileList.append(big);

    Parameters most;
    most.listName = "3-MostStars";
    most.description = i18n("Set for HFR estimation on most stars.");
    most.initialKeep = 1000;
    most.keepNum = 1000;

    most.minarea = 10;
    most.maxEllipse = 0;
    most.convFilterType = SSolver::CONV_GAUSSIAN;
    most.fwhm = 1;
    most.r_min = 3.5;
    most.minSize = 0;
    most.maxSize = 0;

    most.removeBrightest = 0;
    most.removeDimmest = 0;
    most.saturationLimit = 0;
    profileList.append(most);

    return profileList;
}

}
