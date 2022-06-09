/*
    SPDX-FileCopyrightText: 2022 Hy Murveit <hy@murveit.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_polaralignwidget.h"

#include "ekos/ekos.h"

#include <QWidget>
#include <KLed>

#include "polaralignmentassistant.h"

namespace Ekos
{

class PolarAlignWidget : public QWidget, public Ui::PolarAlignWidget
{
        Q_OBJECT

    public:
        enum PAState
        {
            CAPTURE1,
            SOLVE1,
            SLEW1,
            CAPTURE2,
            SOLVE2,
            SLEW2,
            CAPTURE3,
            SOLVE3,
            SETUP,
            ADJUST
        };

        PolarAlignWidget(QWidget * parent = nullptr);
        void init();

        void update(PAState state);
        void updatePAHStage(PolarAlignmentAssistant::PAHStage stage);


    public slots:

    private:
        void setPreviousGreen(PAState state);

};
}
