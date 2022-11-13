/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QMutex>

#include <indiapi.h>
#include <basedevice.h>

class QTextEdit;
class QTabWidget;
class QSplitter;
class GUIManager;
class ClientManager;
class INDI_G;

/**
 * @class INDI_D
 * INDI_D represents an INDI GUI Device. INDI_D is the top level device container. It contains a collection of groups of properties.
 * Each group is represented as a separate tab within the GUI.
 *
 * @author Jasem Mutlaq
 */
class INDI_D : public QWidget
{
        Q_OBJECT
    public:
        INDI_D(QWidget *parent, const std::shared_ptr<INDI::BaseDevice> &in_idv, ClientManager *in_cm);


        ClientManager *getClientManager() const
        {
            return m_ClientManager;
        }

        INDI_G *getGroup(const QString &groupName) const;

        const std::shared_ptr<INDI::BaseDevice> &getBaseDevice() const
        {
            return m_BaseDevice;
        }

        QList<INDI_G *> getGroups() const
        {
            return groupsList;
        }

        void clearMessageLog();

        const QString &name() const
        {
            return m_Name;
        }

    public slots:
        bool buildProperty(INDI::Property prop);
        //bool removeProperty(INDI::Property prop);
        bool removeProperty(const QString &device, const QString &name);
        bool updateSwitchGUI(ISwitchVectorProperty *svp);
        bool updateTextGUI(ITextVectorProperty *tvp);
        bool updateNumberGUI(INumberVectorProperty *nvp);
        bool updateLightGUI(ILightVectorProperty *lvp);
        bool updateBLOBGUI(IBLOB *bp);

        void updateMessageLog(INDI::BaseDevice *idv, int messageID);

    private:
        QString m_Name;

        // GUI
        QSplitter *deviceVBox { nullptr };
        QTabWidget *groupContainer { nullptr };
        QTextEdit *msgST_w { nullptr };

        // Managers
        std::shared_ptr<INDI::BaseDevice> m_BaseDevice;
        ClientManager *m_ClientManager { nullptr };

        QList<INDI_G *> groupsList;
};
