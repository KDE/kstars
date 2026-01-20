/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitsdebayer.h"

#include "fitsdata.h"
#include "fitsview.h"
#include "fitsviewer.h"

#include <QPushButton>

debayerUI::debayerUI(QDialog *parent) : QDialog(parent)
{
    setupUi(parent);
    setModal(false);
}

FITSDebayer::FITSDebayer(FITSViewer *parent) : QDialog(parent)
{
    ui = new debayerUI(this);

    viewer = parent;

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &FITSDebayer::applyDebayer);

    // Fill in the engine combo and select the default setting
    ui->engineCombo->clear();
    for (int i = 0; i < static_cast<int>(DebayerEngine::MAX_ITEMS); i++)
        ui->engineCombo->addItem(BayerUtils::debayerEngineToString(static_cast<DebayerEngine>(i)), i);

    ui->engineCombo->setToolTip(BayerUtils::debayerEngineToolTip());
    ui->engineCombo->setCurrentIndex(static_cast<int>(Options::imageDebayerEngine()));

    // Connection for Engine
    connect(ui->engineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FITSDebayer::updateMethodList);

    // Fill in the bayer pattern combo
    ui->filterCombo->clear();
    for (int i = 0; i < static_cast<int>(BayerPattern::MAX_ITEMS); i++)
        ui->filterCombo->addItem(BayerUtils::bayerPatternToString(static_cast<BayerPattern>(i)), i);

    // Initial sync
    updateMethodList();
}

void FITSDebayer::updateMethodList()
{
    ui->methodCombo->blockSignals(true);
    ui->methodCombo->clear();

    if (ui->engineCombo->currentIndex() == static_cast<int>(DebayerEngine::OpenCV))
    {
        for (int i = 0; i < static_cast<int>(OpenCVAlgo::MAX_ITEMS); i++)
            ui->methodCombo->addItem(BayerUtils::openCVAlgoToString(static_cast<OpenCVAlgo>(i)), i);

        ui->methodCombo->setToolTip(BayerUtils::openCVAlgoToolTip());
    }
    else // ENGINE_DC1394
    {
        for (int i = 0; i < static_cast<int>(DC1394DebayerMethod::MAX_ITEMS); i++)
            ui->methodCombo->addItem(BayerUtils::dc1394MethodToString(static_cast<DC1394DebayerMethod>(i)), i);

        ui->methodCombo->setToolTip(BayerUtils::dc1394MethodToolTip());
    }
    ui->methodCombo->blockSignals(false);
}

void FITSDebayer::applyDebayer()
{
    QSharedPointer<FITSView> view;
    if (viewer->getCurrentView(view))
    {
        auto image_data = view->imageData();

        BayerParameters param;
        param.engine = static_cast<DebayerEngine>(ui->engineCombo->currentIndex());
        if (param.engine == DebayerEngine::OpenCV)
        {
            OpenCVParams cvParams;
            cvParams.algo    = static_cast<OpenCVAlgo>(ui->methodCombo->currentIndex());
            cvParams.pattern = static_cast<BayerPattern>(ui->filterCombo->currentIndex());
            cvParams.offsetX = ui->XOffsetSpin->value();
            cvParams.offsetY = ui->YOffsetSpin->value();
            param.params = QVariant::fromValue(cvParams);
        }
        else
        {
            DC1394Params dc1394Params;
            dc1394Params.params.method  = BayerUtils::convertDC1394Method(
                                              static_cast<DC1394DebayerMethod>(ui->methodCombo->currentIndex()));
            dc1394Params.params.filter  = BayerUtils::convertDC1394Filter(
                                              static_cast<BayerPattern>(ui->filterCombo->currentIndex()));
            dc1394Params.params.offsetX = ui->XOffsetSpin->value();
            dc1394Params.params.offsetY = ui->YOffsetSpin->value();
            param.params = QVariant::fromValue(dc1394Params);
        }

        image_data->setBayerParams(&param);

        ui->statusEdit->setText(i18n("Processing..."));

        qApp->processEvents();

        if (image_data->debayer(true))
        {
            image_data->calculateStats(true, false);
            view->loadData(image_data);
            ui->statusEdit->setText(i18n("Complete."));
        }
        else
            ui->statusEdit->setText(i18n("Debayer failed."));
    }
}

void FITSDebayer::setBayerParams(BayerParameters *param)
{
    ui->engineCombo->setCurrentIndex(static_cast<int>(param->engine));

    if (param->engine == DebayerEngine::DC1394)
    {
        if (!param->params.canConvert<DC1394Params>())
            return;

        DC1394Params dc1394Params = param->params.value<DC1394Params>();

        ui->methodCombo->setCurrentIndex(dc1394Params.params.method);
        ui->filterCombo->setCurrentIndex(static_cast<int>
                                         (BayerUtils::convertDC1394Filter_t(dc1394Params.params.filter)));

        ui->XOffsetSpin->setValue(dc1394Params.params.offsetX);
        ui->YOffsetSpin->setValue(dc1394Params.params.offsetY);
    }
    else if (param->engine == DebayerEngine::OpenCV)
    {
        if (!param->params.canConvert<OpenCVParams>())
            return;

        OpenCVParams cvParams = param->params.value<OpenCVParams>();

        ui->methodCombo->setCurrentIndex(static_cast<int>(cvParams.algo));
        ui->filterCombo->setCurrentIndex(static_cast<int>(cvParams.pattern));

        ui->XOffsetSpin->setValue(cvParams.offsetX);
        ui->YOffsetSpin->setValue(cvParams.offsetY);
    }
}
