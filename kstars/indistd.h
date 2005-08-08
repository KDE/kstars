/*  INDI STD
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    2004-01-18: Classes that handle INDI Standard properties.
 */
 
 #ifndef INDISTD_H
 #define INDISTD_H
 
 #include <qobject.h>
 #include <indi/lilxml.h>
 #include <kfileitem.h>
 
 class INDI_E;
 class INDI_P;
 class INDI_D;
 class KStars;
 class SkyObject;
 class StreamWG;
 class CCDPreviewWG;
 class QSocketNotifier;
 class KProgressDialog;
 class KDirLister;
 class SkyObject;
 
 
 /* This class implmements standard properties on the device level*/
 class INDIStdDevice : public QObject
 {
   Q_OBJECT
   public:
   INDIStdDevice(INDI_D *associatedDevice, KStars * kswPtr);
   ~INDIStdDevice();
   
   KStars      		*ksw;			/* Handy pointer to KStars */
   INDI_D      		*dp;			/* associated device */

   StreamWG             *streamWindow;
   CCDPreviewWG    *CCDPreviewWindow;
   SkyObject   		*currentObject;
   QTimer      		*devTimer;	
   KProgressDialog      *downloadDialog;
   
    
   enum DTypes { DATA_FITS, DATA_STREAM, DATA_OTHER, DATA_CCDPREVIEW };
   
   void setTextValue(INDI_P *pp);
   void setLabelState(INDI_P *pp);
   void registerProperty(INDI_P *pp);
   void handleBLOB(unsigned char *buffer, int bufferSize, QString dataFormat);
    
   /* Device options */
   void initDeviceOptions();
   void handleDevCounter();
   bool handleNonSidereal();
   void streamDisabled();
   
   
   /* INDI STD: Updates device time */
   void updateTime();
    /* INDI STD: Updates device location */
   void updateLocation();
   /* Update image prefix */
   void updateSequencePrefix(QString newPrefix);
   
   int                  dataType;
   int 			initDevCounter;
   QString		dataExt;
   LilXML		*parser;
   
   QString		seqPrefix;
   int			seqCount;
   bool			batchMode;
   bool			ISOMode;
   KDirLister           *seqLister;
   SkyObject		*telescopeSkyObject;
   
   public slots:
   void timerDone();
   
   protected slots:
   void checkSeqBoundary(const KFileItemList & items);
   
   signals:
   void linkRejected();
   void linkAccepted();
   void FITSReceived(QString deviceLabel);
 
 };
 
 /* This class implmements standard properties */
 class INDIStdProperty : public QObject
 {
    Q_OBJECT
   public:
   INDIStdProperty(INDI_P *associatedProperty, KStars * kswPtr, INDIStdDevice *stdDevPtr);
   ~INDIStdProperty();

    KStars        *ksw;			/* Handy pointer to KStars */
    INDIStdDevice *stdDev;              /* pointer to common std device */
    INDI_P	  *pp;			/* associated property */
    
    /* Perform switch converting */
    bool convertSwitch(int switchIndex, INDI_E *lp);
    bool newSwitch(int id, INDI_E* el);
    
    public slots:
    void newTime();
    void newText();
    

    
};

#endif
