/*  Ekos Alignment Manual Rotator
    SPDX-FileCopyrightText: 2021 Rick Bassham

    SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "ui_manualrotator.h"
#include "ekos/ekos.h"
#include "align.h"


namespace Ekos
{

class ManualRotator : public QDialog, public Ui::ManualRotator
{
        Q_OBJECT

    public:
        explicit ManualRotator(Align *parent);
        ~ManualRotator();

        void setRotatorDiff(double current, double target, double diff);

private:

    signals:
        void newLog(const QString &);
        void captureAndSolve();

    private:

        Align *m_AlignInstance {nullptr};
};
}
