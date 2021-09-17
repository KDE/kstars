/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2017 Robert Lancaster <rlancaste@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsastrometry.h"
#include "parameters.h"

#include <QWidget>

class KConfigDialog;

namespace Ekos
{
class Align;

class OpsAstrometry : public QWidget, public Ui::OpsAstrometry
{
    Q_OBJECT

  public:

    explicit OpsAstrometry(Align *parent);
    virtual ~OpsAstrometry() override = default;

  protected:
    void showEvent(QShowEvent *) override;

  private slots:
    void slotUpdatePosition();
    void slotUpdateScale();
    void slotApply();

  private:
    KConfigDialog *m_ConfigDialog { nullptr };
    Align *alignModule { nullptr };
};
}
