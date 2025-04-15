/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QList>

class ProfileScript;
class QVBoxLayout;
class QDialogButtonBox;
class QLabel;

class ProfileScriptDialog : public QDialog
{
        Q_OBJECT


    public:
        explicit ProfileScriptDialog(const QStringList &drivers, const QByteArray &settings, QWidget *parent = nullptr);
        const QJsonArray &jsonSettings() const
        {
            return m_ProfileScripts;
        }

    protected:

        QJsonArray m_ProfileScripts;
        QList<ProfileScript *> m_ProfileScriptWidgets;

        void addRule();
        void addJSONRule(QJsonObject settings);
        void removeRule();
        void generateSettings();
        void parseSettings(const QByteArray &settings);

    private:
        QVBoxLayout *m_MainLayout {nullptr};
        QDialogButtonBox *m_ButtonBox {nullptr};
        QPushButton *m_AddRuleB {nullptr}, *m_RemoveRuleB {nullptr};
        QStringList m_DriversList;
        QLabel *m_DriverLabel {nullptr}, *m_PreLabel {nullptr}, *m_PostLabel {nullptr}, *m_PreStopLabel {nullptr}, *m_PostStopLabel;

};


