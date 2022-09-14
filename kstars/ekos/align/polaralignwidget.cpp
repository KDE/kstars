/*
    SPDX-FileCopyrightText: 2022 Hy Murveit <hy@murveit.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "polaralignwidget.h"

namespace Ekos
{
PolarAlignWidget::PolarAlignWidget(QWidget * parent) : QWidget(parent)
{
    setupUi(this);
    init();
}

namespace
{
void initLed(KLed *led)
{
    led->setColor(Qt::gray);
    led->setLook(KLed::Flat);
    led->setShape(KLed::Circular);
    led->setState(KLed::Off);
}
}  // namespace

void PolarAlignWidget::init()
{
    initLed(PAcapture1Led);
    initLed(PAcapture2Led);
    initLed(PAcapture3Led);
    initLed(PAslew1Led);
    initLed(PAslew2Led);
    initLed(PAsolve1Led);
    initLed(PAsolve2Led);
    initLed(PAsolve3Led);
    initLed(PAsetupLed);
    initLed(PAadjustLed);
}

void PolarAlignWidget::setPreviousGreen(PAState state)
{
    switch (state)
    {
        case ADJUST:
            PAsetupLed->setColor(Qt::green);
            PAsetupLed->on();
        // fall through
        case SETUP:
            PAsolve3Led->setColor(Qt::green);
            PAsolve3Led->on();
        // fall through
        case SOLVE3:
            PAcapture3Led->setColor(Qt::green);
            PAcapture3Led->on();
        // fall through
        case CAPTURE3:
            PAslew2Led->setColor(Qt::green);
            PAslew2Led->on();
        // fall through
        case SLEW2:
            PAsolve2Led->setColor(Qt::green);
            PAsolve2Led->on();
        // fall through
        case SOLVE2:
            PAcapture2Led->setColor(Qt::green);
            PAcapture2Led->on();
        // fall through
        case CAPTURE2:
            PAslew1Led->setColor(Qt::green);
            PAslew1Led->on();
        // fall through
        case SLEW1:
            PAsolve1Led->setColor(Qt::green);
            PAsolve1Led->on();
        // fall through
        case SOLVE1:
            PAcapture1Led->setColor(Qt::green);
            PAcapture1Led->on();
        // fall through
        case CAPTURE1:
            break;
        default:
            break;
    }
}

void PolarAlignWidget::updatePAHStage(PolarAlignmentAssistant::Stage stage)
{
    switch(stage)
    {
        case PolarAlignmentAssistant::PAH_IDLE:
            init();
            break;
        case PolarAlignmentAssistant::PAH_FIRST_CAPTURE:
            update(CAPTURE1);
            break;
        case PolarAlignmentAssistant::PAH_FIRST_SOLVE:
            update(SOLVE1);
            break;
        case PolarAlignmentAssistant::PAH_FIRST_ROTATE:
            update(SLEW1);
            break;
        case PolarAlignmentAssistant::PAH_SECOND_CAPTURE:
            update(CAPTURE2);
            break;
        case PolarAlignmentAssistant::PAH_SECOND_SOLVE:
            update(SOLVE2);
            break;
        case PolarAlignmentAssistant::PAH_SECOND_ROTATE:
            update(SLEW2);
            break;
        case PolarAlignmentAssistant::PAH_THIRD_CAPTURE:
            update(CAPTURE3);
            break;
        case PolarAlignmentAssistant::PAH_THIRD_SOLVE:
            update(SOLVE3);
            break;
        case PolarAlignmentAssistant::PAH_STAR_SELECT:
            update(SETUP);
            break;
        case PolarAlignmentAssistant::PAH_REFRESH:
            update(ADJUST);
            break;
        case PolarAlignmentAssistant::PAH_POST_REFRESH:
            init();
            break;

        // These are ignored in the widget.
        case PolarAlignmentAssistant::PAH_FIRST_SETTLE:
        case PolarAlignmentAssistant::PAH_SECOND_SETTLE:
        case PolarAlignmentAssistant::PAH_FIND_CP:
            break;
    }
}

void PolarAlignWidget::update(PAState state)
{
    // turn off all leds
    init();

    switch (state)
    {
        case CAPTURE1:
            PAcapture1Led->setColor(Qt::yellow);
            PAcapture1Led->on();
            break;
        case SOLVE1:
            setPreviousGreen(SOLVE1);
            PAsolve1Led->setColor(Qt::yellow);
            PAsolve1Led->on();
            break;
        case SLEW1:
            setPreviousGreen(SLEW1);
            PAslew1Led->setColor(Qt::yellow);
            PAslew1Led->on();
            break;
        case CAPTURE2:
            setPreviousGreen(CAPTURE2);
            PAcapture2Led->setColor(Qt::yellow);
            PAcapture2Led->on();
            break;
        case SOLVE2:
            setPreviousGreen(SOLVE2);
            PAsolve2Led->setColor(Qt::yellow);
            PAsolve2Led->on();
            break;
        case SLEW2:
            setPreviousGreen(SLEW2);
            PAslew2Led->setColor(Qt::yellow);
            PAslew2Led->on();
            break;
        case CAPTURE3:
            setPreviousGreen(CAPTURE3);
            PAcapture3Led->setColor(Qt::yellow);
            PAcapture3Led->on();
            break;
        case SOLVE3:
            setPreviousGreen(SOLVE3);
            PAsolve3Led->setColor(Qt::yellow);
            PAsolve3Led->on();
            break;
        case SETUP:
            setPreviousGreen(SETUP);
            PAsetupLed->setColor(Qt::yellow);
            PAsetupLed->on();
            break;
        case ADJUST:
            setPreviousGreen(ADJUST);
            PAadjustLed->setColor(Qt::yellow);
            PAadjustLed->on();
            break;
        default:
            break;
    }
}
}
