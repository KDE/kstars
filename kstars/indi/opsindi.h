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

/**
 * @class OpsINDI
 *
 * Enables the user to change several INDI options including default ports for common devices, time and location source, and options pertnaning to FITSViewer tool.
 *
 * @author Jasem Mutlaq
 */
class OpsINDI : public QFrame, public Ui::OpsINDI
{
    Q_OBJECT

public:
    OpsINDI();
    ~OpsINDI();

private slots:
    void saveFITSDirectory();
    void saveDriversDirectory();
    void slotShowLogFiles();
    void toggleINDIInternal();
    void toggleDriversInternal();
    void verifyINDIServer();

private:
    KConfigDialog *m_ConfigDialog;
};

#endif

