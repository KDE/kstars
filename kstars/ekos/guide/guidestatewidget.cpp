/*  Ekos guiding state widget
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
                  2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "guidestatewidget.h"

namespace Ekos
{
GuideStateWidget::GuideStateWidget(QWidget * parent) : QWidget(parent)
{
    setupUi(this);
    init();
}

void GuideStateWidget::init()
{
    // Guiding state
    if (idlingStateLed == nullptr)
    {
        idlingStateLed = new KLed(Qt::gray, KLed::Off, KLed::Flat, KLed::Circular, this);
        idlingStateLed->setObjectName("idlingStateLed");
        guideStateLayout->insertWidget(0, idlingStateLed);
    }
    if (preparingStateLed == nullptr)
    {
        preparingStateLed = new KLed(Qt::gray, KLed::Off, KLed::Flat, KLed::Circular, this);
        preparingStateLed->setObjectName("preparingStateLed");
        guideStateLayout->insertWidget(2, preparingStateLed);
    }
    if (runningStateLed == nullptr)
    {
        runningStateLed = new KLed(Qt::gray, KLed::Off, KLed::Flat, KLed::Circular, this);
        runningStateLed->setObjectName("runningStateLed");
        guideStateLayout->insertWidget(4, runningStateLed);
    }
}

void GuideStateWidget::updateGuideStatus(GuideState state)
{
    idlingStateLed->off();
    preparingStateLed->off();
    runningStateLed->off();
    switch (state)
    {
        case GUIDE_DISCONNECTED:
            idlingStateLed->setColor(Qt::red);
            idlingStateLed->on();
            break;
        case GUIDE_CONNECTED:
            idlingStateLed->setColor(Qt::green);
            preparingStateLed->setColor(Qt::gray);
            runningStateLed->setColor(Qt::gray);
            idlingStateLed->on();
            break;
        case GUIDE_CAPTURE:
        case GUIDE_LOOPING:
        case GUIDE_DARK:
        case GUIDE_SUBFRAME:
            preparingStateLed->setColor(Qt::green);
            runningStateLed->setColor(Qt::gray);
            preparingStateLed->on();
            break;
        case GUIDE_STAR_SELECT:
        case GUIDE_CALIBRATING:
            preparingStateLed->setColor(Qt::yellow);
            runningStateLed->setColor(Qt::gray);
            preparingStateLed->on();
            break;
        case GUIDE_CALIBRATION_ERROR:
            preparingStateLed->setColor(Qt::red);
            runningStateLed->setColor(Qt::red);
            preparingStateLed->on();
            break;
        case GUIDE_CALIBRATION_SUCESS:
            preparingStateLed->setColor(Qt::green);
            runningStateLed->setColor(Qt::yellow);
            preparingStateLed->on();
            break;
        case GUIDE_GUIDING:
            preparingStateLed->setColor(Qt::green);
            runningStateLed->setColor(Qt::green);
            runningStateLed->setState(KLed::On);
            break;
        case GUIDE_MANUAL_DITHERING:
        case GUIDE_DITHERING:
        case GUIDE_DITHERING_SETTLE:
        case GUIDE_REACQUIRE:
        case GUIDE_SUSPENDED:
            runningStateLed->setColor(Qt::yellow);
            runningStateLed->setState(KLed::On);
            break;
        case GUIDE_DITHERING_ERROR:
        case GUIDE_ABORTED:
            idlingStateLed->setColor(Qt::green);
            preparingStateLed->setColor(Qt::red);
            runningStateLed->setColor(Qt::red);
            idlingStateLed->on();
            break;
        case GUIDE_IDLE:
        default:
            idlingStateLed->setColor(Qt::green);
            idlingStateLed->on();
            break;
    }

}
}
