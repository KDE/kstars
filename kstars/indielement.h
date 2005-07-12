/*  INDI Element
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    2004-01-15	INDI element is the most basic unit of the INDI KStars client.
 */

#ifndef INDIELEMENT_H
#define INDIELEMENT_H

#include <kdialogbase.h>
#include <unistd.h>
#include <qstring.h>
#include <qptrlist.h>

#include "indi/lilxml.h"

#define	INDIVERSION	1.5	/* we support this or less */

/* GUI layout */
#define PROPERTY_LABEL_WIDTH	80
#define ELEMENT_LABEL_WIDTH	175
#define ELEMENT_READ_WIDTH	175
#define ELEMENT_WRITE_WIDTH	175
#define ELEMENT_FULL_WIDTH	340
#define MIN_SET_WIDTH		50
#define MAX_SET_WIDTH		110
#define MAXINDINAME		32
#define MED_INDI_FONT		2
#define MAX_LABEL_LENGTH	20

// Pulse tracking
#define INDI_PULSE_TRACKING   15000

/* decoded elements.
 * lights use PState, TB's use the alternate binary names.
 */
typedef enum {PS_IDLE = 0, PS_OK, PS_BUSY, PS_ALERT, PS_N} PState;
#define	PS_OFF	PS_IDLE		/* alternate name */
#define	PS_ON	PS_OK		/* alternate name */
typedef enum {PP_RW = 0, PP_WO, PP_RO} PPerm;
typedef enum {PG_NONE = 0, PG_TEXT, PG_NUMERIC, PG_BUTTONS,
  PG_RADIO, PG_MENU, PG_LIGHTS, PG_BLOB} PGui;

/* INDI std properties */
/* N.B. Need to modify corresponding entry in indidevice.cpp when changed */    
enum stdProperties { CONNECTION, DEVICE_PORT, TIME, SDTIME, GEOGRAPHIC_COORD,   /* General */
                     EQUATORIAL_COORD, EQUATORIAL_EOD_COORD, HORIZONTAL_COORD,  /* Telescope */
                     ABORT_MOTION, ON_COORD_SET, SOLAR_SYSTEM, MOVEMENT, PARK,  /* Telescope */
                     CCD_EXPOSE_DURATION, CCD_TEMPERATURE, CCD_FRAME,           /* CCD */
                     CCD_FRAME_TYPE, CCD_BINNING, CCD_INFO, CCDPREVIEW_STREAM,  /* CCD */
                     CCDPREVIEW_CTRL,						/* CCD */
                     VIDEO_STREAM,						/* Video */
		     FOCUS_SPEED, FOCUS_MOTION, FOCUS_TIMER,			/* Focuser */
                     FILTER_SLOT};						/* Filter */
		     
/* Devices families that we explicity support (i.e. with std properties) */
enum deviceFamily { KSTARS_TELESCOPE, KSTARS_CCD, KSTARS_FILTER, KSTARS_VIDEO, KSTARS_FOCUSER, KSTARS_DOME, KSTARS_GPS };

#define	MAXSCSTEPS	1000	/* max number of steps in a scale */
#define MAXRADIO	4	/* max numbere of buttons in a property */

/* Forward decleration */
class KLed;
class KLineEdit;
class KDoubleSpinBox;
class KPushButton;
class KCheckBox;
class KSqueezedTextLabel;

class QLabel;
class QHBoxLayout;
class QVBoxLayout;
class QSpacerItem;
class QCheckBox;
class QSlider;

class INDI_P;

/* Useful XML functions */
XMLAtt *   findAtt     (XMLEle *ep  , const char *name , char errmsg[]);
XMLEle *   findEle     (XMLEle *ep  , INDI_P *pp, const char *child, char errmsg[]);

/* INDI Element */
class INDI_E : public QObject
{
 Q_OBJECT
 public:
  INDI_E(INDI_P *parentProperty, QString inName, QString inLabel);
  ~INDI_E();
    QString name;			/*  name */
    QString label;			/* label is the name by default, unless specefied */
    PState state;			/* control on/off t/f etc */
    INDI_P *pp;				/* parent property */

    QHBoxLayout    *EHBox;   		/* Horizontal layout */
    
    /* GUI widgets, only malloced when needed */
    KSqueezedTextLabel *label_w;	// label
    KLineEdit	   *read_w;		// read field
    KLineEdit	   *write_w;		// write field
    KLed	   *led_w;		// light led
    KDoubleSpinBox *spin_w;		// spinbox
    QSlider	   *slider_w;		// Slider
    KPushButton    *push_w;		// push button
    KPushButton    *browse_w;		// browse button
    QCheckBox      *check_w;		// check box
    QSpacerItem    *hSpacer;		// Horizontal spacer
    
    double min, max, step;		// params for scale
    double value;			// current value
    double targetValue;			// target value
    QString text;			// current text
    QString format;			// number format, if applicable
    
   int buildTextGUI    (QString initText);
   int buildNumberGUI  (double initValue);
   int buildLightGUI();
   int buildBLOBGUI();
   void drawLt();
   
   void initNumberValues(double newMin, double newMax, double newStep, char * newFormat);
   void updateValue(double newValue);
   void setMin (double inMin);
   void setMax (double inMax);
   
   void setupElementLabel();
   void setupElementRead(int length);
   void setupElementWrite(int length);
   void setupElementScale(int length);
   void setupBrowseButton();
   
   public slots:
   void spinChanged(double value);
   void sliderChanged(int value);
   void browseBlob();

};

#endif
