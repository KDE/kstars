/*  Astrometry.net Options Editor
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2017 Robert Lancaster <rlancaste@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include "ui_optionsprofileeditor.h"
#include "parameters.h"


#include <QWidget>
#include <QDir>

class KConfigDialog;

namespace Ekos
{
class Align;

class OptionsProfileEditor : public QWidget, public Ui::OptionsProfileEditor
{
    Q_OBJECT

  public:

    explicit OptionsProfileEditor(Align *parent);
    virtual ~OptionsProfileEditor() override = default;

    //These functions handle the settings for the Sextractors and Solvers
    SSolver::Parameters getSettingsFromUI();
    void sendSettingsToUI(SSolver::Parameters a);

    //These functions save and load the settings of the program.
    void loadProfiles();
    void saveProfiles();
    void loadOptionsProfile();
    void saveBackupProfiles();
    void loadBackupProfiles();
    void loadDefaultProfiles();

    void connectOptionsProfileComboBox();
    void disconnectOptionsProfileComboBox();
  protected:

  private slots:

    void slotApply();

  private:
    QString savedOptionsProfiles;
    void settingJustChanged();
    QString dirPath = QDir::homePath();
    QList<SSolver::Parameters> optionsList;
    bool optionsAreSaved = true;
    KConfigDialog *m_ConfigDialog { nullptr };
    Align *alignModule { nullptr };
};
}
