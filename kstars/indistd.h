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
 
 class INDI_E;
 class INDI_P;
 class INDI_D;
 class KStars;
 class SkyObject;
 class StreamWG;
 class QSocketNotifier;
 class KProgressDialog;

 
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
   SkyObject   		*currentObject;
   QTimer      		*devTimer;	
   KProgressDialog      *downloadDialog;
   
    
   enum DTypes { DATA_FITS, DATA_STREAM, DATA_OTHER };
   
   void setTextValue(INDI_P *pp);
   void setLabelState(INDI_P *pp);
   void registerProperty(INDI_P *pp);
    
   /* Data channel */
   void establishDataChannel(QString host, int port);
   void allocateCompressedBuffer();
   void allocateStreamBuffer();
   
   /* Device options */
   void initDeviceOptions();
   void handleDevCounter();
   bool handleNonSidereal(SkyObject *o);
   void streamDisabled();
   
   
   /* INDI STD: Updates device time */
   void updateTime();
    /* INDI STD: Updates device location */
   void updateLocation();
   
   int                  dataType;
   int 			initDevCounter;
   int 			streamFD;
   unsigned int 	totalCompressedBytes, totalBytes;
   unsigned char        *streamBuffer;
   unsigned char	*compressedBuffer;
   QSocketNotifier 	*sNotifier;
   QString		dataExt;
   
   public slots:
   void streamReceived();
   void timerDone();
   
   signals:
   void linkRejected();
   void linkAccepted();
 
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
