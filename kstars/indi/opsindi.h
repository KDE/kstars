/*
    SPDX-FileCopyrightText: 2005 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsindi.h"

class KStars;
class KConfigDialog;

/**
 * @class OpsINDI
 *
 * Enables the user to change several INDI options including default ports for common devices,
 * time and location source, and options pertaining to FITSViewer tool.
 *
 * @author Jasem Mutlaq
 */
class OpsINDI : public QFrame, public Ui::OpsINDI
{
        Q_OBJECT

    public:
        OpsINDI();
        virtual ~OpsINDI() override = default;

    private slots:
        void saveFITSDirectory();
        void saveDriversDirectory();
        void slotShowLogFiles();
        void toggleINDIInternal();
        void toggleDriversInternal();
        void verifyINDIServer();
        void saveINDIHubAgent();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };
};
