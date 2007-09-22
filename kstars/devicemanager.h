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

 #include "indielement.h"

class INDIMenu;
class INDI_P;
class INDI_D;

class QSocketNotifier;

// INDI device manager
class DeviceManager : public QObject
{
    Q_OBJECT
public:
    DeviceManager(INDIMenu *INDIparent, int inID);
    ~DeviceManager();

    INDIMenu *parent;

    QList<INDI_D*> indi_dev;

    int			mgrID;
    int			serverFD;
    FILE			*serverFP;
    LilXML		*XMLParser;
    QSocketNotifier 	*sNotifier;
    QString		host;
    QString		port;

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

    bool indiConnect    (const QString &inHost, const QString &inPort);

public slots:
    void dataReceived();

signals:
    void newDevice();

};

#endif
