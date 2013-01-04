/*  INDI Options (subclass)
    Copyright (C) 2005 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef OpsEkos_H_
#define OpsEkos_H_

#include "ui_opsekos.h"

class KStars;
class KConfigDialog;

class OpsEkos : public QFrame, public Ui::OpsEkos
{
    Q_OBJECT

public:
    explicit OpsEkos( KStars *_ks );
    ~OpsEkos();

private slots:

    void slotApply();
    void slotCancel();

private:
    KConfigDialog *m_ConfigDialog;
};

#endif

