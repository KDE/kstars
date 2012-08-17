/*  INDI frontend for KStars
    Copyright (C) 2012 Jasem Mutlaq

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef INDIMENU_H_
#define INDIMENU_H_

#include "indielement.h"
#include "devicemanager.h"

#include <QGridLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QTabWidget>

class INDI_D;


class QLabel;
class QHBoxLayout;
class QVBoxLayout;
class QFrame;
class QString;
class QTextEdit;
class QTabWidget;
class QGridLayout;

class KStars;

class INDIMenu : public QWidget
{
    Q_OBJECT
public:
    INDIMenu(QWidget * parent = 0);
    ~INDIMenu();

    KStars *ksw;

    QList<DeviceManager*> managers;

    /*****************************************************************
    * GUI stuff
    ******************************************************************/
    QVBoxLayout	*mainLayout;
    QTabWidget	*mainTabWidget;
    QTextEdit 	*msgST_w;

    void updateStatus();
    QString getUniqueDeviceLabel(const QString &deviceName);
  
    INDI_D * findDevice(const QString &deviceName);
    INDI_D * findDeviceByLabel(const QString &label);
  
    void setCurrentDevice(const QString &device) { currentDevice = device; }
    QString getCurrentDevice() { return currentDevice; }
  
    DeviceManager* startDeviceManager(QList<IDevice *> &processed_devices, DeviceManager::ManagerMode inMode, uint inPort);
    DeviceManager* initDeviceManager(QString inHost, uint inPort, DeviceManager::ManagerMode inMode);
    void stopDeviceManager(QList<IDevice *> &processed_devices);

  private:
    QPushButton  *clearB;
    QPushButton  *closeB;
    QString currentDevice;
  
  public slots:
    void removeDeviceManager(DeviceManager *deviceManager);
    void clearLog();


};

#endif
