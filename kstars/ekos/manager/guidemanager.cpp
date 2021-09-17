/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikartech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "guidemanager.h"

#include "kstarsdata.h"
#include "Options.h"

namespace Ekos
{
GuideManager::GuideManager(QWidget *parent) : QWidget(parent)
{
    setupUi(this);
    guideStateWidget = new GuideStateWidget();
    guideTitleLayout->addWidget(guideStateWidget, 0, Qt::AlignRight);
}

void GuideManager::init(Guide *guideProcess)
{
    // guide details buttons
    connect(guideDetailNextButton, &QPushButton::clicked, [this]() {
        const int pos = guideDetailWidget->currentIndex();
        if (pos == 0 || (pos == 1 && guideStarPixmap.get() != nullptr))
            guideDetailWidget->setCurrentIndex(pos+1);
        else if (pos > 0)
            guideDetailWidget->setCurrentIndex(0);

        updateGuideDetailView();
    });

    connect(guideDetailPrevButton, &QPushButton::clicked, [this]() {
        const int pos = guideDetailWidget->currentIndex();
        if (pos > 0)
            guideDetailWidget->setCurrentIndex(pos-1);
        else if (guideStarPixmap.get() != nullptr)
            guideDetailWidget->setCurrentIndex(2);
        else
            guideDetailWidget->setCurrentIndex(1);

        updateGuideDetailView();
    });

    // feed guide state widget
    connect(guideProcess, &Ekos::Guide::newStatus, guideStateWidget, &Ekos::GuideStateWidget::updateGuideStatus);

    if (!guidePI)
    {
        guidePI = new QProgressIndicator(guideProcess);
        guideTitleLayout->insertWidget(2, guidePI);
    }

    // initialize the target rings
    targetPlot->buildTarget(Options::guiderAccuracyThreshold());


    // establish connections to receive guiding data
    driftGraph->connectGuider(guideProcess->getGuider());
    targetPlot->connectGuider(guideProcess->getGuider());

    // connect to Guide UI controls
    //This connects all the buttons and slider below the guide plots.
    connect(guideProcess->showRAPlotCheck, &QCheckBox::toggled, [this](bool isChecked) {driftGraph->toggleShowPlot(GuideGraph::G_RA, isChecked);});
    connect(guideProcess->showDECPlotCheck, &QCheckBox::toggled, [this](bool isChecked) {driftGraph->toggleShowPlot(GuideGraph::G_DEC, isChecked);});
    connect(guideProcess->showRACorrectionsCheck, &QCheckBox::toggled, [this](bool isChecked) {driftGraph->toggleShowPlot(GuideGraph::G_RA_PULSE, isChecked);});
    connect(guideProcess->showDECorrectionsCheck, &QCheckBox::toggled, [this](bool isChecked) {driftGraph->toggleShowPlot(GuideGraph::G_DEC_PULSE, isChecked);});
    connect(guideProcess->showSNRPlotCheck, &QCheckBox::toggled, [this](bool isChecked) {driftGraph->toggleShowPlot(GuideGraph::G_SNR, isChecked);});
    connect(guideProcess->showRMSPlotCheck, &QCheckBox::toggled, [this](bool isChecked) {driftGraph->toggleShowPlot(GuideGraph::G_RMS, isChecked);});
    connect(guideProcess->correctionSlider, &QSlider::sliderMoved, driftGraph, &GuideDriftGraph::setCorrectionGraphScale);

    connect(guideProcess->latestCheck, &QCheckBox::toggled, targetPlot, &GuideTargetPlot::setLatestGuidePoint);
    connect(guideProcess->accuracyRadiusSpin, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), targetPlot,
            &GuideTargetPlot::buildTarget);


}


void GuideManager::updateGuideStatus(Ekos::GuideState status)
{
    guideStatus->setText(Ekos::getGuideStatusString(status));

    switch (status)
    {
        case Ekos::GUIDE_IDLE:
        case Ekos::GUIDE_CALIBRATION_ERROR:
        case Ekos::GUIDE_ABORTED:
        case Ekos::GUIDE_SUSPENDED:
        case Ekos::GUIDE_DITHERING_ERROR:
        case Ekos::GUIDE_CALIBRATION_SUCESS:
            if (guidePI->isAnimated())
                guidePI->stopAnimation();
            break;

        case Ekos::GUIDE_CALIBRATING:
            guidePI->setColor(QColor(KStarsData::Instance()->colorScheme()->colorNamed("TargetColor")));
            if (guidePI->isAnimated() == false)
                guidePI->startAnimation();
            break;
        case Ekos::GUIDE_GUIDING:
            guidePI->setColor(Qt::darkGreen);
            if (guidePI->isAnimated() == false)
                guidePI->startAnimation();
            targetPlot->clear();
            driftGraph->clear();
            break;
        case Ekos::GUIDE_DITHERING:
            guidePI->setColor(QColor(KStarsData::Instance()->colorScheme()->colorNamed("TargetColor")));
            if (guidePI->isAnimated() == false)
                guidePI->startAnimation();
            break;
        case Ekos::GUIDE_DITHERING_SUCCESS:
            guidePI->setColor(Qt::darkGreen);
            if (guidePI->isAnimated() == false)
                guidePI->startAnimation();
            break;

        default:
            if (guidePI->isAnimated())
                guidePI->stopAnimation();
            break;
    }

}



void GuideManager::updateGuideStarPixmap(QPixmap &starPix)
{
    if (starPix.isNull())
        return;

    guideStarPixmap.reset(new QPixmap(starPix));
    updateGuideDetailView();
}

void GuideManager::updateSigmas(double ra, double de)
{
    errRA->setText(QString::number(ra, 'f', 2) + "\"");
    errDEC->setText(QString::number(de, 'f', 2) + "\"");
}

void GuideManager::updateGuideDetailView()
{
    const int pos = guideDetailWidget->currentIndex();
    if (pos == 2 && guideStarPixmap.get() != nullptr)
        guideStarView->setPixmap(guideStarPixmap.get()->scaled(guideStarView->width(), guideStarView->height(),
                                                                 Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void GuideManager::reset()
{
    guideStatus->setText(i18n("Idle"));

    if (guidePI)
        guidePI->stopAnimation();
}

}
