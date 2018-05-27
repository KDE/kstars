/*  Ekos Options
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "ui_opsguide.h"
#include "guide.h"

class KConfigDialog;

namespace Ekos
{
/**
 * @class OpsGuide
 *
 * Enables the user to set guide options
 *
 * @author Jasem Mutlaq
 */
class OpsGuide : public QFrame, public Ui::OpsGuide
{
    Q_OBJECT

  public:
    explicit OpsGuide();
    virtual ~OpsGuide() override = default;

  signals:
    void settingsUpdated();

  private:
    KConfigDialog *m_ConfigDialog { nullptr };
};
}
