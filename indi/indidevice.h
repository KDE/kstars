/*  INDI Device
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)
    		       Elwood C. Downey

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef INDIDEVICE_H_
#define INDIDEVICE_H_

#include <unistd.h>

#include "indielement.h"

#include <QGridLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

class DeviceManager;
class INDI_D;
class INDI_P;
class INDI_G;
class INDI_E;
class INDIMenu;
class INDIStdDevice;


class QLabel;
class QHBoxLayout;
class QVBoxLayout;
class QFrame;
class QString;
class QTextEdit;
class QTabWidget;
class QGridLayout;
class QSplitter;

/*************************************************************************
** The INDI Tree
**
** INDI_ELEMENT <----------------------------------------
**     |						|
**     -----> INDI_PROPERTY				|
**                        |				|
**                        -----> INDI_GROUP		|
**                                        |		|
**                                        -----> INDI_DEVICE
**                                               |         |
                                          Device Manager  INDI Menu
**************************************************************************/


/* INDI device */
class INDI_D : public KDialog
{
    Q_OBJECT
public:
    INDI_D(INDIMenu *parentMenu, DeviceManager *InParentManager, const QString &inName, const QString &inLabel);
    ~INDI_D();

    QString 	name;			/* device name */
    QString	label;			/* device label */
    QSplitter   *deviceVBox;

    QTabWidget  *groupContainer;	/* Groups within the device */
    QTextEdit	*msgST_w;		/* scrolled text for messages */
    unsigned char *dataBuffer;          /* Generic buffer */
    QCheckBox    *enableBLOBC;

    INDIStdDevice  *stdDev;

    QList<INDI_G*> gl;			/* list of pointers to groups */

    INDI_G        *curGroup;
    bool	  INDIStdSupport;

    INDIMenu      *parent;
    DeviceManager *deviceManager;

    enum DTypes { DATA_FITS, ASCII_DATA_STREAM, VIDEO_STREAM, DATA_OTHER, DATA_CCDPREVIEW };

    /*****************************************************************
    * Build
    ******************************************************************/
    int buildTextGUI    (XMLEle *root, QString & errmsg);
    int buildNumberGUI  (XMLEle *root, QString & errmsg);
    int buildSwitchesGUI(XMLEle *root, QString & errmsg);
    int buildMenuGUI    (INDI_P *pp, XMLEle *root, QString & errmsg);
    int buildLightsGUI  (XMLEle *root, QString & errmsg);
    int buildBLOBGUI    (XMLEle *root, QString & errmsg);

    /*****************************************************************
    * Add
    ******************************************************************/
    INDI_P *  addProperty (XMLEle *root, QString & errmsg);

    /*****************************************************************
    * Find
    ******************************************************************/
    INDI_P *   findProp    (const QString &name);
    INDI_E *   findElem    (const QString &name);
    INDI_G *   findGroup   (const QString &grouptag, int create, QString & errmsg);
    int        findPerm    (INDI_P *pp  , XMLEle *root, PPerm *permp, QString & errmsg);

    /*****************************************************************
    * Set/New
    ******************************************************************/
    int setValue       (INDI_P *pp, XMLEle *root, QString & errmsg);
    int setLabelState  (INDI_P *pp, XMLEle *root, QString & errmsg);
    int setTextValue   (INDI_P *pp, XMLEle *root, QString & errmsg);
    int setBLOB        (INDI_P *pp, XMLEle * root, QString & errmsg);

    int newValue       (INDI_P *pp, XMLEle *root, QString & errmsg);
    int newTextValue   (INDI_P *pp, XMLEle *root, QString & errmsg);

    int setAnyCmd      (XMLEle *root, QString & errmsg);
    int newAnyCmd      (XMLEle *root, QString & errmsg);

    int  removeProperty(INDI_P *pp);

    /*****************************************************************
    * Crack
    ******************************************************************/
    int crackLightState  (char *name, PState *psp);
    int crackSwitchState (char *name, PState *psp);

    /*****************************************************************
    * Data processing
    ******************************************************************/
    int processBlob(INDI_E *blobEL, XMLEle *ep, QString & errmsg);

    /*****************************************************************
    * INDI standard property policy
    ******************************************************************/
    bool isOn();
    void registerProperty(INDI_P *pp);
    bool isINDIStd(INDI_P *pp);

public slots:
    void engageTracking();
    void setBLOBOption(int state);



};

#endif
