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
class QListBox;
class QListBoxItem;
class QPixmap;
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
	QHBoxLayout *hlay, *ColorButtonsLayout, *hlayColorTab;
	QHBoxLayout *hlayLeftBox, *hlayLeftBox2;
	QVBoxLayout *vlayStarsTab, *vlayCatTab, *vlayGuideTab, *vlayPlanetTab;
	QVBoxLayout *vlayLeftBox, *vlayRightBox, *DisplayBoxLayout;
	QGridLayout *glayPlanetTab;

	QGroupBox  *DisplayBox, *LeftBox, *RightBox;
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
	QCheckBox *showMilkyWayFilled;
	QCheckBox *showGrid;
	QCheckBox *showEquator;
	QCheckBox *showEcliptic;
	QCheckBox *showHorizon;
	QCheckBox *showGround;

	QCheckBox *showSun, *showMoon;
	QCheckBox *showMercury, *showVenus, *showMars, *showJupiter;
	QCheckBox *showSaturn, *showUranus, *showNeptune, *showPluto;

	QListBox *ColorPalette, *PresetBox;
	QPixmap *SkyColor, *MessColor, *NGCColor, *ICColor, *LinksColor;
	QPixmap *EqColor, *EclColor, *HorzColor, *GridColor, *MWColor;
	QPixmap *SNameColor, *CNameColor, *CLineColor;

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
	
	QPushButton *AddPreset;
	QPushButton *showAll, *showNone;

//	QPushButton *NightColors;
//	QPushButton *ChartColors;
//	QPushButton *ResetColors;

	QButtonGroup *CoordsGroup;
	QRadioButton *EquatRadio;
	QRadioButton *AltAzRadio;

	KIntSpinBox *IntensityBox;
	QComboBox *StarColorMode;

	QStringList PresetFileList;

private slots:
/**
	*Choose a new palette Color for the selected item with a QColorDialog.
	*/
	void newColor( QListBoxItem* );
/**
	*Load one of the predefined color schemes.  Just calls setColors with the
  *filename selected from the PresetFileList.
	*/
	void slotPreset( int i );
/**
	*Save the current color scheme as a custom preset.
	*/
	void slotAddPreset( void );
/**
	*Select the color scheme stored in filename.
	*/
	bool setColors( QString filename );
/**
	*Select the default preset color scheme.
	*/
	void defaultColors( void );
/**
	*Select the "Night Vision" preset color scheme/
	*/
	void redColors( void );
/**
	*Select the chart preset color scheme.
	*/
	void chartColors( void );
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
/**
	* Set the star color mode.
	*/
	void changeStarColorMode( int newMode );
/**
	* Mark all planets for display.
	*/
	void markPlanets( void );
/**
	* Unmark all planets, so they won't be displayed.
	*/
	void unMarkPlanets( void );
};

#endif
