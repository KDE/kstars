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
 
 class TQSocketNotifier;
 
 // INDI device manager
class DeviceManager : public QObject
{
   Q_OBJECT
   public:
   DeviceManager(INDIMenu *INDIparent, int inID);
   ~DeviceManager();

   INDIMenu *parent;

   TQPtrList<INDI_D> indi_dev;

   int			mgrID;
   int			serverFD;
   FILE			*serverFP;
   LilXML		*XMLParser;
   TQSocketNotifier 	*sNotifier;
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
   void startBlob (TQString devName, TQString propName, TQString timestamp);
   void sendOneBlob(TQString blobName, unsigned int blobSize, TQString blobFormat, unsigned char * blobBuffer);
   void finishBlob();

   /*****************************************************************
   * Misc.
   ******************************************************************/
   int  delPropertyCmd (XMLEle *root, char errmsg[]);
   int  removeDevice   (TQString devName, char errmsg[]);
   INDI_D *  findDev   (TQString devName, char errmsg[]);

   int  messageCmd     (XMLEle *root, char errmsg[]);
   void checkMsg       (XMLEle *root, INDI_D *dp);
   void doMsg          (XMLEle *msg , INDI_D *dp);

   bool indiConnect    (TQString inHost, TQString inPort);

  public slots:
   void dataReceived();
   
  signals:
   void newDevice();

};

#endif
