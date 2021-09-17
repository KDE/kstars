/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opscalibration.h"

class KConfigDialog;

namespace Ekos
{
class InternalGuider;

/**
 * @class OpsCalibration
 *
 * Enables the user to set guide calibration options
 *
 * @author Jasem Mutlaq
 */
class OpsCalibration : public QFrame, public Ui::OpsCalibration
{
    Q_OBJECT

  public:
    explicit OpsCalibration(InternalGuider *guiderObject);
    virtual ~OpsCalibration() override = default;

  protected:
    void showEvent(QShowEvent *) override;

  private slots:

    void slotApply();

  private:
    KConfigDialog *m_ConfigDialog { nullptr };
    InternalGuider *guider { nullptr };
};
}
