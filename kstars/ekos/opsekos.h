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

class KConfigDialog;

/**
 * @class OpsEkos
 *
 * Enables the user to set remote connection devices and options in addition to online and offline astrometry.net settings.
 * The user can also select to enable or disable audiable alarms upon capturing FITS or when an operation is completed.
 *
 * @author Jasem Mutlaq
 */
class OpsEkos : public QFrame, public Ui::OpsEkos
{
    Q_OBJECT

public:
    explicit OpsEkos();
    ~OpsEkos();

private slots:

    void slotApply();
    void slotCancel();

private:
    KConfigDialog *m_ConfigDialog;
};

#endif

