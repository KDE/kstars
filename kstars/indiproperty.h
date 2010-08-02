/*  INDI Property
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
   
 */

#ifndef INDIPROPERTY_H
#define INDIPROPERTY_H

#include "indielement.h"


class INDI_G;
class INDIStdProperty;

class KPopupMenu;
class KComboBox;
class KLed;

class TQLabel;
class TQPushButton;
class TQHBoxLayout;
class TQVBoxLayout;
class TQButtonGroup;



/* INDI property */
class INDI_P : public QObject
{
  Q_OBJECT
   public:
   INDI_P(INDI_G *parentGroup, TQString inName);
   ~INDI_P();

    QString	name;			/* property name */
    TQString     label;			/* property label */
    
    INDI_G	*pg;			/* parent group */
    KPopupMenu  *assosiatedPopup;	/* assosiated popup menu, if any */
    INDIStdProperty *indistd;		/* Assosciated std routines class */
    double	timeout;		/* timeout, seconds */
    PState	state;			/* state light code */
    KLed	*light;			/* state LED */
    PPerm       perm;		        /* permissions wrt client */
    PGui        guitype;		/* type of GUI, if any */

    
    int 	stdID;			/* Standard property ID, if any */
    
    TQLabel      *label_w;		/* Label widget */
    TQPushButton *set_w;		        /* set button */
    
    TQSpacerItem    *HorSpacer;		/* Horizontal spacer */
    TQHBoxLayout    *PHBox;   		/* Horizontal container */
    TQVBoxLayout    *PVBox;   		/* Vertical container */
    
    TQButtonGroup   *groupB;		/* group button for radio and check boxes (Elements) */
    KComboBox      *om_w;		/* Combo box for menu */
    
    TQPtrList<INDI_E> el;		/* list of elements */

    /* Draw state LED */
    void drawLt(PState lstate);
    
    /* First step in adding a new GUI element */
    void addGUI(XMLEle *root);
    
    /* Set Property's parent group */
    void setGroup(INDI_G *parentGroup) { pg = parentGroup; }
    
    /* Find an element within the property */
    INDI_E * findElement(TQString elementName);
    /* Search for an element, and if found, evaluate its state */
    bool isOn(TQString component);	
   
   /* Build Functions */ 
   int buildTextGUI    (XMLEle *root, char errmsg[]);
   int buildNumberGUI  (XMLEle *root, char errmsg[]);
   int buildSwitchesGUI(XMLEle *root, char errmsg[]);
   int buildMenuGUI    (XMLEle *root, char errmsg[]);
   int buildLightsGUI  (XMLEle *root, char errmsg[]);
   int buildBLOBGUI    (XMLEle *root, char errmsg[]);
      
   /* Setup the 'set' button in the property */
    void setupSetButton(TQString caption);
    
   /* Turn a switch on */
    void activateSwitch(TQString name);
    
    public slots:
    void newText();
    void newSwitch(int id);
    void newBlob();
    void convertSwitch(int id);
    
    signals:
    void okState();
    
};

#endif
