/*  INDI GUI Manager
    Copyright (C) 2012 Jasem Mutlaq

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef GUIMANAGER_H
#define GUIMANAGER_H

#include <QGridLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QPointer>

#ifdef Q_OS_LINUX
#include <indibase.h>
#elif defined(Q_OS_WIN)
#include <basemediator.h>
#endif 

#include "clientmanager.h"

class QLabel;
class QHBoxLayout;
class QVBoxLayout;
class QFrame;
class QString;
class QTabWidget;
class QGridLayout;
class QPushButton;
class FITSViewer;

class INDI_D;

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

    void updateStatus();


    INDI_D * findGUIDevice(const QString &deviceName);

    void addClient(ClientManager *cm);
    void removeClient(ClientManager *cm);

    QList<INDI_D*> getDevices() { return guidevices; }

    int size() { return guidevices.size(); }    

protected:
    void closeEvent(QCloseEvent *);
    void hideEvent(QHideEvent *);
    void showEvent(QShowEvent *);

  private:
    /*****************************************************************
    * GUI stuff
    ******************************************************************/
    QVBoxLayout	*mainLayout;
    QTabWidget	*mainTabWidget;
    QPushButton  *clearB;
    QPushButton  *closeB;
    GUIManager(QWidget * parent = 0);

    static GUIManager * _GUIManager;
    QList<ClientManager *> clients;
    QList<INDI_D*> guidevices;    

  public slots:
    void clearLog();
    void buildDevice(DeviceInfo *di);
    void removeDevice(DeviceInfo *di);
};

#endif // GUIMANAGER_H
