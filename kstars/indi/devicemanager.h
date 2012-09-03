/*  Device Manager
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    JM Changelog
    2004-16-1:	Start
   
 */

#ifndef DEVICEMANAGER_H_
#define DEVICEMANAGER_H_

#include <QTcpSocket>

class INDIMenu;
class INDI_P;
class INDI_D;

class KProcess;
class QTcpSocket;

// INDI device manager
class DeviceManager : public QObject
{
    Q_OBJECT
public:
    enum ManagerMode { M_LOCAL, M_SERVER, M_CLIENT };
    enum { INDI_DEVICE_NOT_FOUND=-1, INDI_PROPERTY_INVALID=-2, INDI_PROPERTY_DUPLICATED = -3, INDI_DISPATCH_ERROR=-4 };

    DeviceManager(INDIMenu *INDIparent, QString inHost, uint inPort, ManagerMode inMode);
      ~DeviceManager();

    INDIMenu *parent;
    QList<INDI_D*> indi_dev;
    QList<IDevice *> managed_devices;

    QTcpSocket		 serverSocket;
    LilXML		 *XMLParser;
    QString		 host;
    uint		 port;
    QString 		 serverBuffer;
    ManagerMode 	 mode;
    KProcess 		 *serverProcess;

    int dispatchCommand   (XMLEle *root, QString & errmsg);

    INDI_D *  addDevice   (XMLEle *dep , QString & errmsg);
    INDI_D *  findDev     (XMLEle *root, int  create, QString & errmsg);

    /*****************************************************************
    * Send to server
    ******************************************************************/
    void sendNewText    (INDI_P *pp);
    void sendNewNumber  (INDI_P *pp);
    void sendNewSwitch  (INDI_P *pp, INDI_E *lp);
    void startBlob (const QString &devName, const QString &propName, const QString &timestamp);
    void sendOneBlob(const QString &blobName, unsigned int blobSize, const QString &blobFormat, unsigned char * blobBuffer);
    void finishBlob();

    /*****************************************************************
    * Misc.
    ******************************************************************/
    int  delPropertyCmd (XMLEle *root, QString & errmsg);
    int  removeDevice   (const QString &devName, QString & errmsg);
    INDI_D *  findDev   (const QString &devName, QString & errmsg);

    int  messageCmd     (XMLEle *root, QString & errmsg);
    void checkMsg       (XMLEle *root, INDI_D *dp);
    void doMsg          (XMLEle *msg , INDI_D *dp);

    void appendManagedDevices(QList<IDevice *> & processed_devices);
    void startServer();
    void stopServer();
    void connectToServer();
    void enableBLOB(bool enable, QString device = QString(), QString property = QString());

    QString getServerBuffer() { return serverBuffer; }

public slots:
    void dataReceived();
    void connectionSuccess();
    void connectionError();
    void processStandardError();

signals:
    void newDevice(INDI_D *);
    void deviceManagerError(DeviceManager *);
    void newServerInput();

};

#endif
