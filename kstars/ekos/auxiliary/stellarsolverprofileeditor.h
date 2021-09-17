/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2017 Robert Lancaster <rlancaste@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_stellarsolverprofileeditor.h"
#include "parameters.h"
#include "stellarsolverprofile.h"


#include <QWidget>
#include <QDir>

class KConfigDialog;

namespace Ekos
{
class Align;

class StellarSolverProfileEditor : public QWidget, public Ui::StellarSolverProfileEditor
{
        Q_OBJECT

    public:
        void setProfileGroup(ProfileGroup group);

        explicit StellarSolverProfileEditor(QWidget *parent, ProfileGroup group, KConfigDialog *dialog);
        virtual ~StellarSolverProfileEditor() override = default;

        //These functions handle the settings for the Sextractors and Solvers
        SSolver::Parameters getSettingsFromUI();
        void sendSettingsToUI(SSolver::Parameters a);

        //These functions save and load the settings of the program.
        void openSingleProfile();
        void saveSingleProfile();
        void copySingleProfile();
        void loadProfiles();
        void saveProfiles();
        void loadOptionsProfile();
        void loadOptionsProfileIgnoreOldSettings(int index);
        void saveBackupProfiles();
        void openBackupProfiles();
        QList<SSolver::Parameters> getDefaultProfiles();
        void loadDefaultProfiles();

        void connectOptionsProfileComboBox();
        void disconnectOptionsProfileComboBox();
    public slots:
        void loadProfile(int profile);
    signals:
        void optionsProfilesUpdated();
    protected:

    private slots:

        void slotApply();


    private:
        QString savedOptionsProfiles;
        int openOptionsProfileNum = 0;
        void settingJustChanged();
        QString dirPath = QDir::homePath();
        QList<SSolver::Parameters> optionsList;
        bool optionsAreSaved = true;
        KConfigDialog *m_ConfigDialog { nullptr };

        ProfileGroup selectedProfileGroup = AlignProfiles;
};
}
