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
#include <QPointer>

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
        INDI_D(QWidget *parent, INDI::BaseDevice baseDevice, ClientManager *in_cm);


        ClientManager *getClientManager() const;

        /** @brief Forward a property update to the driver via the ClientManager.
         *  No-ops (with a warning) if the ClientManager is no longer valid, e.g.
         *  the device was disconnected concurrently with this call. */
        void sendNewProperty(INDI::Property prop);

        /** @brief Enable/disable BLOB reception for a property via the ClientManager.
         *  No-ops (with a warning) if the ClientManager is no longer valid. */
        void setBLOBEnabled(bool enabled, const QString &device, const QString &property);

        INDI_G *getGroup(const QString &groupName) const;

        INDI::BaseDevice getBaseDevice() const
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

    public Q_SLOTS:
        bool buildProperty(INDI::Property prop);
        bool updateProperty(INDI::Property prop);
        bool removeProperty(INDI::Property prop);

        bool updateSwitchGUI(INDI::Property prop);
        bool updateTextGUI(INDI::Property prop);
        bool updateNumberGUI(INDI::Property prop);
        bool updateLightGUI(INDI::Property prop);
        bool updateBLOBGUI(INDI::Property prop);

        void updateMessageLog(INDI::BaseDevice idv, int messageID);

    private:
        QString m_Name;

        // GUI
        QSplitter *deviceVBox { nullptr };
        QTabWidget *groupContainer { nullptr };
        QTextEdit *msgST_w { nullptr };

        // Managers
        INDI::BaseDevice m_BaseDevice;
        QPointer<ClientManager> m_ClientManager;

        QList<INDI_G *> groupsList;
};
