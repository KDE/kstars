/*  FITS Debayer class
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

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
}

void FITSDebayer::applyDebayer()
{
    FITSView *view = viewer->getCurrentView();

    if (view)
    {
        FITSData *image_data = view->getImageData();

        dc1394bayer_method_t method = (dc1394bayer_method_t)ui->methodCombo->currentIndex();
        dc1394color_filter_t filter = (dc1394color_filter_t)(ui->filterCombo->currentIndex() + 512);

        int offsetX = ui->XOffsetSpin->value();
        int offsetY = ui->YOffsetSpin->value();

        BayerParams param;
        param.method  = method;
        param.filter  = filter;
        param.offsetX = offsetX;
        param.offsetY = offsetY;

        image_data->setBayerParams(&param);

        ui->statusEdit->setText(i18n("Processing..."));

        qApp->processEvents();

        if (image_data->debayer())
        {
            ui->statusEdit->setText(i18n("Complete."));
            view->rescale(ZOOM_KEEP_LEVEL);
            view->updateFrame();
        }
        else
            ui->statusEdit->setText(i18n("Debayer failed."));
    }
}

void FITSDebayer::setBayerParams(BayerParams *param)
{
    ui->methodCombo->setCurrentIndex(param->method);
    ui->filterCombo->setCurrentIndex(param->filter - 512);

    ui->XOffsetSpin->setValue(param->offsetX);
    ui->YOffsetSpin->setValue(param->offsetY);
}
