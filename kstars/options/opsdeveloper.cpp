/*
    SPDX-FileCopyrightText: 2022 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsdeveloper.h"

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "../fitsviewer/bayerparameters.h"

OpsDeveloper::OpsDeveloper() : QFrame(KStars::Instance())
{
    setupUi(this);

    // Fill the engine combos
    kcfg_ImageDebayerEngine->clear();
    kcfg_StackDebayerEngine->clear();
    for (int i = 0; i < static_cast<int>(DebayerEngine::MAX_ITEMS); i++)
    {
        kcfg_ImageDebayerEngine->addItem(BayerUtils::debayerEngineToString(static_cast<DebayerEngine>(i)), i);
        kcfg_StackDebayerEngine->addItem(BayerUtils::debayerEngineToString(static_cast<DebayerEngine>(i)), i);
    }
    kcfg_ImageDebayerEngine->setToolTip(BayerUtils::debayerEngineToolTip());
    kcfg_ImageDebayerEngine->setCurrentIndex(static_cast<int>(Options::imageDebayerEngine()));
    kcfg_StackDebayerEngine->setToolTip(BayerUtils::debayerEngineToolTip());
    kcfg_StackDebayerEngine->setCurrentIndex(static_cast<int>(Options::stackDebayerEngine()));
    // Connection for Image Debayering
    connect(kcfg_ImageDebayerEngine, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]()
    {
        updateAlgoList(kcfg_ImageDebayerEngine, kcfg_ImageDebayerAlgo);
    });

    // Connection for Stack Debayering
    connect(kcfg_StackDebayerEngine, QOverload<int>::of(&QComboBox::currentIndexChanged), [this]()
    {
        updateAlgoList(kcfg_StackDebayerEngine, kcfg_StackDebayerAlgo);
    });

    // Initial sync
    updateAlgoList(kcfg_ImageDebayerEngine, kcfg_ImageDebayerAlgo);
    updateAlgoList(kcfg_StackDebayerEngine, kcfg_StackDebayerAlgo);
}

void OpsDeveloper::updateAlgoList(QComboBox *engineCombo, QComboBox *algoCombo)
{
    if (!engineCombo || !algoCombo)
        return;

    algoCombo->blockSignals(true);
    algoCombo->clear();

    if (engineCombo->currentIndex() == static_cast<int>(DebayerEngine::OpenCV))
    {
        for (int i = 0; i < static_cast<int>(OpenCVAlgo::MAX_ITEMS); i++)
            algoCombo->addItem(BayerUtils::openCVAlgoToString(static_cast<OpenCVAlgo>(i)), i);

        algoCombo->setToolTip(BayerUtils::openCVAlgoToolTip());
    }
    else // ENGINE_DC1394
    {
        for (int i = 0; i < static_cast<int>(DC1394DebayerMethod::MAX_ITEMS); i++)
            algoCombo->addItem(BayerUtils::dc1394MethodToString(static_cast<DC1394DebayerMethod>(i)), i);

        algoCombo->setToolTip(BayerUtils::dc1394MethodToolTip());
    }

    algoCombo->blockSignals(false);
}
