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

class KStars;

/**@class ViewOpsDialog
	*Dialog containing controls for all user-configuration options in KStars.
	*@short User configuration dialog.
	*@author Jason Harris
	*@version 1.0
	*/

class ViewOpsDialog : public KDialogBase  {
  Q_OBJECT
public:
/**Constructor.  Connect Signals to Slots.
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
/**Choose a new palette Color for the selected item with a QColorDialog.
	*/
	void newColor( QListBoxItem* );

/**Load one of the predefined color schemes.  Just calls setColors with the
	*filename selected from the PresetFileList.
	*@p i list-index of the color scheme
	*/
	void slotPreset( int i );

/**Save the current color scheme as a custom preset.
	*/
	void slotAddPreset( void );

/**Remove the currently-selected custom color-scheme preset.
	*/
	void slotRemovePreset( void );

/**Select the color scheme stored in filename.
	*/
	bool setColors( QString filename );

/**Sync the KStars display with the new options.
	*/
	void updateDisplay( void );

/**Adjust the Options::slewTimeScale variable.
	*/
	void changeSlewTimeScale( float );

/**Adjust the limiting star magnitude (brightness) for the display,
	*to be used when fully zoomed in
	*@p newValue the new limiting mag
	*/
	void changeMagDrawStars( double newValue );

/**Adjust the limiting star magnitude (brightness) for the display,
	*to be used when fully zoomed out
	*@p newValue the new limiting mag
	*/
	void changeMagDrawStarZoomOut( double newValue );

/**Adjust the limiting DSO magnitude (brightness) for the display,
	*to be used when fully zoomed in
	*@p newValue the new limiting mag
	*/
	void changeMagDrawDeepSky( double newValue );
	
/**Adjust the limiting DSO magnitude (brightness) for the display,
	*to be used when fully zoomed out
	*@p newValue the new limiting mag
	*/
	void changeMagDrawDeepSkyZoomOut( double newValue );

/**Adjust the limiting star magnitude (brightness) for the display,
	*to be used when the display is moving
	*@p newValue the new limiting mag
	*/
	void changeMagHideStars( double newValue );

/**Adjust the limiting star magnitude (brightness) for attaching
	*name and/or magnitude labels
	*@p newValue the new limiting mag
	*/
	void changeMagDrawInfo( double );

/**Adjust the star color intensity (saturation) value
	*@p newValue the new color intensity value
	*/
	void changeStarColorIntensity ( int newValue );

/**Adjust the star color mode:
	*(0=Real colors; 1=Solid Red; 2=Solid Black; 3=Solid White)
	*@p newValue the new color mode
	*/
	void changeStarColorMode( int newMode );

/**Adjust the limiting asteroid magnitude (brightness) for the display.
	*@p newValue the new limiting mag
	*/
	void changeAstDrawMagLimit( double );

/**Adjust the limiting asteroid magnitude (brightness) for attaching
	*name labels
	*@p newValue the new limiting mag
	*/
	void changeAstNameMagLimit( double );

/**Adjust the maximum solar radius for attaching name labels to comets.
	*@p newValue the new maximum solar radius
	*/
	void changeComNameMaxRad( double );

/**Mark all planets for display.
	*/
	void markPlanets( void );

/**Unmark all planets, so they won't be displayed.
	*/
	void unMarkPlanets( void );

/**Add a custom catalog to the list of deep-sky catalogs
	*/
	void slotAddCatalog( void );

/**Remove a custom catalog from the list of deep-sky catalogs
	*/
	void slotRemoveCatalog( void );

/**When a custom catalog is selected, enable the "Remove" button.
	*/
	void selectCatalog( void );

/**Toggle whether trails are automatically attached to a centered solar system body.
	*/
	void changeAutoTrail( void );

/**Remove all planet trails
	*/
	void clearPlanetTrails( void );

/**emit signal clearCache.
	*@see signals: clearCache()
	*/
	void sendClearCache();

	signals:
/**Send signal for clearing cache of find dialog in KStars class.
	*Normally send if constellation name option is changed.
	*/
		void clearCache();
};

#endif
