/*  INDI Options (subclass)
    Copyright (C) 2005 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef OPSINDI_H_
#define OPSINDI_H_

#include "ui_opsindi.h"

class KStars;
class KConfigDialog;

class OpsINDI : public QFrame, public Ui::OpsINDI
{
    Q_OBJECT

public:
    OpsINDI( KStars *_ks );
    ~OpsINDI();

private slots:
    void saveFilterAlias();
    void updateFilterAlias(int index);
    void saveFITSDirectory();
    void saveDriversDirectory();
    void slotApply();
    void slotCancel();

private:
    QStringList m_filterList;
    KConfigDialog *m_ConfigDialog;
};

#endif

