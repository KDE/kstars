/*  INDI Element
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    2004-01-15	INDI element is the most basic unit of the INDI KStars client.
 */

#ifndef INDIELEMENT_H_
#define INDIELEMENT_H_

#include <QDialog>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <indiapi.h>
#include <indiproperty.h>

#include "indicommon.h"

/* Forward decleration */
class KLed;
class QLineEdit;
class QDoubleSpinBox;
class QPushButton;
class KSqueezedTextLabel;

class QLabel;
class QHBoxLayout;
class QVBoxLayout;
class QSpacerItem;
class QCheckBox;
class QButtonGroup;
class QSlider;

class INDI_P;

/**
 * @class INDI_E
 *  INDI_E represents an INDI GUI element (Number, Text, Switch, Light, or BLOB) within an INDI property. It is the most basic
 * GUI representation of property elements.
 *
 * @author Jasem Mutlaq
 */
class INDI_E : public QObject
{
    Q_OBJECT
public:
    INDI_E(INDI_P *gProp, INDI::Property *dProp);
    ~INDI_E();

    const QString &getLabel() { return label; }
    const QString &getName() { return name;}

    QString getWriteField();
    QString getReadField();


    void buildSwitch(QButtonGroup* groupB, ISwitch *sw);
    void buildMenuItem(ISwitch *sw);
    void buildText(IText *itp);
    void buildNumber(INumber *inp);
    void buildLight(ILight *ilp);
    void buildBLOB(IBLOB *ibp);

    // Updates GUI from data in INDI properties
    void syncSwitch();
    void syncText();
    void syncNumber();
    void syncLight();

    // Save GUI data in INDI properties
    void updateTP();
    void updateNP();

    void setText(const QString & newText);

    void setMin ();
    void setMax ();

    void setupElementLabel();
    void setupElementRead(int length);
    void setupElementWrite(int length);
    void setupElementScale(int length);
    void setupBrowseButton();

    bool getBLOBDirty() { return blobDirty; }
    void setBLOBDirty(bool isDirty) { blobDirty = isDirty; }


private:
    QString name;			/* name */
    QString label;			/* label is the name by default, unless specified */

    INDI::Property *dataProp;           /* parent DATA property */
    INDI_P *guiProp;			/* parent GUI property */

    QHBoxLayout    *EHBox;   		/* Horizontal layout */

    /* GUI widgets, only malloced when needed */
    KSqueezedTextLabel *label_w;	// label
    QLineEdit	   *read_w;		// read field
    QLineEdit	   *write_w;		// write field
    KLed	   *led_w;		// light led
    QDoubleSpinBox *spin_w;		// spinbox
    QSlider	   *slider_w;		// Slider
    QPushButton    *push_w;		// push button
    QPushButton    *browse_w;		// browse button
    QCheckBox      *check_w;		// check box
    QSpacerItem    *hSpacer;		// Horizontal spacer

    ISwitch        *sp;
    INumber        *np;
    IText          *tp;
    ILight         *lp;
    IBLOB          *bp;

    bool blobDirty;
    QString text;			// current text

public slots:
    void spinChanged(double value);
    void sliderChanged(int value);
    void browseBlob();


};

#endif
