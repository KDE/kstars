#ifndef INDI_D_H
#define INDI_D_H

/*  GUI Device Manager
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <KDialog>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <libindi/indiapi.h>
#include <libindi/basedevice.h>

class QTextEdit;
class QTabWidget;
class QSplitter;
class GUIManager;
class ClientManager;
class INDI_G;

class INDI_D : public KDialog
{
    Q_OBJECT
public:

    //enum DTypes { DATA_FITS, ASCII_DATA_STREAM, VIDEO_STREAM, DATA_OTHER, DATA_CCDPREVIEW };

    INDI_D(GUIManager *in_manager, INDI::BaseDevice *in_idv, ClientManager *in_cm);
    ~INDI_D();

    QSplitter *getDeviceBox() { return deviceVBox; }

    void setTabID(int newTabID) { tabID = newTabID; }
    int getTabID() { return tabID; }

    ClientManager *getClientManager() { return clientManager; }

    INDI_G *getGroup (const QString & groupName);

    INDI::BaseDevice *getBaseDevice() { return dv;}

    QList<INDI_G *> getGroups() { return groupsList; }

    void clearMessageLog();

public slots:

    /*****************************************************************
    * Build
    ******************************************************************/
    bool buildProperty   (INDI::Property * prop);
    bool removeProperty   (INDI::Property * prop);
    bool updateSwitchGUI(ISwitchVectorProperty *svp);
    bool updateTextGUI    (ITextVectorProperty *tvp);
    bool updateNumberGUI  (INumberVectorProperty *nvp);
    bool updateLightGUI  (ILightVectorProperty *lvp);
    bool updateBLOBGUI  (IBLOB *bp);

    void updateMessageLog(INDI::BaseDevice *idv, int messageID);
    /*
    bool buildTextGUI    (ITextVectorProperty *tvp, QString & errmsg);
    bool buildNumberGUI  (INumberVectorProperty *nvp, QString & errmsg);
    bool buildSwitchesGUI(ISwitchVectorProperty *svp, QString & errmsg);
    bool buildMenuGUI    (ISwitchVectorProperty *svp, QString & errmsg);
    bool buildLightsGUI  (ILightVectorProperty *lvp, QString & errmsg);
    bool buildBLOBGUI    (IBLOBVectorProperty *bvp, QString & errmsg);*/


    /*****************************************************************
    * Set/New
    ******************************************************************/
    /*int setValue       (INDI_P *pp, XMLEle *root, QString & errmsg);
    int setLabelState  (INDI_P *pp, XMLEle *root, QString & errmsg);
    int setTextValue   (INDI_P *pp, XMLEle *root, QString & errmsg);
    int setBLOB        (INDI_P *pp, XMLEle * root, QString & errmsg);

    int newValue       (INDI_P *pp, XMLEle *root, QString & errmsg);
    int newTextValue   (INDI_P *pp, XMLEle *root, QString & errmsg);

    int setAnyCmd      (XMLEle *root, QString & errmsg);
    int newAnyCmd      (XMLEle *root, QString & errmsg);

    int  removeProperty(INDI_P *pp);

*/

private:

    QString 	name;			/* device name */
    QSplitter   *deviceVBox;
    QTabWidget  *groupContainer;	/* Groups within the device */
    QTextEdit	*msgST_w;		/* scrolled text for messages */

    INDI::BaseDevice *dv;
    ClientManager *clientManager;
    GUIManager *guiManager;
    int tabID;

    QList<INDI_G *> groupsList;

/*signals:
    void newSwitch(const QString &sw, ISState value);
    void newNumber(const QString &nm, double value);
    void newText(const QString &tx, QString value);
    void newLight(const QString &lg, IPState value);
    void newBLOB(const QString &bb, unsigned char *buffer, int bufferSize, const QString &dataFormat, INDI_D::DTypes dataType);
    void newProperty(INDI_P *p);
 */

};

#endif // INDI_D_H
