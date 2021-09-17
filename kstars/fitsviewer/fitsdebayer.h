/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "bayer.h"
#include "ui_fitsdebayer.h"

class FITSViewer;

class debayerUI : public QDialog, public Ui::FITSDebayerDialog
{
    Q_OBJECT

  public:
    explicit debayerUI(QDialog *parent = 0);
};

class FITSDebayer : public QDialog
{
    Q_OBJECT

  public:
    explicit FITSDebayer(FITSViewer *parent);

    virtual ~FITSDebayer() override = default;

    void setBayerParams(BayerParams *param);

  public slots:
    void applyDebayer();

  private:
    FITSViewer *viewer { nullptr };
    debayerUI *ui { nullptr };
};
