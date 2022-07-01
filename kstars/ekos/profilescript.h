/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>
#include <QJsonObject>

class QComboBox;
class QSpinBox;
class QLineEdit;
class QPushButton;

class ProfileScript : public QWidget
{
        Q_OBJECT
        Q_PROPERTY(uint PreDelay MEMBER m_PreDelay)
        Q_PROPERTY(QString PreScript MEMBER m_PreScript)
        Q_PROPERTY(QString Driver MEMBER m_Driver)
        Q_PROPERTY(uint PostDelay MEMBER m_PostDelay)
        Q_PROPERTY(QString PostScript MEMBER m_PostScript)

    public:
        explicit ProfileScript(QWidget *parent = nullptr);
        void setDriverList(const QStringList &value);
        void syncGUI();
        QJsonObject toJSON() const;

    protected:
        QString m_PreScript, m_PostScript, m_Driver;
        uint m_PreDelay {0}, m_PostDelay {0};

    private:
        QComboBox *m_DriverCombo {nullptr};
        QSpinBox *m_PreDelaySpin {nullptr}, *m_PostDelaySpin {nullptr};
        QLineEdit *m_PreScriptEdit {nullptr}, *m_PostScriptEdit {nullptr};
        QPushButton *m_PreScriptB {nullptr}, *m_PostScriptB {nullptr}, *m_RemoveB {nullptr};

    signals:
        void removedRequested();

    protected:
        void selectPreScript();
        void selectPostScript();

};

