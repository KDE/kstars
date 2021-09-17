/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

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
        ~OpsEkos() = default;

    private:
        KConfigDialog *m_ConfigDialog;
};
