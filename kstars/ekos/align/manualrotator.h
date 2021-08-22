/*  Ekos Alignment Manual Rotator
    Copyright (C) 2021 by Rick Bassham

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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
