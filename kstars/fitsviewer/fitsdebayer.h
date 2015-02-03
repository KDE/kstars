#ifndef FITSDEBAYER_H
#define FITSDEBAYER_H

/*  FITS Debayer class
    Copyright (C) 2015 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include "ui_fitsdebayer.h"
#include "bayer.h"

class FITSViewer;

class debayerUI : public QDialog, public Ui::FITSDebayerDialog
{
    Q_OBJECT

public:
    debayerUI(QDialog *parent=0);

};

class FITSDebayer : public QDialog
{
    Q_OBJECT

public:

    FITSDebayer(FITSViewer *parent);
    ~FITSDebayer();

    void setBayerParams(BayerParams * param);

public slots:
    void applyDebayer();

private:
    FITSViewer *viewer;
    debayerUI *ui;
};

#endif // FITSDEBAYER_H
