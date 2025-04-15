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
        Q_PROPERTY(uint StoppingDelay MEMBER m_StoppingDelay)
        Q_PROPERTY(QString StoppingScript MEMBER m_StoppingScript)
        Q_PROPERTY(uint StoppedDelay MEMBER m_StoppedDelay)
        Q_PROPERTY(QString StoppedScript MEMBER m_StoppedScript)

    public:
        explicit ProfileScript(QWidget *parent = nullptr);
        void setDriverList(const QStringList &value);
        void syncGUI();
        QJsonObject toJSON() const;

    protected:
        QString m_PreScript, m_PostScript, m_Driver, m_StoppingScript, m_StoppedScript;
        uint m_PreDelay {0}, m_PostDelay {0}, m_StoppingDelay {0}, m_StoppedDelay {0};

    private:
        QComboBox *m_DriverCombo {nullptr};
        QSpinBox *m_PreDelaySpin {nullptr}, *m_PostDelaySpin {nullptr}, *m_StoppingDelaySpin {nullptr}, *m_StoppedDelaySpin {nullptr};
        QLineEdit *m_PreScriptEdit {nullptr}, *m_PostScriptEdit {nullptr}, *m_StoppingScriptEdit {nullptr}, *m_StoppedScriptEdit {nullptr};
        QPushButton *m_PreScriptB {nullptr}, *m_PostScriptB {nullptr}, *m_RemoveB {nullptr}, *m_StoppingScriptB {nullptr}, *m_StoppedScriptB {nullptr};

    signals:
        void removedRequested();

    protected:
        void selectPreScript();
        void selectPostScript();
        void selectStoppingScript();
        void selectStoppedScript();

};

