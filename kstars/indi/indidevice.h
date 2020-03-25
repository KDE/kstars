#ifndef INDI_D_H
#define INDI_D_H

/*  GUI Device Manager
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <QDialog>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

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
class INDI_D : public QDialog
{
        Q_OBJECT
    public:
        INDI_D(INDI::BaseDevice *in_idv, ClientManager *in_cm);
        ~INDI_D();

        QSplitter *getDeviceBox()
        {
            return deviceVBox;
        }

        ClientManager *getClientManager()
        {
            return m_ClientManager;
        }

        INDI_G *getGroup(const QString &groupName);

        INDI::BaseDevice *getBaseDevice()
        {
            return m_BaseDevice;
        }

        QList<INDI_G *> getGroups()
        {
            return groupsList;
        }

        void clearMessageLog();

    public slots:
        bool buildProperty(INDI::Property *prop);
        bool removeProperty(INDI::Property *prop);
        bool removeProperty(const QString &device, const QString &group, const QString &name);
        bool updateSwitchGUI(ISwitchVectorProperty *svp);
        bool updateTextGUI(ITextVectorProperty *tvp);
        bool updateNumberGUI(INumberVectorProperty *nvp);
        bool updateLightGUI(ILightVectorProperty *lvp);
        bool updateBLOBGUI(IBLOB *bp);

        void updateMessageLog(INDI::BaseDevice *idv, int messageID);

    private:
        QString m_Name;
        QSplitter *deviceVBox;
        QTabWidget *groupContainer; /* Groups within the device */
        QTextEdit *msgST_w;         /* scrolled text for messages */

        INDI::BaseDevice *m_BaseDevice;
        ClientManager *m_ClientManager;

        QList<INDI_G *> groupsList;
};

#endif // INDI_D_H
