/*  INDI Device
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)
    		       Elwood C. Downey

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef INDIDEVICE_H
#define INDIDEVICE_H

#include <kdialogbase.h>
#include <unistd.h>
#include <vector>
#include <qvaluelist.h>

#include "indi/lilxml.h"

#define	INDIVERSION	1.11	/* we support this or less */

/* decoded elements.
 * lights use PState, TB's use the alternate binary names.
 */
typedef enum {PS_IDLE = 0, PS_OK, PS_BUSY, PS_ALERT, PS_N} PState;
#define	PS_OFF	PS_IDLE		/* alternate name */
#define	PS_ON	PS_OK		/* alternate name */
typedef enum {PP_RW = 0, PP_WO, PP_RO} PPerm;
typedef enum {PG_NONE = 0, PG_TEXT, PG_NUMERIC, PG_BUTTONS,
    PG_RADIO, PG_MENU, PG_LIGHTS} PGui;

#define	MAXSCSTEPS	1000	/* max number of steps in a scale */
#define MAXRADIO	4

class DeviceManager;
class INDI_D;
class INDI_P;
class INDI_G;
class INDI_L;
class INDIMenu;
class SkyObject;

class KLed;
class KLineEdit;
class KComboBox;
class KDoubleSpinBox;
class KPushButton;
class KPopupMenu;

class QTable;
class QLabel;
class QHBoxLayout;
class QVBoxLayout;
class QFrame;
class QLineEdit;
class QString;
class QTextEdit;
class QListView;
class QTabWidget;
class QSpacerItem;
class QGroupBox;
class QGridLayout;
class QButtonGroup;
class QCheckBox;
class QScrollView;
class QSocketNotifier;
class QVBox;

/*************************************************************************
** The INDI Tree
**
** INDI_LABEL <------------------------------------------
**     |						|
**     -----> INDI_PROPERTY				|
**                        |				|
**                        -----> INDI_GROUP		|
**                                        |		|
**                                        -----> INDI_DEVICE
**                                               |         |
                                          Device Manager  INDI Menu
**************************************************************************/

/* INDI label for various widgets */
class INDI_L
{
 public:
  INDI_L(INDI_P *parentProperty, char *inName, char *inLabel);
  ~INDI_L();
    char *name;				//  name
    char *label;			// label is the name by default, unless specefied
    PState state;			/* control on/off t/f etc */
    INDI_P *pp;				/* parent property */

    QLabel         *label_w;		// label widget (lights)
    KDoubleSpinBox *spin_w;		// spinbox
    QPushButton    *push_w;		// push button
    QCheckBox      *check_w;		// check box
    KLed           *led_w;		// light led
    double min, max, step;		// params for scale
    double value;			// current value
    char *text;				// current text
    char *format;			// number format, if applicable
};

/* INDI property */
class INDI_P : public QObject
{
  Q_OBJECT
   public:
   INDI_P(INDI_G *parentGroup, char *inName);
   ~INDI_P();

    char	*name;			/* malloced name */
    INDI_G	*pg;			/* parent group */
    float	timeout;		/* TODO timeout, seconds */
    PState	state;			/* state light code */
    KLed	*light;
    QLabel      *label_w;
    bool        isINDIStd;		/* is it an INDI std policy? */
    KPopupMenu  *parentPopup;		/* Parent popup menu, if any */
    PPerm perm;			        /* permissions wrt client */


    PGui guitype;			/* type of GUI, if any */
    QPushButton    *set_w;		// set button
    QTable         *table_w;
    KComboBox      *om_w;		// option menu -- PG_MENU only
    QHBoxLayout    *hLayout;   		// Horizontal layout
    QButtonGroup   *groupB;		// group button for radio and check boxes
    std::vector<INDI_L *>      labels;

    void drawLt(KLed *w, PState lstate);
    void setGroup(INDI_G *parentGroup) { pg = parentGroup; }
    INDI_L * findLabel(const char *lname);

    void updateTime();
    void updateLocation();
    bool isOn(QString component);


    public slots:
    void newText();
    void newSwitch(int id);
    void convertSwitch(int id);
    void newTime();

};

/* INDI group */
class INDI_G
{
  public:
  INDI_G(INDI_D *parentDevice, char *inName);
  ~INDI_G();

  QGroupBox   *box;
  QGridLayout *layout;

  INDI_D *dp;

  void addProperty(INDI_P *pp);
  int removeProperty(INDI_P *pp);


  std::vector<INDI_P      *> pl;	/* malloced list of pointers to properties */

};

/* INDI device */
class INDI_D : public QObject
{
 Q_OBJECT
  public:
   INDI_D(INDIMenu *parentMenu, DeviceManager *parentManager, char *inName);
   ~INDI_D();

