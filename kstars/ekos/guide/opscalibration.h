/*  Ekos Options
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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
