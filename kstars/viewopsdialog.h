/***************************************************************************
                          viewopsdialog.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Apr 28 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/




#ifndef VIEWOPSDIALOG_H
#define VIEWOPSDIALOG_H

#include <kdialogbase.h>

class QHBoxLayout;
class QVBoxLayout;
class QGridLayout;
class QGroupBox;
class QButtonGroup;
class QRadioButton;
class QTabWidget;
class QWidget;
class QCheckBox;
class QFrame;
class QPushButton;
class QColor;
class MagnitudeSpinBox;
class KIntSpinBox;

/**
	*Dialog containing controls for all user-configuration options in KStars,
	*including color schemes, display toggles, and coordinate system.
	*@short User configuration dialog.
  *@author Jason Harris
	*@version 0.4
  */

class ViewOpsDialog : public KDialogBase  {
  Q_OBJECT
public:
/**
	*Constructor.  Create all widgets, and pack them into the hierarchy
	*of QLayouts, QFrames, and Tabs.	Connect Signals to Slots.
	*/
	ViewOpsDialog( QWidget *parent = 0 );
/**
	*Destructor (empty).
	*/
	~ViewOpsDialog();

private:
	QHBoxLayout *hlay, *ColorButtonsLayout;
	QVBoxLayout *vlayStarsTab, *vlayCatTab, *vlayGuideTab, *vlayPlanetTab, *vlayColorTab, *DisplayBoxLayout;
	QGridLayout *glayColorTab, *glayPlanetTab;

	QGroupBox  *DisplayBox;
	QTabWidget *DisplayTabs;
	QWidget    *StarsTab, *CatalogTab, *GuideTab, *PlanetTab, *ColorTab;
	
	QCheckBox *showBSC;
	QCheckBox *showMessier, *showMessImages;
	QCheckBox *showNGC;
	QCheckBox *showIC;

	// Star options
	MagnitudeSpinBox *magSpinBoxDrawStars;
	MagnitudeSpinBox *magSpinBoxDrawStarInfo;
	QCheckBox *showStarNames;
	QCheckBox *showStarMagnitude;
		
	QCheckBox *showConstellLines;
	QCheckBox *showConstellNames;
	QCheckBox *useLatinConstellNames;
	QCheckBox *showMilkyWay;
	QCheckBox *showEquator;
	QCheckBox *showEcliptic;
	QCheckBox *showHorizon;
	QCheckBox *showGround;

	QCheckBox *showSun, *showMoon;
	QCheckBox *showMercury, *showVenus, *showMars, *showJupiter;
	QCheckBox *showSaturn, *showUranus, *showNeptune, *showPluto;
	
	QPushButton *ChangeCSky;
//	QPushButton *ChangeCStar;
	QPushButton *ChangeCMess;
	QPushButton *ChangeCNGC;
	QPushButton *ChangeCIC;
	QPushButton *ChangeCHST;
	QPushButton *ChangeCMilkyWay;
	QPushButton *ChangeCEquator;
	QPushButton *ChangeCEcliptic;
	QPushButton *ChangeCHorizon;
	QPushButton *ChangeCConstLine;
	QPushButton *ChangeCConstText;
	QPushButton *ChangeCStarText;
	
	QPushButton *NightColors;
	QPushButton *ResetColors;

	QButtonGroup *CoordsGroup;
	QRadioButton *EquatRadio;
	QRadioButton *AltAzRadio;

	KIntSpinBox *IntensityBox;

private slots:
/**
	*Choose a new Sky color with a QColorDialog.
	*/
	void newSkyColor( void );
/**
	*Choose a new Star color with a QColorDialog.
	*/
//	void newStarColor( void );
/**
	*Choose a new Messier-object color with a QColorDialog.
	*/
	void newMessColor( void );
/**
	*Choose a new NGC-object color with a QColorDialog.
	*/
	void newNGCColor( void );
/**
	*Choose a new IC-object color with a QColorDialog.
	*/
	void newICColor( void );
/**
	*Choose a new HST-object color with a QColorDialog.
	*/
	void newHSTColor( void );
/**
	*Choose a new Milky Way color with a QColorDialog.
	*/
	void newMWColor( void );
/**
	*Choose a new Equator color with a QColorDialog.
	*/
	void newEqColor( void );
/**
	*Choose a new Ecliptic color with a QColorDialog.
	*/
	void newEclColor( void );
/**
	*Choose a new Horizon color with a QColorDialog.
	*/
	void newHorzColor( void );
/**
	*Choose a new Constellation-line color with a QColorDialog.
	*/
	void newCLineColor( void );
/**
	*Choose a new Constellation-name color with a QColorDialog.
	*/
	void newCNameColor( void );
/**
	*Choose a new Star-name color with a QColorDialog.
	*/
	void newSNameColor( void );
/**
	*Select the "Night Vision" preset color scheme/
	*/
	void redColors( void );
/**
	*Select the default preset color scheme.
	*/
	void defaultColors( void );
/**
	*Sync the KStars display with a newly-changed option.
	*/
	void updateDisplay( void );
/**
	*Switch between Equatorial (RA, Dec) and Horizontal (Az, Alt)
	*coordinate systems.
	*/	
	void changeCoordSys( void );

/**
  * We have a new minimum star magnitude (brightness) for the display
  */
	void changeMagDrawStars( int newValue );

/**
  * We have a new minimum star magnitude (brightness) for the displaying star information
  */
	void changeMagDrawInfo( int newValue );
	
/**
	* Set the intensity of starcolors.
	*/
	void changeStarColorIntensity ( int newValue );
};

#endif
