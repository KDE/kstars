/*  INDI Property
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
   
 */

#ifndef INDIPROPERTY_H_
#define INDIPROPERTY_H_

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <indiproperty.h>

#include "indicommon.h"

class INDI_G;
class INDI_E;

class QComboBox;
class KLed;
class KSqueezedTextLabel;

class QPushButton;
class QHBoxLayout;
class QVBoxLayout;
class QButtonGroup;
class QAbstractButton;
class QCheckBox;

/**
 * @class INDI_P
 * INDI_P represents a single INDI property (Switch, Text, Number, Light, or BLOB). It handles building the GUI and updating the property status and/or value as new data
 * arrive from INDI Serve. It also sends any changes in the property value back to INDI server via the ClientManager.
 *
 * @author Jasem Mutlaq
 */
class INDI_P : public QObject
{
    Q_OBJECT
public:
    INDI_P(INDI_G *ipg, INDI::Property *prop);
    ~INDI_P();

    /* Draw state LED */
    void updateStateLED();

    /* Update menu gui */
    void updateMenuGUI();

    void initGUI();

    /* First step in adding a new GUI element */
    //void addGUI(XMLEle *root);

    /* Set Property's parent group */
    //void setGroup(INDI_G *parentGroup) { pg = parentGroup; }

    void buildSwitchGUI();
    void buildMenuGUI();
    void buildTextGUI();
    void buildNumberGUI();
    void buildLightGUI();
    void buildBLOBGUI();

    /* Setup the 'set' button in the property */
    void setupSetButton(const QString &caption);

    void newTime();

    PGui getGUIType() { return guiType;}

    INDI_G *getGroup() { return pg;}

    QHBoxLayout * getContainer() { return PHBox; }

    const QString  &getName() { return name; }

    void addWidget(QWidget *w);
    void addLayout(QHBoxLayout *layout);

    INDI_E * getElement(const QString &elementName);

    QList<INDI_E *> getElements() { return elementList; }
private:

    INDI::Property *dataProp;
    INDI_G	*pg;			/* parent group */
    QCheckBox   *enableBLOBC;
    KSqueezedTextLabel      *labelW;		/* Label widget */
    QPushButton *setB;		        /* set button */
    KLed	*ledStatus;		/* state LED */
    PGui         guiType;		/* type of GUI, if any */


    QSpacerItem    *horSpacer;		/* Horizontal spacer */
    QHBoxLayout    *PHBox;   		/* Horizontal container */
    QVBoxLayout    *PVBox;   		/* Vertical container */

    QButtonGroup   *groupB;		/* group button for radio and check boxes (Elements) */
    QComboBox      *menuC;		/* Combo box for menu */

    QString         name;

    QList<INDI_E*> elementList;		/* list of elements */

public slots:
    void processSetButton();
    void newSwitch(QAbstractButton * button);
    void newSwitch(int index);
    void newSwitch(const QString & name);


    void sendBlob();
    void sendSwitch();
    void sendText();

    void setBLOBOption(int state);
};

#endif
