/*  Astrometry.net Options Editor
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2017 Robert Lancaster <rlancaste@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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
