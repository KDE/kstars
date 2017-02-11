/*  DSLR Info Dialog
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
        
*/

#ifndef DSLRINFODIALOG_H
#define DSLRINFODIALOG_H

#include <QDialog>

#include <indidevapi.h>

#include "ui_dslrinfo.h"

#include "indi/indiccd.h"

class DSLRInfo : public QDialog, public Ui::DSLRInfo
{
    Q_OBJECT

public:
    explicit DSLRInfo(QWidget *parent, ISD::CCD *ccd);

protected slots:
    void save();

private:
    ISD::CCD *currentCCD=NULL;

};

#endif
