/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2017 Robert Lancaster <rlancaste@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsprograms.h"

#include <QWidget>

class KConfigDialog;

namespace Ekos
{
class Align;

class OpsPrograms : public QWidget, public Ui::OpsPrograms
{
    Q_OBJECT

  public:
    explicit OpsPrograms(Align *parent);
    virtual ~OpsPrograms() override = default;

  protected:
  private slots:
    void loadDefaultPaths(int option);
    void toggleSolverInternal();
    void toggleConfigInternal();
    void toggleWCSInternal();
    void toggleSextractorInternal();
    void slotApply();

  signals:
    void settingsUpdated();

  private:
    KConfigDialog *m_ConfigDialog { nullptr };
    Align *alignModule { nullptr };


};
}
