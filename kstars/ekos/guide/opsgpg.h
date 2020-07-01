/*  GPG Options
    Copyright (C) 2020 Hy Murveit

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "ui_opsgpg.h"

class KConfigDialog;

namespace Ekos
{
class InternalGuider;

/**
 * @class OpsGPG
 *
 * Enables the user to set Gaussian Process Guider options
 *
 * @author Hy Murveit
 */
class OpsGPG : public QFrame, public Ui::OpsGPG
{
    Q_OBJECT

  public:
    explicit OpsGPG(InternalGuider *guiderObject);
    virtual ~OpsGPG() override = default;

  private slots:
    void slotApply();

  private:
    KConfigDialog *m_ConfigDialog { nullptr };
    InternalGuider *guider { nullptr };
};
}
