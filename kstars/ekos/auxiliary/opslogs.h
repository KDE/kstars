/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opslogs.h"

class KConfigDialog;

namespace Ekos
{
/**
 * @class OpsLogs
 *
 * Enables the user to set logging options
 *
 * @author Jasem Mutlaq
 */
class OpsLogs : public QFrame, public Ui::OpsLogs
{
    Q_OBJECT

  public:
    explicit OpsLogs();
    uint16_t getINDIDebugInterface() { return m_INDIDebugInterface; }
    bool isINDISettingsChanged() { return m_SettingsChanged; }


  private slots:
    void refreshInterface();
    void slotToggleVerbosityOptions();
    void slotToggleOutputOptions();
    void slotClearLogs();

  private:
    qint64 getDirSize(const QString &dirPath);

    uint16_t m_INDIDebugInterface = { 0 };
    bool m_SettingsChanged = { false };
};

}
