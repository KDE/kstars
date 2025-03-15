/*
    SPDX-FileCopyrightText: 2024 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "imagingplanneroptions.h"

#include "Options.h"
#include "ui_imagingplanneroptions.h"


ImagingPlannerOptionsUI::ImagingPlannerOptionsUI(QWidget *p) : QFrame(p)
{
    setupUi(this);
}

ImagingPlannerOptions::ImagingPlannerOptions(QWidget *parent) : QDialog(parent)
{
    ui = new ImagingPlannerOptionsUI(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(ui);

    setWindowTitle(i18nc("@title:window", "Imaging Planner Options"));
    connect(ui->CenterOnSkyMapCB, &QCheckBox::clicked, this, &ImagingPlannerOptions::slotCenterOnSkyMap);
    connect(ui->IndependentWindowCB, &QCheckBox::clicked, this, &ImagingPlannerOptions::slotIndependentWindow);
    connect(ui->StartSolvingImmediatelyCB, &QCheckBox::clicked, this, &ImagingPlannerOptions::slotStartSolvingImmediately);
}

void ImagingPlannerOptions::showEvent(QShowEvent *)
{
    ui->CenterOnSkyMapCB->setChecked(Options::imagingPlannerCenterOnSkyMap());
    ui->IndependentWindowCB->setChecked(Options::imagingPlannerIndependentWindow());
    ui->StartSolvingImmediatelyCB->setChecked(Options::imagingPlannerStartSolvingImmediately());
}

void ImagingPlannerOptions::slotCenterOnSkyMap(bool checked)
{
    Options::setImagingPlannerCenterOnSkyMap(checked);
}

void ImagingPlannerOptions::slotIndependentWindow(bool checked)
{
    Options::setImagingPlannerIndependentWindow(checked);
}

void ImagingPlannerOptions::slotStartSolvingImmediately(bool checked)
{
    Options::setImagingPlannerStartSolvingImmediately(checked);
}
