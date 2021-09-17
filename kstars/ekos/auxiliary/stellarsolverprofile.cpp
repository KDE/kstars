/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2017 Robert Lancaster <rlancaste@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "stellarsolverprofile.h"

#include <stellarsolver.h>
#include <KLocalizedString>

namespace Ekos
{

QList<Parameters> getDefaultFocusOptionsProfiles()
{
    QList<Parameters> profileList;

    Parameters focusDefault;
    focusDefault.listName = "1-Focus-Default";
    focusDefault.description = i18n("Default focus star-extraction.");
    focusDefault.initialKeep = 250;
    focusDefault.keepNum = 100;
    focusDefault.minarea = 20;
    focusDefault.maxEllipse = 1.5;
    StellarSolver::createConvFilterFromFWHM(&focusDefault, 2);
    focusDefault.r_min = 5;
    focusDefault.maxSize = 10;
    focusDefault.removeBrightest = 10;
    focusDefault.removeDimmest = 20;
    focusDefault.saturationLimit = 90;
    profileList.append(focusDefault);

    Parameters stars;
    stars.listName = "2-AllStars";
    stars.description = i18n("Profile for the source extraction of all the stars in an image.");
    stars.maxEllipse = 1.5;
    StellarSolver::createConvFilterFromFWHM(&stars, 1);
    stars.r_min = 2;
    profileList.append(stars);

    Parameters smallStars;
    smallStars.listName = "3-SmallSizedStars";
    smallStars.description = i18n("Profile optimized for source extraction of smaller stars.");
    smallStars.maxEllipse = 1.5;
    StellarSolver::createConvFilterFromFWHM(&smallStars, 1);
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
    StellarSolver::createConvFilterFromFWHM(&mid, 4);
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
    StellarSolver::createConvFilterFromFWHM(&big, 8);
    big.r_min = 20;
    big.minSize = 5;
    big.initialKeep = 500;
    big.removeDimmest = 50;
    profileList.append(big);

    return profileList;
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
    StellarSolver::createConvFilterFromFWHM(&stars, 1);
    stars.r_min = 2;
    profileList.append(stars);

    SSolver::Parameters smallStars;
    smallStars.listName = "3-SmallSizedStars";
    smallStars.description = i18n("Profile optimized for source extraction of smaller stars.");
    smallStars.maxEllipse = 1.5;
    StellarSolver::createConvFilterFromFWHM(&smallStars, 1);
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
    StellarSolver::createConvFilterFromFWHM(&mid, 4);
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
    StellarSolver::createConvFilterFromFWHM(&big, 8);
    big.r_min = 20;
    big.minSize = 5;
    big.initialKeep = 500;
    big.removeDimmest = 50;
    profileList.append(big);

    return profileList;
}

QList<SSolver::Parameters> getDefaultAlignOptionsProfiles()
{
    QList<SSolver::Parameters> profileList;

    SSolver::Parameters defaultProfile;
    defaultProfile.listName = "1-Default";
    defaultProfile.description = i18n("Default profile. Generic and not optimized for any specific purpose.");
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
    StellarSolver::createConvFilterFromFWHM(&fastSolving, 4);
    profileList.append(fastSolving);

    SSolver::Parameters parLargeSolving;
    parLargeSolving.listName = "3-LargeScaleSolving";
    parLargeSolving.description = i18n("Profile intended for Plate Solving camera lens sized images");
    parLargeSolving.minwidth = 10;
    parLargeSolving.maxwidth = 180;
    parLargeSolving.keepNum = 50;
    parLargeSolving.initialKeep = 500;
    parLargeSolving.maxEllipse = 1.5;
    StellarSolver::createConvFilterFromFWHM(&parLargeSolving, 4);
    profileList.append(parLargeSolving);

    SSolver::Parameters fastSmallSolving;
    fastSmallSolving.listName = "4-SmallScaleSolving";
    fastSmallSolving.description = i18n("Profile intended for Plate Solving telescopic sized images");
    fastSmallSolving.minwidth = 0.1;
    fastSmallSolving.maxwidth = 10;
    fastSmallSolving.keepNum = 50;
    fastSmallSolving.initialKeep = 500;
    fastSmallSolving.maxEllipse = 1.5;
    StellarSolver::createConvFilterFromFWHM(&fastSmallSolving, 4);
    profileList.append(fastSmallSolving);

    return profileList;
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
    StellarSolver::createConvFilterFromFWHM(&hfrDefault, 2);
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
    StellarSolver::createConvFilterFromFWHM(&big, 8);
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
    StellarSolver::createConvFilterFromFWHM(&most, 1);
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
