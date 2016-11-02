/*  Ekos Options
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

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
class OpsEkos : public QTabWidget, public Ui::OpsEkos
{
    Q_OBJECT

public:
    explicit OpsEkos();
    ~OpsEkos();

private slots:

    void slotApply();
    void toggleSolverInternal();
    void toggleConfigInternal();
    void toggleWCSInternal();

private:
    KConfigDialog *m_ConfigDialog;
};

#endif

