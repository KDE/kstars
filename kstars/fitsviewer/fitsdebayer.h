/*  FITS Debayer class
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

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
