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
#include "opscatalog.h"
#include "opssolarsystem.h"
#include "opsguides.h"
#include "opscolors.h"
#include "opsadvanced.h"

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
class QListView;
class QCheckListItem;
class QPixmap;
class QSpinBox;
class MagnitudeSpinBox;
class TimeStepBox;
class KIntSpinBox;
class KStars;

/**
	*Dialog containing controls for all user-configuration options in KStars.
	*@short User configuration dialog.
  *@author Jason Harris
	*@version 0.9
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
	*Destructor. Delete color objects.
	*/
	~ViewOpsDialog();

private:
	QPtrList<bool> showCatalog;

	QPixmap *SkyColor, *MessColor, *NGCColor, *ICColor, *LinksColor;
	QPixmap *EqColor, *EclColor, *HorzColor, *GridColor, *MWColor;
	QPixmap *PNameColor, *SNameColor, *CNameColor, *CLineColor;
	QStringList PresetFileList;
	QCheckListItem *showMessier, *showMessImages, *showNGC, *showIC;

	OpsCatalog *cat;
	OpsSolarSystem *ss;
	OpsGuides *guide;
	OpsColors *color;
	OpsAdvanced *adv;

	KStars *ksw;

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
	*Remove the currently-selected custom preset.
	*/
	void slotRemovePreset( void );

/**
	*Select the color scheme stored in filename.
	*/
	bool setColors( QString filename );

/**
	*Sync the KStars display with a newly-changed option.
	*/
	void updateDisplay( void );

/**
	*Adjust the ksw->options()->slewTimeScale variable.
	*/
	void changeSlewTimeScale( float );

/**
  * We have a new minimum star magnitude (brightness) for the display
  */
	void changeMagDrawStars( double newValue );

/**
  * We have a new minimum star magnitude (brightness) for the display,
	* to be used when fully zoomed out
  */
	void changeMagDrawStarZoomOut( double newValue );

/**
  * We have a new minimum star magnitude to be used while moving the display
  */
	void changeMagHideStars( double newValue );

/**
  * We have a new minimum star magnitude (brightness) for the displaying star information
  */
	void changeMagDrawInfo( double );

/**
	* Set the intensity of starcolors.
	*/
	void changeStarColorIntensity ( int newValue );

/**
	* Set the star color mode.
	*/
	void changeStarColorMode( int newMode );

/**
	* Set magnitude limit for drawing asteroids.
	*/
	void changeAstDrawMagLimit( double );

/**
	* Set magnitude limit for labeling asteroids.
	*/
	void changeAstNameMagLimit( double );

/**
	* Set maximum solar radius for labeling comets.
	*/
	void changeComNameMaxRad( double );

/**
	* Mark all planets for display.
	*/
	void markPlanets( void );

/**
	* Unmark all planets, so they won't be displayed.
	*/
	void unMarkPlanets( void );

/**
	* Add a custom catalog to the list of deep-sky catalogs
	*/
	void slotAddCatalog( void );

/**
	* Remove a custom catalog from the list of deep-sky catalogs
	*/
	void slotRemoveCatalog( void );

	void selectCatalog( void );

	/**Toggle whether trails are added to centered planet.
		*The actual option toggle is handled bu updateDisplay().
		*Here, we just check to see if there is currently a temporaryTrail (or if one is needed)
		*/
	void changeAutoTrail( void );

	/**Remove all planet trails
		*/
	void clearPlanetTrails( void );
/**
	*emit signal clearCache.
	*@see signals: clearCache()
	*/
	void sendClearCache();

	signals:
/**
	*Send signal for clearing cache of find dialog in KStars class.
	*Normally send if constellation name option would changed.
	*/
		void clearCache();
};

#endif
