/*
    SPDX-FileCopyrightText: 2020 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
