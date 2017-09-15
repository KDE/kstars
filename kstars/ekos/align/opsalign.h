/*  Astrometry.net Options Editor
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2017 Robert Lancaster <rlancaste@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef OPSALIGN_H
#define OPSALIGN_H

#include <QWidget>

#include "ui_opsalign.h"

class KConfigDialog;

namespace Ekos
{
class Align;

class OpsAlign : public QWidget, public Ui::OpsAlign
{
    Q_OBJECT

  public:
    explicit OpsAlign(Align *parent);
    ~OpsAlign();

  protected:
  private slots:
    void toggleSolverInternal();
    void toggleConfigInternal();
    void toggleWCSInternal();
    void slotApply();

  signals:
    void settingsUpdated();

  private:
    KConfigDialog *m_ConfigDialog;
    Align *alignModule;
};
}

#endif // OpsAlign_H
