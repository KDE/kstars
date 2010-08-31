/*  INDI STD
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    2004-01-18: Classes that handle INDI Standard properties.
 */

 #ifndef INDISTD_H_
 #define INDISTD_H_

#include <qobject.h>
#include <kfileitem.h>

#include <lilxml.h>

class INDI_E;
class INDI_P;
class INDI_D;
class KStars;
class SkyObject;
class StreamWG;
class KProgressDialog;
class KDirLister;
class SkyObject;
class SkyPoint;


/* This class implmements standard properties on the device level*/
class INDIStdDevice : public QObject
{
    Q_OBJECT
public:
    INDIStdDevice(INDI_D *associatedDevice, KStars * kswPtr);
    ~INDIStdDevice();

    KStars      	*ksw;			/* Handy pointer to KStars */
    INDI_D      	*dp;			/* associated device */

    StreamWG            *streamWindow;
    SkyObject   	*currentObject;
    QTimer      	*devTimer;
    KProgressDialog     *downloadDialog;

    enum DTypes { DATA_FITS, DATA_STREAM, DATA_OTHER, DATA_CCDPREVIEW };

    void setTextValue(INDI_P *pp);
    void setLabelState(INDI_P *pp);
    void registerProperty(INDI_P *pp);
    void handleBLOB(unsigned char *buffer, int bufferSize, const QString &dataFormat);

    /* Device options */
    void createDeviceInit();
    void processDeviceInit();
    bool handleNonSidereal();
    void streamDisabled();

    /* INDI STD: Slew to a point */
    bool slew_scope(SkyPoint *scope_target, INDI_E *lp=NULL);
    /* INDI STD: Updates device time */
    void updateTime();
    /* INDI STD: Updates device location */
    void updateLocation();
    /* Update image prefix */
    void updateSequencePrefix(const QString &newPrefix);

    int                  dataType;
    QString		dataExt;
    LilXML		*parser;

    QString		seqPrefix;
    int			seqCount;
    bool		batchMode;
    bool		ISOMode;
    bool		driverLocationUpdated, driverTimeUpdated;
    KDirLister          *seqLister;
    SkyObject		*telescopeSkyObject;

public slots:
    void timerDone();

protected slots:
    void checkSeqBoundary(const KFileItemList & items);

signals:
    void linkRejected();
    void linkAccepted();
    void newTelescope();
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

    /* Perform action trigged by INDI property */
    bool actionTriggered(INDI_E *lp);
    bool newSwitch(INDI_E* el);

public slots:
    void newTime();
    void newText();



};

#endif
