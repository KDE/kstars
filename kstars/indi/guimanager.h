/*  INDI GUI Manager
    Copyright (C) 2012 Jasem Mutlaq

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QList>
#include <QWidget>

class QCloseEvent;
class QHideEvent;
class QPushButton;
class QShowEvent;
class QString;
class QTabWidget;
class QVBoxLayout;

class INDI_D;

class ClientManager;
class DeviceInfo;

/**
 * @class GUIManager
 * GUIManager creates the INDI Control Panel upon receiving a new device. Each device is displayed
 * on a separate tab. The device and property GUI creation is performed dynamically via introspection. As new properties
 * arrive from the ClientManager, they get created in the GUI.
 *
 * @author Jasem Mutlaq
 */
class GUIManager : public QWidget
{
        Q_OBJECT
    public:
        static GUIManager *Instance();
        static void release();

        void updateStatus(bool toggle_behavior);

        INDI_D *findGUIDevice(const QString &deviceName);

        void addClient(ClientManager *cm);
        void removeClient(ClientManager *cm);

        QList<INDI_D *> getDevices()
        {
            return guidevices;
        }

        int size()
        {
            return guidevices.size();
        }

    protected:
        void closeEvent(QCloseEvent *) override;
        void hideEvent(QHideEvent *) override;
        void showEvent(QShowEvent *) override;

    private:
        /*****************************************************************
         * GUI stuff
         ******************************************************************/
        QVBoxLayout *mainLayout;
        QTabWidget *mainTabWidget;
        QPushButton *clearB;
        QPushButton *closeB;
        GUIManager(QWidget *parent = nullptr);
        ~GUIManager();

        static GUIManager *_GUIManager;
        QList<ClientManager *> clients;
        QList<INDI_D *> guidevices;

    public slots:
        void changeAlwaysOnTop(Qt::ApplicationState state);
        void clearLog();
        void buildDevice(DeviceInfo *di);
        void removeDevice(const QString &name);
};
