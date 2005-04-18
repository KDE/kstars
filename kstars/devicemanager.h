/*  Device Manager
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    JM Changelog
    2004-16-1:	Start
   
 */
 
 #ifndef DEVICEMANAGER_H
 #define DEVICEMANAGER_H
 
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

   QPtrList<INDI_D> indi_dev;

   int			mgrID;
   int			serverFD;
   FILE			*serverFP;
   LilXML		*XMLParser;
   QSocketNotifier 	*sNotifier;
   QString		host;
   QString		port;

   int dispatchCommand   (XMLEle *root, char errmsg[]);

   INDI_D *  addDevice   (XMLEle *dep , char errmsg[]);
   INDI_D *  findDev     (XMLEle *root, int  create, char errmsg[]);

   /*****************************************************************
   * Send to server
   ******************************************************************/
   void sendNewText    (INDI_P *pp);
   void sendNewNumber  (INDI_P *pp);
   void sendNewSwitch  (INDI_P *pp, int index);
   void startBlob (QString devName, QString propName, QString timestamp);
   void sendOneBlob(QString blobName, unsigned int blobSize, QString blobFormat, unsigned char * blobBuffer);
   void finishBlob();

   /*****************************************************************
   * Misc.
   ******************************************************************/
   int  delPropertyCmd (XMLEle *root, char errmsg[]);
   int  removeDevice   (QString devName, char errmsg[]);
   INDI_D *  findDev   (QString devName, char errmsg[]);

   int  messageCmd     (XMLEle *root, char errmsg[]);
   void checkMsg       (XMLEle *root, INDI_D *dp);
   void doMsg          (XMLEle *msg , INDI_D *dp);

   bool indiConnect    (QString inHost, QString inPort);

  public slots:
   void dataReceived();
   
  signals:
   void newDevice();

};

#endif
