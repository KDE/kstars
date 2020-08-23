/*  Astrometry.net Options Editor
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2017 Robert Lancaster <rlancaste@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "ui_opsalign.h"
#include "parameters.h"

#include <QWidget>

class KConfigDialog;

namespace Ekos
{
class Align;

class OpsAlign : public QWidget, public Ui::OpsAlign
{
    Q_OBJECT

  public:
    explicit OpsAlign(Align *parent);
    virtual ~OpsAlign() override = default;

  protected:
  private slots:
    void slotApply();

  signals:
    void settingsUpdated();

  private:
    QList<SSolver::Parameters> optionsList;
    KConfigDialog *m_ConfigDialog { nullptr };
    Align *alignModule { nullptr };


};
}
