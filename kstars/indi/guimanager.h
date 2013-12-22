#ifndef GUIMANAGER_H
#define GUIMANAGER_H

/*  INDI frontend for KStars
    Copyright (C) 2012 Jasem Mutlaq

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

/*  INDI frontend for KStars
    Copyright (C) 2012 Jasem Mutlaq

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <QGridLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QTabWidget>

#include <indibase.h>

#include "clientmanager.h"

class QLabel;
class QHBoxLayout;
class QVBoxLayout;
class QFrame;
class QString;
class QTextEdit;
class QTabWidget;
class QGridLayout;
class QPushButton;

class INDI_D;


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