    char 	*name;			/* malloced name */
    QFrame	*tabContainer;
    QTextEdit	*msgST_w;		/* scrolled text for messages */
    QScrollView *sv;
    QVBoxLayout *mainLayout;
    QVBox       *propertyLayout;
    QSpacerItem *vSpacer;
    QSpacerItem *hSpacer;

    QPushButton  *clear;
    QPushButton  *savelog;
    QHBoxLayout  *buttonLayout;

    std::vector<INDI_G      *> gl;	/* malloced list of pointers to groups */

    int biggestLabel;
    int widestTableCol;
    int uniHeight;
    int initDevCounter;

    INDI_G        *curGroup;
    bool	  INDIStdSupport;

    INDIMenu      *parent;
    DeviceManager *parentMgr;

   /*****************************************************************
   * Build
   ******************************************************************/
   int buildTextGUI    (XMLEle *root, char errmsg[]);
   int buildRawTextGUI (INDI_P *pp, char *label, char *initstr, int enableSet, char errmsg[]);
   int buildNumberGUI  (XMLEle *root, char errmsg[]);
   int buildSwitchesGUI(XMLEle *root, char errmsg[]);
   int buildMenuGUI    (INDI_P *pp, XMLEle *root, char errmsg[]);
   int buildLightsGUI  (XMLEle *root, char errmsg[]);

   /*****************************************************************
   * Add
   ******************************************************************/
   INDI_P *  addProperty (XMLEle *root, char errmsg[]);
   int       addGUI      (INDI_P *pp  , XMLEle *root, char errmsg[]);

   /*****************************************************************
   * Find
   ******************************************************************/
   INDI_P *   findProp    (const char *name);
   INDI_G *   findGroup   (char *grouptag, int create, char errmsg[]);
   int        findPerm    (INDI_P *pp  , XMLEle *root, PPerm *permp, char errmsg[]);

   /*****************************************************************
   * Set/New
   ******************************************************************/
   int setValue       (INDI_P *pp, XMLEle *root, char errmsg[]);
   int setLabelState  (INDI_P *pp, XMLEle *root, char errmsg[]);
   int setTextValue   (INDI_P *pp, XMLEle *root, char errmsg[]);

   int newValue       (INDI_P *pp, XMLEle *root, char errmsg[]);
   int newTextValue   (INDI_P *pp, XMLEle *root, char errmsg[]);

   int setAnyCmd      (XMLEle *root, char errmsg[]);
   int newAnyCmd      (XMLEle *root, char errmsg[]);

   int  removeProperty(INDI_P *pp, char errmsg[]);
   void resizeGroups();
   void resizeTableHeaders();

   /*****************************************************************
   * Crack
   ******************************************************************/
   int crackLightState  (char *name, PState *psp);
   int crackSwitchState (char *name, PState *psp);

   /*****************************************************************
   * INDI standard property policy
   ******************************************************************/
   bool isOn();
   void registerProperty(INDI_P *pp);
   bool isINDIStd(INDI_P *pp);
   bool handleNonSidereal(SkyObject *o);
   void initDeviceOptions();
   void handleDevCounter();

   SkyObject *currentObject;
   QTimer *devTimer;

   public slots:
   void timerDone();

};

// INDI device manager
class DeviceManager : public QObject
{
   Q_OBJECT
   public:
   DeviceManager(INDIMenu *INDIparent, int inID);
   ~DeviceManager();

   INDIMenu *parent;

   std::vector<INDI_D *> indi_dev;

   int			mgrID;
   int			serverFD;
   FILE			*serverFP;
   LilXML		*XMLParser;
   QSocketNotifier 	*sNotifier;

   int dispatchCommand (XMLEle *root, char errmsg[]);

   INDI_D *  addDevice   (XMLEle *dep , char errmsg[]);
   INDI_D *  findDev     (XMLEle *root, int  create, char errmsg[]);

   /*****************************************************************
   * Send to server
   ******************************************************************/
   void sendNewText  (INDI_P *pp);
   void sendNewNumber (INDI_P *pp);
   void sendNewSwitch  (INDI_P *pp);

   /*****************************************************************
   * Misc.
   ******************************************************************/
   int  delPropertyCmd (XMLEle *root, char errmsg[]);
   int  removeDevice(char *devName, char errmsg[]);
   INDI_D *  findDev(char *devName, char errmsg[]);

   int  messageCmd     (XMLEle *root, char errmsg[]);
   void checkMsg       (XMLEle *root, INDI_D *dp);
   void doMsg          (XMLEle *msg , INDI_D *dp);

   bool indiConnect(QString host, QString port);

  public slots:
   void dataReceived();

};

#endif
