/*  INDI Device
    Copyright (C) 2003 Elwood C. Downey
    		       Jasem Mutlaq

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

#include "indi/lilxml.h"

#define	INDIVERSION	1.0	/* we support this or less */

/* decoded elements.
 * lights use PState, TB's use the alternate binary names.
 */
typedef enum {PS_IDLE = 0, PS_OK, PS_BUSY, PS_ALERT, PS_N} PState;
#define	PS_OFF	PS_IDLE		/* alternate name */
#define	PS_ON	PS_OK		/* alternate name */
typedef enum {PP_RW = 0, PP_WO, PP_RO} PPerm;
typedef enum {PG_NONE = 0, PG_TEXT, PG_NUMERIC, PG_SCALE, PG_BUTTONS,
    PG_RADIO, PG_MENU, PG_LIGHTS} PGui;

#define	MAXSCSTEPS	1000	/* max number of steps in a scale */
#define MAXRADIO	4

class INDI_D;
class INDI_P;
class INDI_G;
class INDI_L;
class INDIMenu;

class KLed;
class KLineEdit;
class KComboBox;
class KDoubleSpinBox;
class KPushButton;

class QLabel;
class QHBoxLayout;
class QVBoxLayout;
class QFrame;
class QLineEdit;
class QString;
class QStringList;
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
** The Tree
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

class DeviceManager : public QObject
{
   Q_OBJECT
   public:
   DeviceManager(INDIMenu *INDIparent);
   ~DeviceManager();

   INDIMenu *parent;

   std::vector<INDI_D *> indi_dev;

   int			serverFD;
   FILE			*serverFP;
   LilXML		*XMLParser;
   QSocketNotifier *sNotifier;

   int dispatchCommand (XMLEle *root, char errmsg[]);

   INDI_D *  addDevice   (XMLEle *dep , char errmsg[]);

   //TODO
   int       removeDevice(const char *name) { return -1;}
   INDI_D *  findDev     (XMLEle *root, int  create, char errmsg[]);

   /*****************************************************************
   * Send to server
   ******************************************************************/
   void sendNewString  (INDI_P *pp, const char *txt);
   void sendNewSwitch  (INDI_L *lp, int set);

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


/* INDI label for buttons and lights */
class INDI_L
{
 public:
  INDI_L(INDI_P *parentProperty, char *inName);
  ~INDI_L() {}
    char *name;			/* malloced label string */
    PState state;		/* control on/off t/f etc */
    INDI_P *pp;			/* parent property */
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
    float	timeout;		/* timeout, seconds */
    PState	state;			/* state light code */
    KLed	*light;
    QLabel      *label_w;

    PGui guitype;			/* type of GUI, if any */
    union
    {
	struct
	{
	    QLabel 	*read_w;	/* read Label widget, or NULL */
	    KLineEdit   *write_w;	/* write TF widget, or NULL */
	    QPushButton *set_w;
	    PPerm perm;			/* permissions wrt client */
	} text;				/* PG_TEXT or PG_NUMERIC */
	struct
	{
	    QLabel 	   *read_w;
	    KDoubleSpinBox *spin_w;	/* "scale" TF widget, or NULL */
	    QPushButton    *set_w;
	    PPerm perm;			/* permissions wrt client */
	    double min, max, step;	/* params for scale */
	} scale;			/* PG_SCALE */
	struct
	{
	    std::vector<INDI_L *> *labels;/* malloced list of pointers to lights */
	    KComboBox *om_w;		/* option menu -- PG_MENU only */
	    QButtonGroup *groupB;
	    QHBoxLayout *hLayout;   /* Horizontal layout for lights only */

	    // vectors to keep track of heap widgets for later clean up
	    std::vector<QPushButton *> *push_v;
	    std::vector<QCheckBox   *> *check_v;
	    std::vector<QLabel      *> *label_v;
	    std::vector<KLed        *> *led_v;
	    int lastItem;

	} multi;			/* PG_RADIO, BUTTONS, LIGHTS */
    } u;

    void drawLt(KLed *w, PState lstate);
    void setGroup(INDI_G *parentGroup) { pg = parentGroup; }

    public slots:
    void newText();
    void newSwitch(int id);
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
    int uniHeight;

    INDI_G       *curGroup;

    INDIMenu    *parent;
    DeviceManager *parentMgr;

   /*****************************************************************
   * Build
   ******************************************************************/
   int buildTextGUI    (XMLEle *root, char errmsg[]);
   int buildRawTextGUI (INDI_P *pp, char *initstr, PPerm p, char errmsg[]);
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
   int setScaleValue  (INDI_P *pp, XMLEle *root, char errmsg[]);
   int setLabelState  (INDI_P *pp, XMLEle *root, char errmsg[]);
   int setMenuChoice  (INDI_P *pp, XMLEle *root, char errmsg[]);
   int setTextValue   (INDI_P *pp, XMLEle *root, char errmsg[]);

   int newValue       (INDI_P *pp, XMLEle *root, char errmsg[]);
   int newScaleValue  (INDI_P *pp, XMLEle *root, char errmsg[]);
   int newTextValue   (INDI_P *pp, XMLEle *root, char errmsg[]);

   int setAnyCmd      (XMLEle *root, char errmsg[]);
   int newAnyCmd      (XMLEle *root, char errmsg[]);

   int  removeProperty(INDI_P *pp, char errmsg[]);
   void resizeGroups();

   /*****************************************************************
   * Crack
   ******************************************************************/
   int crackLightState  (char *name, PState *psp);
   int crackSwitchState (char *name, PState *psp);

};

#endif
