/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef DSLRINFODIALOG_H
#define DSLRINFODIALOG_H

#include <QDialog>

#include <indidevapi.h>

#include "ui_dslrinfo.h"

#include "indi/indiccd.h"

namespace Ekos
{
class Capture;
}

class DSLRInfo : public QDialog, public Ui::DSLRInfo
{
        Q_OBJECT

    public:
        explicit DSLRInfo(QWidget *parent, ISD::CCD *ccd);

    protected slots:
        void save();

    signals:
        void infoChanged();

    private:
        ISD::CCD *currentCCD = nullptr;
        int sensorMaxWidth = 0, sensorMaxHeight = 0;
        double sensorPixelW = 0, sensorPixelH = 0;

        friend class Ekos::Capture;
};

#endif
