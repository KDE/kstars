/***************************************************************************
                          scriptbuilder.cpp  -  description
                             -------------------
    begin                : Thu Apr 17 2003
    copyright            : (C) 2003 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

//needed in slotSave() for chmod() syscall
#include <sys/stat.h>

#include <kdebug.h>
#include <kpushbutton.h>
#include <kcolorbutton.h>
#include <klocale.h>
#include <kcombobox.h>
#include <kicontheme.h>
#include <kiconloader.h>
#include <kio/netaccess.h>
#include <klistbox.h>
#include <klistview.h>
#include <kprocess.h>
#include <ktextedit.h>
#include <ktempfile.h>
#include <kdatewidget.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kstdguiitem.h>
#include <kstandarddirs.h>
#include <kurl.h>
#include <kurlrequester.h>
#include <knuminput.h>

#include <tqcheckbox.h>
#include <tqspinbox.h>
#include <tqwidgetstack.h>
#include <tqwidget.h>
#include <tqptrlist.h>
#include <tqlayout.h>
#include <tqdatetimeedit.h>
#include <tqradiobutton.h>
#include <tqbuttongroup.h>

#include "scriptfunction.h"
#include "scriptbuilderui.h"
#include "scriptnamedialog.h"
#include "optionstreeview.h"

#include "arglooktoward.h"
#include "argsetradec.h"
#include "argsetaltaz.h"
#include "argsetlocaltime.h"
#include "argwaitfor.h"
#include "argwaitforkey.h"
#include "argsettrack.h"
#include "argchangeviewoption.h"
#include "argsetgeolocation.h"
#include "argtimescale.h"
#include "argzoom.h"
#include "argexportimage.h"
#include "argprintimage.h"
#include "argsetcolor.h"
#include "argloadcolorscheme.h"
#include "argstartindi.h"
#include "argshutdownindi.h"
#include "argswitchindi.h"
#include "argsetportindi.h"
#include "argsettargetcoordindi.h"
#include "argsettargetnameindi.h"
#include "argsetactionindi.h"
#include "argsetfocusspeedindi.h"
#include "argstartfocusindi.h"
#include "argsetfocustimeoutindi.h"
#include "argsetgeolocationindi.h"
#include "argstartexposureindi.h"
#include "argsetutcindi.h"
#include "argsetscopeactionindi.h"
#include "argsetframetypeindi.h"
#include "argsetccdtempindi.h"
#include "argsetfilternumindi.h"

#include "scriptbuilder.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "kstarsdatetime.h"
#include "dmsbox.h"
#include "finddialog.h"
#include "locationdialog.h"
#include "skyobjectname.h"
#include "timestepbox.h"
#include "libkdeedu/extdate/extdatewidget.h"

ScriptBuilder::ScriptBuilder( TQWidget *parent, const char *name )
 : KDialogBase( KDialogBase::Plain, i18n( "Script Builder" ), Close, Close, parent, name ), 
		UnsavedChanges(false), currentFileURL(), currentDir( TQDir::homeDirPath() ), 
		currentScriptName(), currentAuthor() {

	TQFrame *page = plainPage();

	ks = (KStars*)parent;
	TQVBoxLayout *vlay = new TQVBoxLayout( page, 0, 0 );
	sb = new ScriptBuilderUI( page );
	vlay->addWidget( sb );

	KStarsFunctionList.setAutoDelete( TRUE );
	INDIFunctionList.setAutoDelete( TRUE);
	ScriptList.setAutoDelete( TRUE );

	//Initialize function templates and descriptions
	KStarsFunctionList.append( new ScriptFunction( "lookTowards", i18n( "Point the display at the specified location. %1 can be the name of an object, a cardinal point on the compass, or 'zenith'." ),
			false, "TQString", "dir" ) );
	KStarsFunctionList.append( new ScriptFunction( "setRaDec", i18n( "Point the display at the specified RA/Dec coordinates.  %1 is expressed in Hours; %2 is expressed in Degrees." ),
			false, "double", "ra", "double", "dec" ) );
	KStarsFunctionList.append( new ScriptFunction( "setAltAz", i18n( "Point the display at the specified Alt/Az coordinates.  %1 and %2 are expressed in Degrees." ),
			false, "double", "alt", "double", "az" ) );
	KStarsFunctionList.append( new ScriptFunction( "zoomIn", i18n( "Increase the display Zoom Level." ), false ) );
	KStarsFunctionList.append( new ScriptFunction( "zoomOut", i18n( "Decrease the display Zoom Level." ), false ) );
	KStarsFunctionList.append( new ScriptFunction( "defaultZoom", i18n( "Set the display Zoom Level to its default value." ), false ) );
	KStarsFunctionList.append( new ScriptFunction( "zoom", i18n( "Set the display Zoom Level manually." ), false, "double", "z" ) );
	KStarsFunctionList.append( new ScriptFunction( "setLocalTime", i18n( "Set the system clock to the specified Local Time." ),
			false, "int", "year", "int", "month", "int", "day", "int", "hour", "int", "minute", "int", "second" ) );
	KStarsFunctionList.append( new ScriptFunction( "waitFor", i18n( "Pause script execution for %1 seconds." ), false, "double", "sec" ) );
	KStarsFunctionList.append( new ScriptFunction( "waitForKey", i18n( "Halt script execution until the key %1 is pressed.  Only single-key strokes are possible; use 'space' for the spacebar." ),
			false, "TQString", "key" ) );
	KStarsFunctionList.append( new ScriptFunction( "setTracking", i18n( "Set whether the display is tracking the current location." ), false, "bool", "track" ) );
	KStarsFunctionList.append( new ScriptFunction( "changeViewOption", i18n( "Change view option named %1 to value %2." ), false, "TQString", "opName", "TQString", "opValue" ) );
	KStarsFunctionList.append( new ScriptFunction( "setGeoLocation", i18n( "Set the geographic location to the city specified by %1, %2 and %3." ),
			false, "TQString", "cityName", "TQString", "provinceName", "TQString", "countryName" ) );
	KStarsFunctionList.append( new ScriptFunction( "setColor", i18n( "Set the color named %1 to the value %2." ), false, "TQString", "colorName", "TQString", "value" ) );
	KStarsFunctionList.append( new ScriptFunction( "loadColorScheme", i18n( "Load the color scheme named %1." ), false, "TQString", "name" ) );
	KStarsFunctionList.append( new ScriptFunction( "exportImage", i18n( "Export the sky image to the file %1, with width %2 and height %3." ), false, "TQString", "fileName", "int", "width", "int", "height" ) );
	KStarsFunctionList.append( new ScriptFunction( "printImage", i18n( "Print the sky image to a printer or file.  If %1 is true, it will show the print dialog.  If %2 is true, it will use the Star Chart color scheme for printing." ), false, "bool", "usePrintDialog", "bool", "useChartColors" ) );
	KStarsFunctionList.append( new ScriptFunction( "stop", i18n( "Halt the simulation clock." ), true ) );
	KStarsFunctionList.append( new ScriptFunction( "start", i18n( "Start the simulation clock." ), true ) );
	KStarsFunctionList.append( new ScriptFunction( "setClockScale", i18n( "Set the timescale of the simulation clock to %1.  1.0 means real-time; 2.0 means twice real-time; etc." ), true, "double", "scale" ) );
	
	// INDI fuctions
	ScriptFunction *startINDIFunc(NULL), *shutdownINDIFunc(NULL), *switchINDIFunc(NULL), *setINDIPortFunc(NULL), *setINDIScopeActionFunc(NULL), *setINDITargetCoordFunc(NULL), *setINDITargetNameFunc(NULL), *setINDIGeoLocationFunc(NULL), *setINDIUTCFunc(NULL), *setINDIActionFunc(NULL), *waitForINDIActionFunc(NULL), *setINDIFocusSpeedFunc(NULL), *startINDIFocusFunc(NULL), *setINDIFocusTimeoutFunc(NULL), *setINDICCDTempFunc(NULL), *setINDIFilterNumFunc(NULL), *setINDIFrameTypeFunc(NULL), *startINDIExposureFunc(NULL); 
	
	startINDIFunc = new ScriptFunction( "startINDI", i18n("Establish an INDI device either in local mode or server mode."), false, "TQString", "deviceName", "bool", "useLocal");
	INDIFunctionList.append ( startINDIFunc );
	
	shutdownINDIFunc = new ScriptFunction( "shutdownINDI", i18n("Shutdown an INDI device."), false, "TQString", "deviceName");
	INDIFunctionList.append ( shutdownINDIFunc);
	
	switchINDIFunc = new ScriptFunction( "switchINDI", i18n("Connect or Disconnect an INDI device."), false, "TQString", "deviceName", "bool", "turnOn");
	switchINDIFunc->setINDIProperty("CONNECTION");
	INDIFunctionList.append ( switchINDIFunc);
	
	setINDIPortFunc = new ScriptFunction( "setINDIPort", i18n("Set INDI's device connection port."), false, "TQString", "deviceName", "TQString", "port");
	setINDIPortFunc->setINDIProperty("DEVICE_PORT");
	INDIFunctionList.append ( setINDIPortFunc);
	
	setINDIScopeActionFunc = new ScriptFunction( "setINDIScopeAction", i18n("Set the telescope action. Available actions are SLEW, TRACK, SYNC, PARK, and ABORT."), false, "TQString", "deviceName", "TQString", "action");
	setINDIScopeActionFunc->setINDIProperty("CHECK");
	INDIFunctionList.append( setINDIScopeActionFunc);
	
	setINDITargetCoordFunc = new ScriptFunction ( "setINDITargetCoord", i18n( "Set the telescope target coordinates to the RA/Dec coordinates.  RA is expressed in Hours; DEC is expressed in Degrees." ), false, "TQString", "deviceName", "double", "RA", "double", "DEC" );
	setINDITargetCoordFunc->setINDIProperty("EQUATORIAL_EOD_COORD");
	INDIFunctionList.append ( setINDITargetCoordFunc );
	
	setINDITargetNameFunc = new ScriptFunction( "setINDITargetName", i18n("Set the telescope target coorinates to the RA/Dec coordinates of the selected object."), false, "TQString", "deviceName", "TQString", "objectName");
	setINDITargetNameFunc->setINDIProperty("EQUATORIAL_EOD_COORD");
	INDIFunctionList.append( setINDITargetNameFunc);
	
	setINDIGeoLocationFunc = new ScriptFunction ( "setINDIGeoLocation", i18n("Set the telescope longitude and latitude. The longitude is E of N."), false, "TQString", "deviceName", "double", "long", "double", "lat");
	setINDIGeoLocationFunc->setINDIProperty("GEOGRAPHIC_COORD");
	INDIFunctionList.append( setINDIGeoLocationFunc);
	
	setINDIUTCFunc = new ScriptFunction ( "setINDIUTC", i18n("Set the device UTC time in ISO 8601 format YYYY/MM/DDTHH:MM:SS."), false, "TQString", "deviceName", "TQString", "UTCDateTime");
	setINDIUTCFunc->setINDIProperty("TIME");
	INDIFunctionList.append( setINDIUTCFunc);
	
	setINDIActionFunc = new ScriptFunction( "setINDIAction", i18n("Activate an INDI action. The action is the name of any INDI switch property element supported by the device."), false, "TQString", "deviceName", "TQString", "actionName");
	INDIFunctionList.append( setINDIActionFunc);
	
	waitForINDIActionFunc = new ScriptFunction ("waitForINDIAction", i18n("Pause script execution until action returns with OK status. The action can be the name of any INDI property supported by the device."), false, "TQString", "deviceName", "TQString", "actionName");
	INDIFunctionList.append( waitForINDIActionFunc );
	
	setINDIFocusSpeedFunc = new ScriptFunction ("setINDIFocusSpeed", i18n("Set the telescope focuser speed. Set speed to 0 to halt the focuser. 1-3 correspond to slow, medium, and fast speeds respectively."), false, "TQString", "deviceName", "unsigned int", "speed");
	setINDIFocusSpeedFunc->setINDIProperty("FOCUS_SPEED");
	INDIFunctionList.append( setINDIFocusSpeedFunc );
	
	startINDIFocusFunc = new ScriptFunction ("startINDIFocus", i18n("Start moving the focuser in the direction Dir, and for the duration specified by setINDIFocusTimeout."), false, "TQString", "deviceName", "TQString", "Dir");
	startINDIFocusFunc->setINDIProperty("FOCUS_MOTION");
	INDIFunctionList.append( startINDIFocusFunc);
	
	setINDIFocusTimeoutFunc = new ScriptFunction ( "setINDIFocusTimeout", i18n("Set the telescope focuser timer in seconds. This is the duration of any focusing procedure performed by calling startINDIFocus."), false, "TQString", "deviceName", "int", "timeout");
	setINDIFocusTimeoutFunc->setINDIProperty("FOCUS_TIMER");
	INDIFunctionList.append( setINDIFocusTimeoutFunc);
	
	setINDICCDTempFunc = new ScriptFunction( "setINDICCDTemp", i18n("Set the target CCD chip temperature."), false, "TQString", "deviceName", "int", "temp");
	setINDICCDTempFunc->setINDIProperty("CCD_TEMPERATURE");
	INDIFunctionList.append( setINDICCDTempFunc);

        setINDIFilterNumFunc = new ScriptFunction( "setINDIFilterNum", i18n("Set the target filter position."), false, "TQString", "deviceName", "int", "filter_num");
	setINDIFilterNumFunc->setINDIProperty("FILTER_SLOT");
	INDIFunctionList.append ( setINDIFilterNumFunc);
	
	setINDIFrameTypeFunc = new ScriptFunction( "setINDIFrameType", i18n("Set the CCD camera frame type. Available options are FRAME_LIGHT, FRAME_BIAS, FRAME_DARK, and FRAME_FLAT."), false, "TQString", "deviceName", "TQString", "type");
	setINDIFrameTypeFunc->setINDIProperty("FRAME_TYPE");
	INDIFunctionList.append( setINDIFrameTypeFunc);
	
	startINDIExposureFunc = new ScriptFunction ( "startINDIExposure", i18n("Start Camera/CCD exposure. The duration is in seconds."), false, "TQString", "deviceName", "int", "timeout");
	startINDIExposureFunc->setINDIProperty("CCD_EXPOSE_DURATION");
	INDIFunctionList.append( startINDIExposureFunc);
	
	
	// Modified by JM
	// We're using KListView instead of listbox to arrange the functions in two
	// main categories: KStars and INDI. INDI is further subdivided.
	
	sb->FunctionListView->addColumn(i18n("Functions"));
	sb->FunctionListView->setSorting(-1);
	
	TQListViewItem *INDI_tree = new TQListViewItem( sb->FunctionListView, "INDI");
        TQListViewItem *INDI_filter = new TQListViewItem( INDI_tree, "Filter");
	TQListViewItem *INDI_focuser = new TQListViewItem( INDI_tree, "Focuser");
	TQListViewItem *INDI_ccd = new TQListViewItem( INDI_tree, "Camera/CCD");
	TQListViewItem *INDI_telescope = new TQListViewItem( INDI_tree, "Telescope");
        TQListViewItem *INDI_general = new TQListViewItem( INDI_tree, "General");
        
	TQListViewItem *kstars_tree = new TQListViewItem( sb->FunctionListView, "KStars");
        
	
	for ( ScriptFunction *sf = KStarsFunctionList.last(); sf; sf = KStarsFunctionList.prev() )
	    new TQListViewItem (kstars_tree, sf->prototype());
	
          // General
          new TQListViewItem(INDI_general, waitForINDIActionFunc->prototype());
          new TQListViewItem(INDI_general, setINDIActionFunc->prototype());
	  new TQListViewItem(INDI_general, setINDIPortFunc->prototype());
	  new TQListViewItem(INDI_general, switchINDIFunc->prototype());
	  new TQListViewItem(INDI_general, shutdownINDIFunc->prototype());
	  new TQListViewItem(INDI_general, startINDIFunc->prototype());

	  // Telescope
	  new TQListViewItem(INDI_telescope, setINDIUTCFunc->prototype());
	  new TQListViewItem(INDI_telescope, setINDIGeoLocationFunc->prototype());
	  new TQListViewItem(INDI_telescope, setINDITargetNameFunc->prototype());
	  new TQListViewItem(INDI_telescope, setINDITargetCoordFunc->prototype());
	  new TQListViewItem(INDI_telescope, setINDIScopeActionFunc->prototype());

	  // CCD
	  new TQListViewItem(INDI_ccd, startINDIExposureFunc->prototype());
	  new TQListViewItem(INDI_ccd, setINDIFrameTypeFunc->prototype());
	  new TQListViewItem(INDI_ccd, setINDICCDTempFunc->prototype());

	  // Focuser
	  new TQListViewItem(INDI_focuser, startINDIFocusFunc->prototype());
	  new TQListViewItem(INDI_focuser, setINDIFocusTimeoutFunc->prototype());
          new TQListViewItem(INDI_focuser, setINDIFocusSpeedFunc->prototype());

	  // Filter
          new TQListViewItem(INDI_filter, setINDIFilterNumFunc->prototype());

	//Add icons to Push Buttons
	KIconLoader *icons = KGlobal::iconLoader();
	sb->NewButton->setIconSet( icons->loadIcon( "filenew", KIcon::Toolbar ) );
	sb->OpenButton->setIconSet( icons->loadIcon( "fileopen", KIcon::Toolbar ) );
	sb->SaveButton->setIconSet( icons->loadIconSet( "filesave", KIcon::Toolbar ) );
	sb->SaveAsButton->setIconSet( icons->loadIconSet( "filesaveas", KIcon::Toolbar ) );
	sb->RunButton->setIconSet( icons->loadIconSet( "launch", KIcon::Toolbar ) );
	sb->CopyButton->setIconSet( icons->loadIconSet( "reload", KIcon::Toolbar ) );
	sb->AddButton->setIconSet( icons->loadIconSet( "back", KIcon::Toolbar ) );
	sb->RemoveButton->setIconSet( icons->loadIconSet( "forward", KIcon::Toolbar ) );
	sb->UpButton->setIconSet( icons->loadIconSet( "up", KIcon::Toolbar ) );
	sb->DownButton->setIconSet( icons->loadIconSet( "down", KIcon::Toolbar ) );

	//Prepare the widget stack
	argBlank = new TQWidget( sb->ArgStack );
	argLookToward = new ArgLookToward( sb->ArgStack );
	argSetRaDec = new ArgSetRaDec( sb->ArgStack );
	argSetAltAz = new ArgSetAltAz( sb->ArgStack );
	argSetLocalTime = new ArgSetLocalTime( sb->ArgStack );
	argWaitFor = new ArgWaitFor( sb->ArgStack );
	argWaitForKey = new ArgWaitForKey( sb->ArgStack );
	argSetTracking = new ArgSetTrack( sb->ArgStack );
	argChangeViewOption = new ArgChangeViewOption( sb->ArgStack );
	argSetGeoLocation = new ArgSetGeoLocation( sb->ArgStack );
	argTimeScale = new ArgTimeScale( sb->ArgStack );
	argZoom = new ArgZoom( sb->ArgStack );
	argExportImage = new ArgExportImage( sb->ArgStack );
	argPrintImage = new ArgPrintImage( sb->ArgStack );
	argSetColor = new ArgSetColor( sb->ArgStack );
	argLoadColorScheme = new ArgLoadColorScheme( sb->ArgStack );
	argStartINDI = new ArgStartINDI (sb->ArgStack);
	argShutdownINDI = new ArgShutdownINDI (sb->ArgStack);
	argSwitchINDI   = new ArgSwitchINDI (sb->ArgStack);
	argSetPortINDI  = new ArgSetPortINDI (sb->ArgStack);
	argSetTargetCoordINDI = new ArgSetTargetCoordINDI (sb->ArgStack);
	argSetTargetNameINDI  = new ArgSetTargetNameINDI (sb->ArgStack);
	argSetActionINDI      = new ArgSetActionINDI (sb->ArgStack);
	argWaitForActionINDI  = new ArgSetActionINDI (sb->ArgStack);
	argSetFocusSpeedINDI  = new ArgSetFocusSpeedINDI (sb->ArgStack);
	argStartFocusINDI     = new ArgStartFocusINDI(sb->ArgStack);
	argSetFocusTimeoutINDI = new ArgSetFocusTimeoutINDI( sb->ArgStack);
	argSetGeoLocationINDI  = new ArgSetGeoLocationINDI( sb->ArgStack);
	argStartExposureINDI   = new ArgStartExposureINDI( sb->ArgStack);
	argSetUTCINDI          = new ArgSetUTCINDI( sb->ArgStack);
	argSetScopeActionINDI  = new ArgSetScopeActionINDI( sb->ArgStack);
	argSetFrameTypeINDI    = new ArgSetFrameTypeINDI (sb->ArgStack);
	argSetCCDTempINDI      = new ArgSetCCDTempINDI(sb->ArgStack);
	argSetFilterNumINDI    = new ArgSetFilterNumINDI(sb->ArgStack);
	
	argStartFocusINDI->directionCombo->insertItem("IN");
	argStartFocusINDI->directionCombo->insertItem("OUT");
	
	argSetScopeActionINDI->actionCombo->insertItem("SLEW");
	argSetScopeActionINDI->actionCombo->insertItem("TRACK");
	argSetScopeActionINDI->actionCombo->insertItem("SYNC");
	argSetScopeActionINDI->actionCombo->insertItem("PARK");
	argSetScopeActionINDI->actionCombo->insertItem("ABORT");
	
	argSetFrameTypeINDI->typeCombo->insertItem("FRAME_LIGHT");
	argSetFrameTypeINDI->typeCombo->insertItem("FRAME_BIAS");
	argSetFrameTypeINDI->typeCombo->insertItem("FRAME_DARK");
	argSetFrameTypeINDI->typeCombo->insertItem("FRAME_FLAT");
	
	sb->ArgStack->addWidget( argBlank );
	sb->ArgStack->addWidget( argLookToward );
	sb->ArgStack->addWidget( argSetRaDec );
	sb->ArgStack->addWidget( argSetAltAz );
	sb->ArgStack->addWidget( argSetLocalTime );
	sb->ArgStack->addWidget( argWaitFor );
	sb->ArgStack->addWidget( argWaitForKey );
	sb->ArgStack->addWidget( argSetTracking );
	sb->ArgStack->addWidget( argChangeViewOption );
	sb->ArgStack->addWidget( argSetGeoLocation );
	sb->ArgStack->addWidget( argTimeScale );
	sb->ArgStack->addWidget( argZoom );
	sb->ArgStack->addWidget( argExportImage );
	sb->ArgStack->addWidget( argPrintImage );
	sb->ArgStack->addWidget( argSetColor );
	sb->ArgStack->addWidget( argLoadColorScheme );
	
	sb->ArgStack->addWidget( argStartINDI);
	sb->ArgStack->addWidget( argShutdownINDI);
	sb->ArgStack->addWidget( argSwitchINDI);
	sb->ArgStack->addWidget( argSetPortINDI);
	sb->ArgStack->addWidget( argSetTargetCoordINDI);
	sb->ArgStack->addWidget( argSetTargetNameINDI);
	sb->ArgStack->addWidget( argSetActionINDI);
	sb->ArgStack->addWidget( argWaitForActionINDI );
	sb->ArgStack->addWidget( argSetFocusSpeedINDI );
	sb->ArgStack->addWidget( argStartFocusINDI);
	sb->ArgStack->addWidget( argSetFocusTimeoutINDI);
	sb->ArgStack->addWidget( argSetGeoLocationINDI);
	sb->ArgStack->addWidget( argStartExposureINDI);
	sb->ArgStack->addWidget( argSetUTCINDI);
	sb->ArgStack->addWidget( argSetScopeActionINDI);
	sb->ArgStack->addWidget( argSetFrameTypeINDI);
	sb->ArgStack->addWidget( argSetCCDTempINDI);
	sb->ArgStack->addWidget( argSetFilterNumINDI);
	
	sb->ArgStack->raiseWidget( 0 );

	snd = new ScriptNameDialog( ks );
	otv = new OptionsTreeView( ks );

	initViewOptions();

	//connect widgets in ScriptBuilderUI
	connect( sb->FunctionListView, TQT_SIGNAL( doubleClicked(TQListViewItem *, const TQPoint &, int )), this, TQT_SLOT( slotAddFunction() ) );
	connect( sb->FunctionListView, TQT_SIGNAL( currentChanged(TQListViewItem*) ), this, TQT_SLOT( slotShowDoc() ) );
	connect( sb->UpButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotMoveFunctionUp() ) );
	connect( sb->ScriptListBox, TQT_SIGNAL( currentChanged(TQListBoxItem*) ), this, TQT_SLOT( slotArgWidget() ) );
	connect( sb->DownButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotMoveFunctionDown() ) );
	connect( sb->CopyButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotCopyFunction() ) );
	connect( sb->RemoveButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotRemoveFunction() ) );
	connect( sb->NewButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotNew() ) );
	connect( sb->OpenButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotOpen() ) );
	connect( sb->SaveButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotSave() ) );
	connect( sb->SaveAsButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotSaveAs() ) );
	connect( sb->AddButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotAddFunction() ) );
	connect( sb->RunButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotRunScript() ) );

	//Connections for Arg Widgets
	connect( argSetGeoLocation->FindCityButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotFindCity() ) );
	connect( argLookToward->FindButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotFindObject() ) );
	connect( argChangeViewOption->TreeButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotShowOptions() ) );

	connect( argLookToward->FocusEdit, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotLookToward() ) );
	connect( argSetRaDec->RaBox, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotRa() ) );
	connect( argSetRaDec->DecBox, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotDec() ) );
	connect( argSetAltAz->AltBox, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotAlt() ) );
	connect( argSetAltAz->AzBox, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotAz() ) );
	connect( argSetLocalTime->DateBox, TQT_SIGNAL( changed(ExtDate) ), this, TQT_SLOT( slotChangeDate() ) );
	connect( argSetLocalTime->TimeBox, TQT_SIGNAL( valueChanged(const TQTime&) ), this, TQT_SLOT( slotChangeTime() ) );
	connect( argWaitFor->DelayBox, TQT_SIGNAL( valueChanged(int) ), this, TQT_SLOT( slotWaitFor() ) );
	connect( argWaitForKey->WaitKeyEdit, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotWaitForKey() ) );
	connect( argSetTracking->CheckTrack, TQT_SIGNAL( stateChanged(int) ), this, TQT_SLOT( slotTracking() ) );
	connect( argChangeViewOption->OptionName, TQT_SIGNAL( activated(const TQString &) ), this, TQT_SLOT( slotViewOption() ) );
	connect( argChangeViewOption->OptionValue, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotViewOption() ) );
	connect( argSetGeoLocation->CityName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotChangeCity() ) );
	connect( argSetGeoLocation->ProvinceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotChangeProvince() ) );
	connect( argSetGeoLocation->CountryName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotChangeCountry() ) );
	connect( argTimeScale->TimeScale, TQT_SIGNAL( scaleChanged(float) ), this, TQT_SLOT( slotTimeScale() ) );
	connect( argZoom->ZoomBox, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotZoom() ) );
	connect( argExportImage->ExportFileName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotExportImage() ) );
	connect( argExportImage->ExportWidth, TQT_SIGNAL( valueChanged(int) ), this, TQT_SLOT( slotExportImage() ) );
	connect( argExportImage->ExportHeight, TQT_SIGNAL( valueChanged(int) ), this, TQT_SLOT( slotExportImage() ) );
	connect( argPrintImage->UsePrintDialog, TQT_SIGNAL( toggled(bool) ), this, TQT_SLOT( slotPrintImage() ) );
	connect( argPrintImage->UseChartColors, TQT_SIGNAL( toggled(bool) ), this, TQT_SLOT( slotPrintImage() ) );
	connect( argSetColor->ColorName, TQT_SIGNAL( activated(const TQString &) ), this, TQT_SLOT( slotChangeColorName() ) );
	connect( argSetColor->ColorValue, TQT_SIGNAL( changed(const TQColor &) ), this, TQT_SLOT( slotChangeColor() ) );
	connect( argLoadColorScheme->SchemeList, TQT_SIGNAL( clicked( TQListBoxItem* ) ), this, TQT_SLOT( slotLoadColorScheme( TQListBoxItem* ) ) );
	connect( snd->ScriptName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotEnableScriptNameOK() ) );
	
	connect( sb->AppendINDIWait, TQT_SIGNAL ( toggled(bool) ), this, TQT_SLOT(slotINDIWaitCheck(bool)));
	
	// Connections for INDI's Arg widgets
	
	// INDI Start Device
	connect (argStartINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDIStartDeviceName()));
	connect (argStartINDI->INDIMode, TQT_SIGNAL ( clicked( int)), this, TQT_SLOT (slotINDIStartDeviceMode())); 
	
	// INDI Shutdown Device
	connect (argShutdownINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDIShutdown()));
	
	// INDI Swtich Device
	connect (argSwitchINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISwitchDeviceName()));
	connect (argSwitchINDI->INDIConnection, TQT_SIGNAL ( clicked( int)), this, TQT_SLOT (slotINDISwitchDeviceConnection())); 
	
	// INDI Set Device Port
	connect (argSetPortINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISetPortDeviceName()));
	connect (argSetPortINDI->devicePort, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISetPortDevicePort()));
	
	// INDI Set Target Coord 
	connect (argSetTargetCoordINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISetTargetCoordDeviceName()));
	connect( argSetTargetCoordINDI->RaBox, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotINDISetTargetCoordDeviceRA() ) );
	connect( argSetTargetCoordINDI->DecBox, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotINDISetTargetCoordDeviceDEC() ) );
	
	// INDI Set Target Name
	connect( argSetTargetNameINDI->FindButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotINDIFindObject() ) );
	connect (argSetTargetNameINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISetTargetNameDeviceName()));
	connect (argSetTargetNameINDI->objectName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISetTargetNameObjectName()));
	
	// INDI Set Action
	connect (argSetActionINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISetActionDeviceName()));
	connect (argSetActionINDI->actionName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISetActionName()));
	
	// INDI Wait For Action
	connect (argWaitForActionINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDIWaitForActionDeviceName()));
	connect (argWaitForActionINDI->actionName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDIWaitForActionName()));
	
	// INDI Set Focus Speed
	connect (argSetFocusSpeedINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISetFocusSpeedDeviceName()));
	connect (argSetFocusSpeedINDI->speedIN, TQT_SIGNAL( valueChanged(int) ), this, TQT_SLOT(slotINDISetFocusSpeed()));
	
	// INDI Start Focus
	connect (argStartFocusINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDIStartFocusDeviceName()));
	connect (argStartFocusINDI->directionCombo, TQT_SIGNAL( activated(const TQString &) ), this, TQT_SLOT(slotINDIStartFocusDirection()));
	
	// INDI Set Focus Timeout
	connect (argSetFocusTimeoutINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISetFocusTimeoutDeviceName()));
	connect (argSetFocusTimeoutINDI->timeOut, TQT_SIGNAL( valueChanged(int) ), this, TQT_SLOT(slotINDISetFocusTimeout()));
	
	// INDI Set Geo Location
	connect (argSetGeoLocationINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISetGeoLocationDeviceName()));
	connect( argSetGeoLocationINDI->longBox, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotINDISetGeoLocationDeviceLong() ) );
	connect( argSetGeoLocationINDI->latBox, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT( slotINDISetGeoLocationDeviceLat() ) );
	
	// INDI Start Exposure
	connect (argStartExposureINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDIStartExposureDeviceName()));
	connect (argStartExposureINDI->timeOut, TQT_SIGNAL( valueChanged(int) ), this, TQT_SLOT(slotINDIStartExposureTimeout()));
	
	// INDI Set UTC
	connect (argSetUTCINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISetUTCDeviceName()));
	connect (argSetUTCINDI->UTC, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISetUTC()));
	
	// INDI Set Scope Action
	connect (argSetScopeActionINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISetScopeActionDeviceName()));
	connect (argSetScopeActionINDI->actionCombo, TQT_SIGNAL( activated(const TQString &) ), this, TQT_SLOT(slotINDISetScopeAction()));
	
	// INDI Set Frame type
	connect (argSetFrameTypeINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISetFrameTypeDeviceName()));
	connect (argSetFrameTypeINDI->typeCombo, TQT_SIGNAL( activated(const TQString &) ), this, TQT_SLOT(slotINDISetFrameType()));
	
	// INDI Set CCD Temp
	connect (argSetCCDTempINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISetCCDTempDeviceName()));
	connect (argSetCCDTempINDI->temp, TQT_SIGNAL( valueChanged(int) ), this, TQT_SLOT(slotINDISetCCDTemp()));

	// INDI Set Filter Num
	connect (argSetFilterNumINDI->deviceName, TQT_SIGNAL( textChanged(const TQString &) ), this, TQT_SLOT(slotINDISetFilterNumDeviceName()));
	connect (argSetFilterNumINDI->filter_num, TQT_SIGNAL( valueChanged(int) ), this, TQT_SLOT(slotINDISetFilterNum()));

	
	//disbale some buttons
	sb->CopyButton->setEnabled( false );
	sb->AddButton->setEnabled( false );
	sb->RemoveButton->setEnabled( false );
	sb->UpButton->setEnabled( false );
	sb->DownButton->setEnabled( false );
	sb->SaveButton->setEnabled( false );
	sb->SaveAsButton->setEnabled( false );
	sb->RunButton->setEnabled( false );
}

ScriptBuilder::~ScriptBuilder()
{
}

void ScriptBuilder::initViewOptions() {
	otv->OptionsList->setRootIsDecorated( true );

	//InfoBoxes
	opsGUI = new TQListViewItem( otv->OptionsList, i18n( "InfoBoxes" ) );
	new TQListViewItem( opsGUI, "ShowInfoBoxes", i18n( "Toggle display of all InfoBoxes" ), i18n( "bool" ) );
	new TQListViewItem( opsGUI, "ShowTimeBox", i18n( "Toggle display of Time InfoBox" ), i18n( "bool" ) );
	new TQListViewItem( opsGUI, "ShowGeoBox", i18n( "Toggle display of Geographic InfoBox" ), i18n( "bool" ) );
	new TQListViewItem( opsGUI, "ShowFocusBox", i18n( "Toggle display of Focus InfoBox" ), i18n( "bool" ) );
	new TQListViewItem( opsGUI, "ShadeTimeBox", i18n( "(un)Shade Time InfoBox" ), i18n( "bool" ) );
	new TQListViewItem( opsGUI, "ShadeGeoBox", i18n( "(un)Shade Geographic InfoBox" ), i18n( "bool" ) );
	new TQListViewItem( opsGUI, "ShadeFocusBox", i18n( "(un)Shade Focus InfoBox" ), i18n( "bool" ) );
	argChangeViewOption->OptionName->insertItem( "ShowInfoBoxes" );
	argChangeViewOption->OptionName->insertItem( "ShowTimeBox" );
	argChangeViewOption->OptionName->insertItem( "ShowGeoBox" );
	argChangeViewOption->OptionName->insertItem( "ShowFocusBox" );
	argChangeViewOption->OptionName->insertItem( "ShadeTimeBox" );
	argChangeViewOption->OptionName->insertItem( "ShadeGeoBox" );
	argChangeViewOption->OptionName->insertItem( "ShadeFocusBox" );

	//Toolbars
	opsToolbar = new TQListViewItem( otv->OptionsList, i18n( "Toolbars" ) );
	new TQListViewItem( opsToolbar, "ShowMainToolBar", i18n( "Toggle display of main toolbar" ), i18n( "bool" ) );
	new TQListViewItem( opsToolbar, "ShowViewToolBar", i18n( "Toggle display of view toolbar" ), i18n( "bool" ) );
	argChangeViewOption->OptionName->insertItem( "ShowMainToolBar" );
	argChangeViewOption->OptionName->insertItem( "ShowViewToolBar" );

	//Show Objects
	opsShowObj = new TQListViewItem( otv->OptionsList, i18n( "Show Objects" ) );
	new TQListViewItem( opsShowObj, "ShowStars", i18n( "Toggle display of Stars" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowDeepSky", i18n( "Toggle display of all deep-sky objects" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowMessier", i18n( "Toggle display of Messier object symbols" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowMessierImages", i18n( "Toggle display of Messier object images" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowNGC", i18n( "Toggle display of NGC objects" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowIC", i18n( "Toggle display of IC objects" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowPlanets", i18n( "Toggle display of all solar system bodies" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowSun", i18n( "Toggle display of Sun" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowMoon", i18n( "Toggle display of Moon" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowMercury", i18n( "Toggle display of Mercury" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowVenus", i18n( "Toggle display of Venus" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowMars", i18n( "Toggle display of Mars" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowJupiter", i18n( "Toggle display of Jupiter" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowSaturn", i18n( "Toggle display of Saturn" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowUranus", i18n( "Toggle display of Uranus" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowNeptune", i18n( "Toggle display of Neptune" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowPluto", i18n( "Toggle display of Pluto" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowAsteroids", i18n( "Toggle display of Asteroids" ), i18n( "bool" ) );
	new TQListViewItem( opsShowObj, "ShowComets", i18n( "Toggle display of Comets" ), i18n( "bool" ) );
	argChangeViewOption->OptionName->insertItem( "ShowSAO" );
	argChangeViewOption->OptionName->insertItem( "ShowDeepSky" );
	argChangeViewOption->OptionName->insertItem( "ShowMess" );
	argChangeViewOption->OptionName->insertItem( "ShowMessImages" );
	argChangeViewOption->OptionName->insertItem( "ShowNGC" );
	argChangeViewOption->OptionName->insertItem( "ShowIC" );
	argChangeViewOption->OptionName->insertItem( "ShowPlanets" );
	argChangeViewOption->OptionName->insertItem( "ShowSun" );
	argChangeViewOption->OptionName->insertItem( "ShowMoon" );
	argChangeViewOption->OptionName->insertItem( "ShowMercury" );
	argChangeViewOption->OptionName->insertItem( "ShowVenus" );
	argChangeViewOption->OptionName->insertItem( "ShowMars" );
	argChangeViewOption->OptionName->insertItem( "ShowJupiter" );
	argChangeViewOption->OptionName->insertItem( "ShowSaturn" );
	argChangeViewOption->OptionName->insertItem( "ShowUranus" );
	argChangeViewOption->OptionName->insertItem( "ShowNeptune" );
	argChangeViewOption->OptionName->insertItem( "ShowPluto" );
	argChangeViewOption->OptionName->insertItem( "ShowAsteroids" );
	argChangeViewOption->OptionName->insertItem( "ShowComets" );

	opsShowOther = new TQListViewItem( otv->OptionsList, i18n( "Show Other" ) );
	new TQListViewItem( opsShowOther, "ShowCLines", i18n( "Toggle display of constellation lines" ), i18n( "bool" ) );
	new TQListViewItem( opsShowOther, "ShowCBounds", i18n( "Toggle display of constellation boundaries" ), i18n( "bool" ) );
	new TQListViewItem( opsShowOther, "ShowCNames", i18n( "Toggle display of constellation names" ), i18n( "bool" ) );
	new TQListViewItem( opsShowOther, "ShowMilkyWay", i18n( "Toggle display of Milky Way" ), i18n( "bool" ) );
	new TQListViewItem( opsShowOther, "ShowGrid", i18n( "Toggle display of the coordinate grid" ), i18n( "bool" ) );
	new TQListViewItem( opsShowOther, "ShowEquator", i18n( "Toggle display of the celestial equator" ), i18n( "bool" ) );
	new TQListViewItem( opsShowOther, "ShowEcliptic", i18n( "Toggle display of the ecliptic" ), i18n( "bool" ) );
	new TQListViewItem( opsShowOther, "ShowHorizon", i18n( "Toggle display of the horizon line" ), i18n( "bool" ) );
	new TQListViewItem( opsShowOther, "ShowGround", i18n( "Toggle display of the opaque ground" ), i18n( "bool" ) );
	new TQListViewItem( opsShowOther, "ShowStarNames", i18n( "Toggle display of star name labels" ), i18n( "bool" ) );
	new TQListViewItem( opsShowOther, "ShowStarMagnitudes", i18n( "Toggle display of star magnitude labels" ), i18n( "bool" ) );
	new TQListViewItem( opsShowOther, "ShowAsteroidNames", i18n( "Toggle display of asteroid name labels" ), i18n( "bool" ) );
	new TQListViewItem( opsShowOther, "ShowCometNames", i18n( "Toggle display of comet name labels" ), i18n( "bool" ) );
	new TQListViewItem( opsShowOther, "ShowPlanetNames", i18n( "Toggle display of planet name labels" ), i18n( "bool" ) );
	new TQListViewItem( opsShowOther, "ShowPlanetImages", i18n( "Toggle display of planet images" ), i18n( "bool" ) );
	argChangeViewOption->OptionName->insertItem( "ShowCLines" );
	argChangeViewOption->OptionName->insertItem( "ShowCBounds" );
	argChangeViewOption->OptionName->insertItem( "ShowCNames" );
	argChangeViewOption->OptionName->insertItem( "ShowMilkyWay" );
	argChangeViewOption->OptionName->insertItem( "ShowGrid" );
	argChangeViewOption->OptionName->insertItem( "ShowEquator" );
	argChangeViewOption->OptionName->insertItem( "ShowEcliptic" );
	argChangeViewOption->OptionName->insertItem( "ShowHorizon" );
	argChangeViewOption->OptionName->insertItem( "ShowGround" );
	argChangeViewOption->OptionName->insertItem( "ShowStarNames" );
	argChangeViewOption->OptionName->insertItem( "ShowStarMagnitudes" );
	argChangeViewOption->OptionName->insertItem( "ShowAsteroidNames" );
	argChangeViewOption->OptionName->insertItem( "ShowCometNames" );
	argChangeViewOption->OptionName->insertItem( "ShowPlanetNames" );
	argChangeViewOption->OptionName->insertItem( "ShowPlanetImages" );

	opsCName = new TQListViewItem( otv->OptionsList, i18n( "Constellation Names" ) );
	new TQListViewItem( opsCName, "UseLatinConstellNames", i18n( "Show Latin constellation names" ), i18n( "bool" ) );
	new TQListViewItem( opsCName, "UseLocalConstellNames", i18n( "Show constellation names in local language" ), i18n( "bool" ) );
	new TQListViewItem( opsCName, "UseAbbrevConstellNames", i18n( "Show IAU-standard constellation abbreviations" ), i18n( "bool" ) );
	argChangeViewOption->OptionName->insertItem( "UseLatinConstellNames" );
	argChangeViewOption->OptionName->insertItem( "UseLocalConstellNames" );
	argChangeViewOption->OptionName->insertItem( "UseAbbrevConstellNames" );

	opsHide = new TQListViewItem( otv->OptionsList, i18n( "Hide Items" ) );
	new TQListViewItem( opsHide, "HideOnSlew", i18n( "Toggle whether objects hidden while slewing display" ), i18n( "bool" ) );
	new TQListViewItem( opsHide, "SlewTimeScale", i18n( "Timestep threshold (in seconds) for hiding objects" ), i18n( "double" ) );
	new TQListViewItem( opsHide, "HideStars", i18n( "Hide faint stars while slewing?" ), i18n( "bool" ) );
	new TQListViewItem( opsHide, "HidePlanets", i18n( "Hide solar system bodies while slewing?" ), i18n( "bool" ) );
	new TQListViewItem( opsHide, "HideMessier", i18n( "Hide Messier objects while slewing?" ), i18n( "bool" ) );
	new TQListViewItem( opsHide, "HideNGC", i18n( "Hide NGC objects while slewing?" ), i18n( "bool" ) );
	new TQListViewItem( opsHide, "HideIC", i18n( "Hide IC objects while slewing?" ), i18n( "bool" ) );
	new TQListViewItem( opsHide, "HideMilkyWay", i18n( "Hide Milky Way while slewing?" ), i18n( "bool" ) );
	new TQListViewItem( opsHide, "HideCNames", i18n( "Hide constellation names while slewing?" ), i18n( "bool" ) );
	new TQListViewItem( opsHide, "HideCLines", i18n( "Hide constellation lines while slewing?" ), i18n( "bool" ) );
	new TQListViewItem( opsHide, "HideCBounds", i18n( "Hide constellation boundaries while slewing?" ), i18n( "bool" ) );
	new TQListViewItem( opsHide, "HideGrid", i18n( "Hide coordinate grid while slewing?" ), i18n( "bool" ) );
	argChangeViewOption->OptionName->insertItem( "HideOnSlew" );
	argChangeViewOption->OptionName->insertItem( "SlewTimeScale" );
	argChangeViewOption->OptionName->insertItem( "HideStars" );
	argChangeViewOption->OptionName->insertItem( "HidePlanets" );
	argChangeViewOption->OptionName->insertItem( "HideMessier" );
	argChangeViewOption->OptionName->insertItem( "HideNGC" );
	argChangeViewOption->OptionName->insertItem( "HideIC" );
	argChangeViewOption->OptionName->insertItem( "HideMilkyWay" );
	argChangeViewOption->OptionName->insertItem( "HideCNames" );
	argChangeViewOption->OptionName->insertItem( "HideCLines" );
	argChangeViewOption->OptionName->insertItem( "HideCBounds" );
	argChangeViewOption->OptionName->insertItem( "HideGrid" );

	opsSkymap = new TQListViewItem( otv->OptionsList, i18n( "Skymap Options" ) );
	new TQListViewItem( opsSkymap, "UseAltAz", i18n( "Use Horizontal coordinates? (otherwise, use Equatorial)" ), i18n( "bool" ) );
	new TQListViewItem( opsSkymap, "ZoomFactor", i18n( "Set the Zoom Factor" ), i18n( "double" ) );
	new TQListViewItem( opsSkymap, "FOV Size", i18n( "Select angular size for the FOV symbol (in arcmin)" ), i18n( "double" ) );
	new TQListViewItem( opsSkymap, "FOV Shape", i18n( "Select tqshape for the FOV symbol (0=Square, 1=Circle, 2=Crosshairs, 4=Bullseye)" ), i18n( "int" ) );
	new TQListViewItem( opsSkymap, "FOV Color", i18n( "Select color for the FOV symbol" ), i18n( "string" ) );
	new TQListViewItem( opsSkymap, "AnimateSlewing", i18n( "Use animated slewing? (otherwise, \"snap\" to new focus)" ), i18n( "bool" ) );
	new TQListViewItem( opsSkymap, "UseRefraction", i18n( "Correct for atmospheric refraction?" ), i18n( "bool" ) );
	new TQListViewItem( opsSkymap, "UseAutoLabel", i18n( "Automatically attach name label to centered object?" ), i18n( "bool" ) );
	new TQListViewItem( opsSkymap, "UseHoverLabel", i18n( "Attach temporary name label when hovering mouse over an object?" ), i18n( "bool" ) );
	new TQListViewItem( opsSkymap, "UseAutoTrail", i18n( "Automatically add trail to centered solar system body?" ), i18n( "bool" ) );
	new TQListViewItem( opsSkymap, "FadePlanetTrails", i18n( "Planet trails fade to sky color? (otherwise color is constant)" ), i18n( "bool" ) );
	argChangeViewOption->OptionName->insertItem( "UseAltAz" );
	argChangeViewOption->OptionName->insertItem( "ZoomFactor" );
	argChangeViewOption->OptionName->insertItem( "FOVName" );
	argChangeViewOption->OptionName->insertItem( "FOVSize" );
	argChangeViewOption->OptionName->insertItem( "FOVShape" );
	argChangeViewOption->OptionName->insertItem( "FOVColor" );
	argChangeViewOption->OptionName->insertItem( "UseRefraction" );
	argChangeViewOption->OptionName->insertItem( "UseAutoLabel" );
	argChangeViewOption->OptionName->insertItem( "UseHoverLabel" );
	argChangeViewOption->OptionName->insertItem( "UseAutoTrail" );
	argChangeViewOption->OptionName->insertItem( "AnimateSlewing" );
	argChangeViewOption->OptionName->insertItem( "FadePlanetTrails" );

	opsLimit = new TQListViewItem( otv->OptionsList, i18n( "Limits" ) );
	new TQListViewItem( opsLimit, "magLimitDrawStar", i18n( "magnitude of faintest star drawn on map when zoomed in" ), i18n( "double" ) );
	new TQListViewItem( opsLimit, "magLimitDrawStarZoomOut", i18n( "magnitude of faintest star drawn on map when zoomed out" ), i18n( "double" ) );
	new TQListViewItem( opsLimit, "magLimitDrawDeepSky", i18n( "magnitude of faintest nonstellar object drawn on map when zoomed in" ), i18n( "double" ) );
	new TQListViewItem( opsLimit, "magLimitDrawDeepSkyZoomOut", i18n( "magnitude of faintest nonstellar object drawn on map when zoomed out" ), i18n( "double" ) );
	new TQListViewItem( opsLimit, "magLimitDrawStarInfo", i18n( "magnitude of faintest star labeled on map" ), i18n( "double" ) );
	new TQListViewItem( opsLimit, "magLimitHideStar", i18n( "magnitude of brightest star hidden while slewing" ), i18n( "double" ) );
	new TQListViewItem( opsLimit, "magLimitAsteroid", i18n( "magnitude of faintest asteroid drawn on map" ), i18n( "double" ) );
	new TQListViewItem( opsLimit, "magLimitAsteroidName", i18n( "magnitude of faintest asteroid labeled on map" ), i18n( "double" ) );
	new TQListViewItem( opsLimit, "maxRadCometName", i18n( "comets nearer to the Sun than this (in AU) are labeled on map" ), i18n( "double" ) );
	argChangeViewOption->OptionName->insertItem( "magLimitDrawStar" );
	argChangeViewOption->OptionName->insertItem( "magLimitDrawStarZoomOut" );
	argChangeViewOption->OptionName->insertItem( "magLimitDrawDeepSky" );
	argChangeViewOption->OptionName->insertItem( "magLimitDrawDeepSkyZoomOut" );
	argChangeViewOption->OptionName->insertItem( "magLimitDrawStarInfo" );
	argChangeViewOption->OptionName->insertItem( "magLimitHideStar" );
	argChangeViewOption->OptionName->insertItem( "magLimitAsteroid" );
	argChangeViewOption->OptionName->insertItem( "magLimitAsteroidName" );
	argChangeViewOption->OptionName->insertItem( "maxRadCometName" );

	//init the list of color names and values
	for ( unsigned int i=0; i < ks->data()->colorScheme()->numberOfColors(); ++i ) {
		argSetColor->ColorName->insertItem( ks->data()->colorScheme()->nameAt(i) );
	}
	
	//init list of color scheme names
	argLoadColorScheme->SchemeList->insertItem( i18n( "use default color scheme", "Default Colors" ) );
	argLoadColorScheme->SchemeList->insertItem( i18n( "use 'star chart' color scheme", "Star Chart" ) );
	argLoadColorScheme->SchemeList->insertItem( i18n( "use 'night vision' color scheme", "Night Vision" ) );
	argLoadColorScheme->SchemeList->insertItem( i18n( "use 'moonless night' color scheme", "Moonless Night" ) );
	
	TQFile file;
	TQString line;
	file.setName( locate( "appdata", "colors.dat" ) ); //determine filename in local user KDE directory tree.
	if ( file.open( IO_ReadOnly ) ) {
		TQTextStream stream( &file );

  	while ( !stream.eof() ) {
			line = stream.readLine();
			argLoadColorScheme->SchemeList->insertItem( line.left( line.find( ':' ) ) );
		}
		file.close();
	}
}

//Slots defined in ScriptBuilderUI
void ScriptBuilder::slotNew() {
	saveWarning();
	if ( !UnsavedChanges ) {
		ScriptList.clear();
		sb->ScriptListBox->clear();
		sb->ArgStack->raiseWidget( argBlank );

		sb->CopyButton->setEnabled( false );
		sb->RemoveButton->setEnabled( false );
		sb->RunButton->setEnabled( false );

		currentFileURL = "";
		currentScriptName = "";
	}
}

void ScriptBuilder::slotOpen() {
	saveWarning();

	TQString fname;
	KTempFile tmpfile;
	tmpfile.setAutoDelete(true);

	if ( !UnsavedChanges ) {
		currentFileURL = KFileDialog::getOpenURL( currentDir, "*.kstars|KStars Scripts (*.kstars)" );

		if ( currentFileURL.isValid() ) {
			currentDir = currentFileURL.directory();

			ScriptList.clear();
			sb->ScriptListBox->clear();
			sb->ArgStack->raiseWidget( argBlank );

			if ( currentFileURL.isLocalFile() ) {
				fname = currentFileURL.path();
			} else {
				fname = tmpfile.name();
				if ( ! KIO::NetAccess::download( currentFileURL, fname, (TQWidget*) 0 ) )
					KMessageBox::sorry( 0, i18n( "Could not download remote file." ), i18n( "Download Error" ) );
			}

			TQFile f( fname );
			if ( !f.open( IO_ReadOnly) ) {
				TQString message = i18n( "Could not open file %1." ).arg( f.name() );
				KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
				currentFileURL = "";
				return;
			}

			TQTextStream istream(&f);
			readScript( istream );

			f.close();
		} else if ( ! currentFileURL.url().isEmpty() ) {
			TQString message = i18n( "Invalid URL: %1" ).arg( currentFileURL.url() );
			KMessageBox::sorry( 0, message, i18n( "Invalid URL" ) );
			currentFileURL = "";
		}
	}
}

void ScriptBuilder::slotSave() {
	TQString fname;
	KTempFile tmpfile;
	tmpfile.setAutoDelete(true);

	if ( currentScriptName.isEmpty() ) {
		//Get Script Name and Author info
		if ( snd->exec() == TQDialog::Accepted ) {
			currentScriptName = snd->ScriptName->text();
			currentAuthor = snd->AuthorName->text();
		} else {
			return;
		}
	}

	if ( currentFileURL.isEmpty() )
		currentFileURL = KFileDialog::getSaveURL( currentDir, "*.kstars|KStars Scripts (*.kstars)" );

	if ( currentFileURL.isValid() ) {
		currentDir = currentFileURL.directory();

		if ( currentFileURL.isLocalFile() ) {
			fname = currentFileURL.path();
			
			//Warn user if file exists
			if (TQFile::exists(currentFileURL.path())) {
				int r=KMessageBox::warningContinueCancel(static_cast<TQWidget *>(parent()),
						i18n( "A file named \"%1\" already exists. "
								"Overwrite it?" ).arg(currentFileURL.fileName()),
						i18n( "Overwrite File?" ),
						i18n( "&Overwrite" ) );
		
				if(r==KMessageBox::Cancel) return;
			}
		} else {
			fname = tmpfile.name();
		}
		
		if ( fname.right( 7 ).lower() != ".kstars" ) fname += ".kstars";

		TQFile f( fname );
		if ( !f.open( IO_WriteOnly) ) {
			TQString message = i18n( "Could not open file %1." ).arg( f.name() );
			KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
			currentFileURL = "";
			return;
		}

		TQTextStream ostream(&f);
		writeScript( ostream );
		f.close();

		//set rwx for owner, rx for group, rx for other
		chmod( fname.ascii(), S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH );

		if ( tmpfile.name() == fname ) { //need to upload to remote location
			if ( ! KIO::NetAccess::upload( tmpfile.name(), currentFileURL, (TQWidget*) 0 ) ) {
				TQString message = i18n( "Could not upload image to remote location: %1" ).arg( currentFileURL.prettyURL() );
				KMessageBox::sorry( 0, message, i18n( "Could not upload file" ) );
			}
		}

		setUnsavedChanges( false );

	} else {
		TQString message = i18n( "Invalid URL: %1" ).arg( currentFileURL.url() );
		KMessageBox::sorry( 0, message, i18n( "Invalid URL" ) );
		currentFileURL = "";
	}
}

void ScriptBuilder::slotSaveAs() {
	currentFileURL = "";
	currentScriptName = "";
	slotSave();
}

void ScriptBuilder::saveWarning() {
	if ( UnsavedChanges ) {
		TQString caption = i18n( "Save Changes to Script?" );
		TQString message = i18n( "The current script has unsaved changes.  Would you like to save before closing it?" );
		int ans = KMessageBox::warningYesNoCancel( 0, message, caption, KStdGuiItem::save(), KStdGuiItem::discard() );
		if ( ans == KMessageBox::Yes ) {
			slotSave();
			setUnsavedChanges( false );
		} else if ( ans == KMessageBox::No ) {
			setUnsavedChanges( false );
		}

		//Do nothing if 'cancel' selected
	}
}

void ScriptBuilder::slotRunScript() {
	//hide window while script runs
// If this is uncommented, the program hangs before the script is executed.  Why?
//	hide();

	//Save current script to a temporary file, then execute that file.
	//For some reason, I can't use KTempFile here!  If I do, then the temporary script
	//is not executable.  Bizarre...
	//KTempFile tmpfile;
	//TQString fname = tmpfile.name();
	TQString fname = locateLocal( "tmp", "kstars-tempscript" );

	TQFile f( fname );
	if ( f.exists() ) f.remove();
	if ( !f.open( IO_WriteOnly) ) {
		TQString message = i18n( "Could not open file %1." ).arg( f.name() );
		KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
		currentFileURL = "";
		return;
	}

	TQTextStream ostream(&f);
	writeScript( ostream );
	f.close();

	//set rwx for owner, rx for group, rx for other
	chmod( f.name().ascii(), S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH );

	KProcess p;
	p << f.name();
	if ( ! p.start( KProcess::DontCare ) )
		kdDebug() << "Process did not start." << endl;

	while ( p.isRunning() ) kapp->processEvents( 50 ); //otherwise tempfile may get deleted before script completes.

	//delete temp file
	if ( f.exists() ) f.remove();

	//uncomment if 'hide()' is uncommented...
//	show();
}

void ScriptBuilder::writeScript( TQTextStream &ostream ) {
	TQString mainpre  = "dcop $KSTARS $MAIN  ";
	TQString clockpre = "dcop $KSTARS $CLOCK ";

	//Write script header
	ostream << "#!/bin/bash" << endl;
	ostream << "#KStars DCOP script: " << currentScriptName << endl;
	ostream << "#by " << currentAuthor << endl;
	ostream << "#last modified: " << KStarsDateTime::tqcurrentDateTime().toString() << endl;
	ostream << "#" << endl;
	ostream << "KSTARS=`dcopfind -a 'kstars*'`" << endl;
	ostream << "MAIN=KStarsInterface" << endl;
	ostream << "CLOCK=clock#1" << endl;

	for ( ScriptFunction *sf = ScriptList.first(); sf; sf = ScriptList.next() )
	{
	        if (!sf->valid()) continue;
		if ( sf->isClockFunction() ) {
			ostream << clockpre << sf->scriptLine() << endl;
		} else {
			ostream << mainpre  << sf->scriptLine() << endl;
			if (sb->AppendINDIWait->isChecked() && !sf->INDIProperty().isEmpty())
			{
			  // Special case for telescope action, we need to know the parent property
			  if (sf->INDIProperty() == "CHECK")
			  {
			    if (sf->argVal(1) == "SLEW" || sf->argVal(1) == "TRACK" || sf->argVal(1) == "SYNC")
			      sf->setINDIProperty("ON_COORD_SET");
			    else if (sf->argVal(1) == "ABORT")
			      sf->setINDIProperty("ABORT_MOTION");
			    else
			      sf->setINDIProperty("PARK");
			  }
			  
			  if ( sf->argVal(0).tqcontains(" ")) 
			    ostream << mainpre << "waitForINDIAction " << "\"" << sf->argVal(0) << "\" " << sf->INDIProperty() << endl;
			  else
			    ostream << mainpre << "waitForINDIAction " << sf->argVal(0) << " " << sf->INDIProperty() << endl;
			}
		}
	}

	//Write script footer
	ostream << "##" << endl;
}

void ScriptBuilder::readScript( TQTextStream &istream ) {
	TQString line;

	while ( ! istream.eof() ) {
		line = istream.readLine();

		//look for name of script
		if ( line.tqcontains( "#KStars DCOP script: " ) )
			currentScriptName = line.mid( 21 ).stripWhiteSpace();

		//look for author of scriptbuilder
		if ( line.tqcontains( "#by " ) )
			currentAuthor = line.mid( 4 ).stripWhiteSpace();

		//Actual script functions
		if ( line.left(4) == "dcop" ) {

		//is ClockFunction?
			bool clockfcn( false );
			if ( line.tqcontains( "$CLOCK" ) ) clockfcn = true;

			//remove leading dcop prefix
			line = line.mid( 20 );

			//construct a stringlist that is fcn name and its arg name/value pairs
			TQStringList fn = TQStringList::split( " ", line );
			if ( parseFunction( fn ) )
			{
			  sb->ScriptListBox->insertItem( ScriptList.current()->name() );
			  // Initially, any read script is valid!
			  ScriptList.current()->setValid(true);
			}
			else kdWarning() << i18n( "Could not parse script.  Line was: %1" ).arg( line ) << endl;

		} // end if left(4) == "dcop"
	} // end while !eof()

	//Select first item in sb->ScriptListBox
	if ( sb->ScriptListBox->count() ) {
		sb->ScriptListBox->setCurrentItem( 0 );
		slotArgWidget();
	}
}

bool ScriptBuilder::parseFunction( TQStringList &fn )
{
        // clean up the string list first if needed
        // We need to perform this in case we havea quoted string "NGC 3000" because this will counted
        // as two arguments, and it should be counted as one.
        bool foundQuote(false), quoteProcessed(false);
	TQString cur, arg;
	TQStringList::iterator it;
	
	for (it = fn.begin(); it != fn.end(); ++it)
	{
	  cur = (*it);
	  
	  if ( cur.startsWith("\""))
	  {
	    arg += cur.right(cur.length() - 1);
	    arg += " ";
	    foundQuote = true;
	    quoteProcessed = true;
	  }
	  else if (cur.endsWith("\""))
	  {
	    arg += cur.left(cur.length() -1);
	    arg += "'";
	    foundQuote = false;
	  }
	  else if (foundQuote)
	  {
	    arg += cur;
	    arg += " ";
	  }
	  else
	  {
	    arg += cur;
	    arg += "'";
	  }
	}
	    
	if (quoteProcessed)
	  fn = TQStringList::split( "'", arg );
	
	//loop over known functions to find a name match
	for ( ScriptFunction *sf = KStarsFunctionList.first(); sf; sf = KStarsFunctionList.next() )
	{
		if ( fn[0] == sf->name() ) {

			if ( fn[0] == "setGeoLocation" ) {
				TQString city( fn[1] ), prov( "" ), cntry( fn[2] );
				if ( fn.count() == 4 ) { prov = fn[2]; cntry = fn[3]; }
				if ( fn.count() == 3 || fn.count() == 4 ) {
					ScriptList.append( new ScriptFunction( sf ) );
					ScriptList.current()->setArg( 0, city );
					ScriptList.current()->setArg( 1, prov );
					ScriptList.current()->setArg( 2, cntry );
				} else return false;

			} else if ( fn.count() != sf->numArgs() + 1 ) return false;

			ScriptList.append( new ScriptFunction( sf ) );

			for ( unsigned int i=0; i<sf->numArgs(); ++i )
				ScriptList.current()->setArg( i, fn[i+1] );

			return true;
		}
		
		for ( ScriptFunction *sf = INDIFunctionList.first(); sf; sf = INDIFunctionList.next() )
		{
		  if ( fn[0] == sf->name() )
		  {

		    if ( fn.count() != sf->numArgs() + 1 ) return false;

		    ScriptList.append( new ScriptFunction( sf ) );

		    for ( unsigned int i=0; i<sf->numArgs(); ++i )
		      ScriptList.current()->setArg( i, fn[i+1] );

		    return true;
		  }
		}
	}

	//if we get here, no function-name match was found
	return false;
}

void ScriptBuilder::setUnsavedChanges( bool b ) {
	UnsavedChanges = b;
	sb->SaveButton->setEnabled( b );
	sb->SaveAsButton->setEnabled( b );
}

void ScriptBuilder::slotEnableScriptNameOK() {
	snd->OKButton->setEnabled( ! snd->ScriptName->text().isEmpty() );
}

void ScriptBuilder::slotCopyFunction() {
	if ( ! UnsavedChanges ) setUnsavedChanges( true );

	int Pos = sb->ScriptListBox->currentItem() + 1;
	ScriptList.insert( Pos, new ScriptFunction( ScriptList.at( Pos-1 ) ) );
	//copy ArgVals
	for ( unsigned int i=0; i < ScriptList.at( Pos-1 )->numArgs(); ++i )
		ScriptList.at(Pos)->setArg(i, ScriptList.at( Pos-1 )->argVal(i) );

	sb->ScriptListBox->insertItem( ScriptList.current()->name(), Pos );
	sb->ScriptListBox->setSelected( Pos, true );
}

void ScriptBuilder::slotRemoveFunction() {
	setUnsavedChanges( true );

	int Pos = sb->ScriptListBox->currentItem();
	ScriptList.remove( Pos );
	sb->ScriptListBox->removeItem( Pos );
	if ( sb->ScriptListBox->count() == 0 ) {
		sb->ArgStack->raiseWidget( argBlank );
		sb->CopyButton->setEnabled( false );
		sb->RemoveButton->setEnabled( false );
	} else {
		sb->ScriptListBox->setSelected( Pos, true );
	}
}

void ScriptBuilder::slotAddFunction() {
  
        ScriptFunction *sc = NULL;
	TQListViewItem *currentItem = sb->FunctionListView->currentItem();
	
	if ( currentItem == NULL || currentItem->depth() == 0)
	  return;
	
	for (sc = KStarsFunctionList.first(); sc; sc = KStarsFunctionList.next())
	  if (sc->prototype() == currentItem->text(0))
	    break;
	
	 if (sc == NULL)
	 {
	   for (sc = INDIFunctionList.first(); sc; sc = INDIFunctionList.next())
	     if (sc->prototype() == currentItem->text(0))
	       break;
	   
	 }
	 
	 if (sc == NULL) return;
	  
	  setUnsavedChanges( true );

	  int Pos = sb->ScriptListBox->currentItem() + 1;

	  ScriptList.insert( Pos, new ScriptFunction(sc) );
	  sb->ScriptListBox->insertItem( ScriptList.current()->name(), Pos );
	  sb->ScriptListBox->setSelected( Pos, true );
}

void ScriptBuilder::slotMoveFunctionUp() {
	if ( sb->ScriptListBox->currentItem() > 0 ) {
		setUnsavedChanges( true );

		TQString t = sb->ScriptListBox->currentText();
		unsigned int n = sb->ScriptListBox->currentItem();

		ScriptFunction *tmp = ScriptList.take( n );
		ScriptList.insert( n-1, tmp );

		sb->ScriptListBox->removeItem( n );
		sb->ScriptListBox->insertItem( t, n-1 );
		sb->ScriptListBox->setSelected( n-1, true );
	}
}

void ScriptBuilder::slotMoveFunctionDown() {
	if ( sb->ScriptListBox->currentItem() > -1 &&
				sb->ScriptListBox->currentItem() < ((int) sb->ScriptListBox->count())-1 ) {
		setUnsavedChanges( true );

		TQString t = sb->ScriptListBox->currentText();
		unsigned int n = sb->ScriptListBox->currentItem();

		ScriptFunction *tmp = ScriptList.take( n );
		ScriptList.insert( n+1, tmp );

		sb->ScriptListBox->removeItem( n );
		sb->ScriptListBox->insertItem( t, n+1 );
		sb->ScriptListBox->setSelected( n+1, true );
	}
}

void ScriptBuilder::slotArgWidget() {
	//First, setEnabled on buttons that act on the selected script function
	if ( sb->ScriptListBox->currentItem() == -1 ) { //no selection
		sb->CopyButton->setEnabled( false );
		sb->RemoveButton->setEnabled( false );
		sb->UpButton->setEnabled( false );
		sb->DownButton->setEnabled( false );
	} else if ( sb->ScriptListBox->count() == 1 ) { //only one item, so disable up/down buttons
		sb->CopyButton->setEnabled( true );
		sb->RemoveButton->setEnabled( true );
		sb->UpButton->setEnabled( false );
		sb->DownButton->setEnabled( false );
	} else if ( sb->ScriptListBox->currentItem() == 0 ) { //first item selected
		sb->CopyButton->setEnabled( true );
		sb->RemoveButton->setEnabled( true );
		sb->UpButton->setEnabled( false );
		sb->DownButton->setEnabled( true );
	} else if ( sb->ScriptListBox->currentItem() == ((int) sb->ScriptListBox->count())-1 ) { //last item selected
		sb->CopyButton->setEnabled( true );
		sb->RemoveButton->setEnabled( true );
		sb->UpButton->setEnabled( true );
		sb->DownButton->setEnabled( false );
	} else { //other item selected
		sb->CopyButton->setEnabled( true );
		sb->RemoveButton->setEnabled( true );
		sb->UpButton->setEnabled( true );
		sb->DownButton->setEnabled( true );
	}

	//sb->RunButton enabled when script not empty.
	if ( sb->ScriptListBox->count() ) {
		sb->RunButton->setEnabled( true );
	} else {
		sb->RunButton->setEnabled( false );
		setUnsavedChanges( false );
	}

	//Display the function's arguments widget
	if ( sb->ScriptListBox->currentItem() > -1 &&
				sb->ScriptListBox->currentItem() < ((int) sb->ScriptListBox->count()) ) {
		TQString t = sb->ScriptListBox->currentText();
		unsigned int n = sb->ScriptListBox->currentItem();
		ScriptFunction *sf = ScriptList.at( n );

		if ( sf->name() == "lookTowards" ) {
			sb->ArgStack->raiseWidget( argLookToward );
			TQString s = sf->argVal(0);
			argLookToward->FocusEdit->setCurrentText( s );

		} else if ( sf->name() == "setRaDec" ) {
			bool ok(false);
			double r(0.0),d(0.0);
			dms ra(0.0);

			sb->ArgStack->raiseWidget( argSetRaDec );

			ok = !sf->argVal(0).isEmpty();
			if (ok) r = sf->argVal(0).toDouble(&ok);
			else argSetRaDec->RaBox->clear();
			if (ok) { ra.setH(r); argSetRaDec->RaBox->showInHours( ra ); }

			ok = !sf->argVal(1).isEmpty();
			if (ok) d = sf->argVal(1).toDouble(&ok);
			else argSetRaDec->DecBox->clear();
			if (ok) argSetRaDec->DecBox->showInDegrees( dms(d) );

		} else if ( sf->name() == "setAltAz" ) {
			bool ok(false);
			double x(0.0),y(0.0);

			sb->ArgStack->raiseWidget( argSetAltAz );

			ok = !sf->argVal(0).isEmpty();
			if (ok) y = sf->argVal(0).toDouble(&ok);
			else argSetAltAz->AzBox->clear();
			if (ok) argSetAltAz->AltBox->showInDegrees( dms(y) );
			else argSetAltAz->AltBox->clear();

			ok = !sf->argVal(1).isEmpty();
			x = sf->argVal(1).toDouble(&ok);
			if (ok) argSetAltAz->AzBox->showInDegrees( dms(x) );

		} else if ( sf->name() == "zoomIn" ) {
			sb->ArgStack->raiseWidget( argBlank );
			//no Args

		} else if ( sf->name() == "zoomOut" ) {
			sb->ArgStack->raiseWidget( argBlank );
			//no Args

		} else if ( sf->name() == "defaultZoom" ) {
			sb->ArgStack->raiseWidget( argBlank );
			//no Args

		} else if ( sf->name() == "zoom" ) {
			sb->ArgStack->raiseWidget( argZoom );
			bool ok(false);
			/*double z = */sf->argVal(0).toDouble(&ok);
			if (ok) argZoom->ZoomBox->setText( sf->argVal(0) );
			else argZoom->ZoomBox->setText( "2000." );

		} else if ( sf->name() == "exportImage" ) {
			sb->ArgStack->raiseWidget( argExportImage );
			argExportImage->ExportFileName->setURL( sf->argVal(0) );
			bool ok(false);
			int w, h;
			w = sf->argVal(1).toInt( &ok );
			if (ok) h = sf->argVal(2).toInt( &ok );
			if (ok) { 
				argExportImage->ExportWidth->setValue( w ); 
				argExportImage->ExportHeight->setValue( h );
			} else { 
				argExportImage->ExportWidth->setValue( ks->map()->width() ); 
				argExportImage->ExportHeight->setValue( ks->map()->height() );
			}

		} else if ( sf->name() == "printImage" ) {
			if ( sf->argVal(0) == i18n( "true" ) ) argPrintImage->UsePrintDialog->setChecked( true );
			else argPrintImage->UsePrintDialog->setChecked( false );
			if ( sf->argVal(1) == i18n( "true" ) ) argPrintImage->UseChartColors->setChecked( true );
			else argPrintImage->UseChartColors->setChecked( false );

		} else if ( sf->name() == "setLocalTime" ) {
			sb->ArgStack->raiseWidget( argSetLocalTime );
			bool ok(false);
			int year, month, day, hour, min, sec;

			year = sf->argVal(0).toInt(&ok);
			if (ok) month = sf->argVal(1).toInt(&ok);
			if (ok) day   = sf->argVal(2).toInt(&ok);
			if (ok) argSetLocalTime->DateBox->setDate( ExtDate( year, month, day ) );
			else argSetLocalTime->DateBox->setDate( ExtDate::tqcurrentDate() );

			hour = sf->argVal(3).toInt(&ok);
			if ( sf->argVal(3).isEmpty() ) ok = false;
			if (ok) min = sf->argVal(4).toInt(&ok);
			if (ok) sec = sf->argVal(5).toInt(&ok);
			if (ok) argSetLocalTime->TimeBox->setTime( TQTime( hour, min, sec ) );
			else argSetLocalTime->TimeBox->setTime( TQTime( TQTime::currentTime() ) );

		} else if ( sf->name() == "waitFor" ) {
			sb->ArgStack->raiseWidget( argWaitFor );
			bool ok(false);
			int sec = sf->argVal(0).toInt(&ok);
			if (ok) argWaitFor->DelayBox->setValue( sec );
			else argWaitFor->DelayBox->setValue( 0 );

		} else if ( sf->name() == "waitForKey" ) {
			sb->ArgStack->raiseWidget( argWaitForKey );
			if ( sf->argVal(0).length()==1 || sf->argVal(0).lower() == "space" )
				argWaitForKey->WaitKeyEdit->setText( sf->argVal(0) );
			else argWaitForKey->WaitKeyEdit->setText( "" );

		} else if ( sf->name() == "setTracking" ) {
			sb->ArgStack->raiseWidget( argSetTracking );
			if ( sf->argVal(0) == i18n( "true" ) ) argSetTracking->CheckTrack->setChecked( true  );
			else argSetTracking->CheckTrack->setChecked( false );

		} else if ( sf->name() == "changeViewOption" ) {
			sb->ArgStack->raiseWidget( argChangeViewOption );
			//find argVal(0) in the combobox...if it isn't there, it will select nothing
			argChangeViewOption->OptionName->setCurrentItem( sf->argVal(0) );
			argChangeViewOption->OptionValue->setText( sf->argVal(1) );

		} else if ( sf->name() == "setGeoLocation" ) {
			sb->ArgStack->raiseWidget( argSetGeoLocation );
			argSetGeoLocation->CityName->setText( sf->argVal(0) );
			argSetGeoLocation->ProvinceName->setText( sf->argVal(1) );
			argSetGeoLocation->CountryName->setText( sf->argVal(2) );

		} else if ( sf->name() == "setColor" ) {
			sb->ArgStack->raiseWidget( argSetColor );
			if ( sf->argVal(0).isEmpty() ) sf->setArg( 0, "SkyColor" );  //initialize default value
			argSetColor->ColorName->setCurrentItem( ks->data()->colorScheme()->nameFromKey( sf->argVal(0) ) );
			argSetColor->ColorValue->setColor( TQColor( sf->argVal(1).remove('\\') ) );

		} else if ( sf->name() == "loadColorScheme" ) {
			sb->ArgStack->raiseWidget( argLoadColorScheme );
			argLoadColorScheme->SchemeList->setCurrentItem( argLoadColorScheme->SchemeList->findItem( sf->argVal(0).remove('\"'), 0 ) );

		} else if ( sf->name() == "stop" ) {
			sb->ArgStack->raiseWidget( argBlank );
			//no Args

		} else if ( sf->name() == "start" ) {
			sb->ArgStack->raiseWidget( argBlank );
			//no Args

		} else if ( sf->name() == "setClockScale" ) {
			sb->ArgStack->raiseWidget( argTimeScale );
			bool ok(false);
			double ts = sf->argVal(0).toDouble(&ok);
			if (ok) argTimeScale->TimeScale->tsbox()->changeScale( float(ts) );
			else argTimeScale->TimeScale->tsbox()->changeScale( 0.0 );

		}
		else if (sf->name() == "startINDI") {
		  sb->ArgStack->raiseWidget( argStartINDI);
		  
		  argStartINDI->deviceName->setText(sf->argVal(0));
		  if (sf->argVal(1) == "true")
		    argStartINDI->LocalButton->setChecked(true);
		  else if (! sf->argVal(1).isEmpty())
		    argStartINDI->LocalButton->setChecked(false);
		}
		else if (sf->name() == "shutdownINDI") {
		  sb->ArgStack->raiseWidget( argShutdownINDI);
		  
		  //if (sf->valid()) kdDebug() << "begin: shutdown is valid" << endl;
		if (sb->ReuseINDIDeviceName->isChecked())
		  {
		    if (!sf->argVal(0).isEmpty())
		      argShutdownINDI->deviceName->setText(sf->argVal(0));
		    else if (argShutdownINDI->deviceName->text().isEmpty() || sf->argVal(0).isEmpty())
		    argShutdownINDI->deviceName->setText(lastINDIDeviceName);
		   else
		     slotINDIShutdown();
		  }
		  else argShutdownINDI->deviceName->setText(sf->argVal(0));
		  
		  //if (sf->valid()) kdDebug() << "end: shutdown is valid" << endl;
		}
		else if (sf->name() == "switchINDI") {
		  sb->ArgStack->raiseWidget( argSwitchINDI);
		  
		  if (sf->argVal(1) == "true" || sf->argVal(1).isEmpty())
		    argSwitchINDI->OnButton->setChecked(true);
		  else
		    argSwitchINDI->OffButton->setChecked(true);
		  
		  argSwitchINDI->deviceName->clear();
		  
		  if (sb->ReuseINDIDeviceName->isChecked())
		  {
		    if (!sf->argVal(0).isEmpty())
		      argSwitchINDI->deviceName->setText(sf->argVal(0));
		    else 
		      argSwitchINDI->deviceName->setText(lastINDIDeviceName);
		  }
		  else argSwitchINDI->deviceName->setText(sf->argVal(0));
		  
		}
		else if (sf->name() == "setINDIPort") {
		  sb->ArgStack->raiseWidget( argSetPortINDI);
		  
		  argSetPortINDI->devicePort->setText(sf->argVal(1));
		  
		  argSetPortINDI->deviceName->clear();
		  
		  if (sb->ReuseINDIDeviceName->isChecked())
		  {
		    if (!sf->argVal(0).isEmpty())
		      argSetPortINDI->deviceName->setText(sf->argVal(0));
		    else 
		      argSetPortINDI->deviceName->setText(lastINDIDeviceName);
		  }
		  else argSetPortINDI->deviceName->setText(sf->argVal(0));
		  
		}
		else if (sf->name() == "setINDITargetCoord") {
		  bool ok(false);
		  double r(0.0),d(0.0);
		  dms ra(0.0);
		  
		  sb->ArgStack->raiseWidget( argSetTargetCoordINDI);
		  
		  ok = !sf->argVal(1).isEmpty();
		  if (ok) r = sf->argVal(1).toDouble(&ok);
		  else argSetTargetCoordINDI->RaBox->clear();
		  if (ok) { ra.setH(r); argSetTargetCoordINDI->RaBox->showInHours( ra ); }

		  ok = !sf->argVal(2).isEmpty();
		  if (ok) d = sf->argVal(2).toDouble(&ok);
		  else argSetTargetCoordINDI->DecBox->clear();
		  if (ok) argSetTargetCoordINDI->DecBox->showInDegrees( dms(d) );
		  
		  argSetTargetCoordINDI->deviceName->clear();
		  
		  if (sb->ReuseINDIDeviceName->isChecked())
		  {
		    if (!sf->argVal(0).isEmpty())
		      argSetTargetCoordINDI->deviceName->setText(sf->argVal(0));
		    else 
		      argSetTargetCoordINDI->deviceName->setText(lastINDIDeviceName);
		  }
		  else argSetTargetCoordINDI->deviceName->setText(sf->argVal(0));
		  
		}
		else if (sf->name() == "setINDITargetName") {
		  sb->ArgStack->raiseWidget( argSetTargetNameINDI);
		  
		  argSetTargetNameINDI->objectName->setText(sf->argVal(1));
		  
		  argSetTargetNameINDI->deviceName->clear();
		  
		  if (sb->ReuseINDIDeviceName->isChecked())
		  {
		    if (!sf->argVal(0).isEmpty())
		      argSetTargetNameINDI->deviceName->setText(sf->argVal(0));
		    else
		      argSetTargetNameINDI->deviceName->setText(lastINDIDeviceName);
		  }
		  else argSetTargetNameINDI->deviceName->setText(sf->argVal(0));
		  
		}
		else if (sf->name() == "setINDIAction") {
		  sb->ArgStack->raiseWidget( argSetActionINDI);
		  
		  argSetActionINDI->actionName->setText(sf->argVal(1));
		  
		  argSetActionINDI->deviceName->clear();
		  
		  if (sb->ReuseINDIDeviceName->isChecked())
		  {
		    if (!sf->argVal(0).isEmpty())
		      argSetActionINDI->deviceName->setText(sf->argVal(0));
		    else
		      argSetActionINDI->deviceName->setText(lastINDIDeviceName);
		  }
		  else argSetActionINDI->deviceName->setText(sf->argVal(0));
		  
		}
		else if (sf->name() == "waitForINDIAction") {
		  sb->ArgStack->raiseWidget( argWaitForActionINDI);
		  
		  argWaitForActionINDI->actionName->setText(sf->argVal(1));
		  
		  argWaitForActionINDI->deviceName->clear();
		  
		  if (sb->ReuseINDIDeviceName->isChecked())
		  {
		    if (!sf->argVal(0).isEmpty())
		      argWaitForActionINDI->deviceName->setText(sf->argVal(0));
		    else
		      argWaitForActionINDI->deviceName->setText(lastINDIDeviceName);
		  }
		  else argWaitForActionINDI->deviceName->setText(sf->argVal(0));
		  
		}
		else if (sf->name() == "setINDIFocusSpeed") {
		  int t(0);
		  bool ok(false);
		  
		  sb->ArgStack->raiseWidget( argSetFocusSpeedINDI);

 		  t = sf->argVal(1).toInt(&ok);
		  if (ok) argSetFocusSpeedINDI->speedIN->setValue(t);
		  else argSetFocusSpeedINDI->speedIN->setValue(0);
		  
		  argSetFocusSpeedINDI->deviceName->clear();
		  
		  if (sb->ReuseINDIDeviceName->isChecked())
		  {
		    if (!sf->argVal(0).isEmpty())
		      argSetFocusSpeedINDI->deviceName->setText(sf->argVal(0));
		    else
		      argSetFocusSpeedINDI->deviceName->setText(lastINDIDeviceName);
		  }
		  else argSetFocusSpeedINDI->deviceName->setText(sf->argVal(0));
		  
		}
		else if (sf->name() == "startINDIFocus") {
		  sb->ArgStack->raiseWidget( argStartFocusINDI);
		  bool itemSet(false);
		  
		  for (int i=0; i < argStartFocusINDI->directionCombo->count(); i++)
		  {
		    if (argStartFocusINDI->directionCombo->text(i) == sf->argVal(1))
		    {
		      argStartFocusINDI->directionCombo->setCurrentItem(i);
		      itemSet = true;
		      break;
		    }
		  }
		  
		  if (!itemSet) argStartFocusINDI->directionCombo->setCurrentItem(0);
		  
		  argStartFocusINDI->deviceName->clear();
		  
		  if (sb->ReuseINDIDeviceName->isChecked())
		  {
		    if (!sf->argVal(0).isEmpty())
		      argStartFocusINDI->deviceName->setText(sf->argVal(0));
		    else
		      argStartFocusINDI->deviceName->setText(lastINDIDeviceName);
		  }
		  else argStartFocusINDI->deviceName->setText(sf->argVal(0));
		  
		}
		else if (sf->name() == "setINDIFocusTimeout") {
		  int t(0);
		  bool ok(false);
		  
		  sb->ArgStack->raiseWidget( argSetFocusTimeoutINDI);
		  
		  t = sf->argVal(1).toInt(&ok);
		  if (ok) argSetFocusTimeoutINDI->timeOut->setValue(t);
		  else argSetFocusTimeoutINDI->timeOut->setValue(0);
		  
		  argSetFocusTimeoutINDI->deviceName->clear();
		  
		  if (sb->ReuseINDIDeviceName->isChecked())
		  {
		    if (!sf->argVal(0).isEmpty())
		      argSetFocusTimeoutINDI->deviceName->setText(sf->argVal(0));
		    else
		      argSetFocusTimeoutINDI->deviceName->setText(lastINDIDeviceName);
		  }
		  else argSetFocusTimeoutINDI->deviceName->setText(sf->argVal(0));
		     
		  }
		  else if (sf->name() == "setINDIGeoLocation") {
		    bool ok(false);
		    double lo(0.0),la(0.0);
		  
		    sb->ArgStack->raiseWidget( argSetGeoLocationINDI);
		  
		    ok = !sf->argVal(1).isEmpty();
		    if (ok) lo = sf->argVal(1).toDouble(&ok);
		    else argSetGeoLocationINDI->longBox->clear();
		    if (ok) { argSetGeoLocationINDI->longBox->showInDegrees( dms(lo) ); }

		    ok = !sf->argVal(2).isEmpty();
		    if (ok) la = sf->argVal(2).toDouble(&ok);
		    else argSetGeoLocationINDI->latBox->clear();
		    if (ok) argSetGeoLocationINDI->latBox->showInDegrees( dms(la) );
		    
		    argSetGeoLocationINDI->deviceName->clear();
		    
		    if (sb->ReuseINDIDeviceName->isChecked())
		    {
		      if (!sf->argVal(0).isEmpty())
			argSetGeoLocationINDI->deviceName->setText(sf->argVal(0));
		      else
			argSetGeoLocationINDI->deviceName->setText(lastINDIDeviceName);
		    }
		    else argSetGeoLocationINDI->deviceName->setText(sf->argVal(0));
		    
		  }
		  else if (sf->name() == "startINDIExposure") {
		    int t(0);
		    bool ok(false);
		  
		    sb->ArgStack->raiseWidget( argStartExposureINDI);
		  
		    t = sf->argVal(1).toInt(&ok);
		    if (ok) argStartExposureINDI->timeOut->setValue(t);
		    else argStartExposureINDI->timeOut->setValue(0);
		    
		    argStartExposureINDI->deviceName->clear();
		    
		    if (sb->ReuseINDIDeviceName->isChecked())
		    {
		      if (!sf->argVal(0).isEmpty())
			argStartExposureINDI->deviceName->setText(sf->argVal(0));
		      else
			argStartExposureINDI->deviceName->setText(lastINDIDeviceName);
		    }
		    else argStartExposureINDI->deviceName->setText(sf->argVal(0));
		     
		  }
		  else if (sf->name() == "setINDIUTC") {
		    sb->ArgStack->raiseWidget( argSetUTCINDI);
		  
		    argSetUTCINDI->UTC->setText(sf->argVal(1));
		    
		    argSetUTCINDI->deviceName->clear();
		    
		    if (sb->ReuseINDIDeviceName->isChecked())
		    {
		      if (!sf->argVal(0).isEmpty())
			argSetUTCINDI->deviceName->setText(sf->argVal(0));
		      else 
			argSetUTCINDI->deviceName->setText(lastINDIDeviceName);
		    }
		    else argSetUTCINDI->deviceName->setText(sf->argVal(0));
		  
		  }
		  else if (sf->name() == "setINDIScopeAction") {
		    sb->ArgStack->raiseWidget( argSetScopeActionINDI);
		    bool itemSet(false);
		  
		    for (int i=0; i < argSetScopeActionINDI->actionCombo->count(); i++)
		    {
		      if (argSetScopeActionINDI->actionCombo->text(i) == sf->argVal(1))
		      {
			argSetScopeActionINDI->actionCombo->setCurrentItem(i);
			itemSet = true;
			break;
		      }
		    }
		  
		    if (!itemSet) argSetScopeActionINDI->actionCombo->setCurrentItem(0);
		  
		    argSetScopeActionINDI->deviceName->clear();
		    
		    if (sb->ReuseINDIDeviceName->isChecked())
		    {
		      if (!sf->argVal(0).isEmpty())
			argSetScopeActionINDI->deviceName->setText(sf->argVal(0));
		      else 
			argSetScopeActionINDI->deviceName->setText(lastINDIDeviceName);
		    }
		    else argSetScopeActionINDI->deviceName->setText(sf->argVal(0));
		  
		  }
		  else if (sf->name() == "setINDIFrameType") {
		    sb->ArgStack->raiseWidget( argSetFrameTypeINDI);
		    bool itemSet(false);
		  
		    for (int i=0; i < argSetFrameTypeINDI->typeCombo->count(); i++)
		    {
		      if (argSetFrameTypeINDI->typeCombo->text(i) == sf->argVal(1))
		      {
			argSetFrameTypeINDI->typeCombo->setCurrentItem(i);
			itemSet = true;
			break;
		      }
		    }
		  
		    if (!itemSet) argSetFrameTypeINDI->typeCombo->setCurrentItem(0);
		  
		    argSetFrameTypeINDI->deviceName->clear();
		    
		    if (sb->ReuseINDIDeviceName->isChecked())
		    {
		      if (!sf->argVal(0).isEmpty())
			argSetFrameTypeINDI->deviceName->setText(sf->argVal(0));
		      else
			argSetFrameTypeINDI->deviceName->setText(lastINDIDeviceName);
		    }
		    else argSetFrameTypeINDI->deviceName->setText(sf->argVal(0));
		  
		  }
		  else if (sf->name() == "setINDICCDTemp") {
		    int t(0);
		    bool ok(false);
		  
		    sb->ArgStack->raiseWidget( argSetCCDTempINDI);
		  
		    t = sf->argVal(1).toInt(&ok);
		    if (ok) argSetCCDTempINDI->temp->setValue(t);
		    else argSetCCDTempINDI->temp->setValue(0);
		  
		    argSetCCDTempINDI->deviceName->clear();
		    
		    if (sb->ReuseINDIDeviceName->isChecked())
		    {
		      if (!sf->argVal(0).isEmpty())
			argSetCCDTempINDI->deviceName->setText(sf->argVal(0));
		      else
			argSetCCDTempINDI->deviceName->setText(lastINDIDeviceName);
		    }
		    else argSetCCDTempINDI->deviceName->setText(sf->argVal(0));
		     
		  }
		  else if (sf->name() == "setINDIFilterNum") {
		    int t(0);
		    bool ok(false);
		  
		    sb->ArgStack->raiseWidget( argSetFilterNumINDI);
		  
		    t = sf->argVal(1).toInt(&ok);
		    if (ok) argSetFilterNumINDI->filter_num->setValue(t);
		    else argSetFilterNumINDI->filter_num->setValue(0);
		  
		    argSetFilterNumINDI->deviceName->clear();
		    
		    if (sb->ReuseINDIDeviceName->isChecked())
		    {
		      if (!sf->argVal(0).isEmpty())
			argSetFilterNumINDI->deviceName->setText(sf->argVal(0));
		      else
			argSetFilterNumINDI->deviceName->setText(lastINDIDeviceName);
		    }
		    else argSetFilterNumINDI->deviceName->setText(sf->argVal(0));
		     
		  }
	}
}

void ScriptBuilder::slotShowDoc() {
  ScriptFunction *sc = NULL;
  TQListViewItem *currentItem = sb->FunctionListView->currentItem();
	
  if ( currentItem == NULL || currentItem->depth() == 0)
    return;
	
  for (sc = KStarsFunctionList.first(); sc; sc = KStarsFunctionList.next())
    if (sc->prototype() == currentItem->text(0))
      break;
  
  if (sc == NULL)
  {
  for (sc = INDIFunctionList.first(); sc; sc = INDIFunctionList.next())
    if (sc->prototype() == currentItem->text(0))
      break;
  }
	
  if (sc == NULL)
  {
    sb->AddButton->setEnabled( false );
    kdWarning() << i18n( "Function index out of bounds." ) << endl;
    return;
  }
	  
    sb->AddButton->setEnabled( true );
    sb->FuncDoc->setText( sc->description() );
}

//Slots for Arg Widgets
void ScriptBuilder::slotFindCity() {
	LocationDialog ld( ks );

	if ( ld.exec() == TQDialog::Accepted ) {
		if ( ld.selectedCity() ) {
			// set new location names
			argSetGeoLocation->CityName->setText( ld.selectedCityName() );
			argSetGeoLocation->ProvinceName->setText( ld.selectedProvinceName() );
			argSetGeoLocation->CountryName->setText( ld.selectedCountryName() );

			ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );
			if ( sf->name() == "setGeoLocation" ) {
				setUnsavedChanges( true );

				sf->setArg( 0, ld.selectedCityName() );
				sf->setArg( 1, ld.selectedProvinceName() );
				sf->setArg( 2, ld.selectedCountryName() );
			} else {
				kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setGeoLocation" ) << endl;
			}
		}
	}
}

void ScriptBuilder::slotFindObject() {
	FindDialog fd( ks );

	if ( fd.exec() == TQDialog::Accepted && fd.currentItem() ) {
		setUnsavedChanges( true );

		argLookToward->FocusEdit->setCurrentText( fd.currentItem()->objName()->text() );
	}
}

void ScriptBuilder::slotINDIFindObject() {
  FindDialog fd( ks );

  if ( fd.exec() == TQDialog::Accepted && fd.currentItem() ) {
    setUnsavedChanges( true );

    argSetTargetNameINDI->objectName->setText( fd.currentItem()->objName()->text() );
  }
}

void ScriptBuilder::slotINDIWaitCheck(bool /*toggleState*/)
{
  
   setUnsavedChanges(true);  
  
}

void ScriptBuilder::slotShowOptions() {
	//Show tree-view of view options
	if ( otv->exec() == TQDialog::Accepted ) {
		argChangeViewOption->OptionName->setCurrentItem( otv->OptionsList->currentItem()->text(0) );
	}
}

void ScriptBuilder::slotLookToward() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "lookTowards" ) {
		setUnsavedChanges( true );

		sf->setArg( 0, argLookToward->FocusEdit->currentText() );
		sf->setValid(true);
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "lookTowards" ) << endl;
	}
}

void ScriptBuilder::slotRa() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "setRaDec" ) {
		//do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
		if ( argSetRaDec->RaBox->text().isEmpty() ) return;

		bool ok(false);
		dms ra = argSetRaDec->RaBox->createDms(false, &ok);
		if ( ok ) {
			setUnsavedChanges( true );

			sf->setArg( 0, TQString( "%1" ).arg( ra.Hours() ) );
			if ( ! sf->argVal(1).isEmpty() ) sf->setValid( true );

		} else {
			sf->setArg( 0, "" );
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setRaDec" ) << endl;
	}
}

void ScriptBuilder::slotDec() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "setRaDec" ) {
		//do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
		if ( argSetRaDec->DecBox->text().isEmpty() ) return;

		bool ok(false);
		dms dec = argSetRaDec->DecBox->createDms(true, &ok);
		if ( ok ) {
			setUnsavedChanges( true );

			sf->setArg( 1, TQString( "%1" ).arg( dec.Degrees() ) );
			if ( ! sf->argVal(0).isEmpty() ) sf->setValid( true );

		} else {
			sf->setArg( 1, "" );
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setRaDec" ) << endl;
	}
}

void ScriptBuilder::slotAz() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "setAltAz" ) {
		//do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
		if ( argSetAltAz->AzBox->text().isEmpty() ) return;

		bool ok(false);
		dms az = argSetAltAz->AzBox->createDms(true, &ok);
		if ( ok ) {
			setUnsavedChanges( true );
			sf->setArg( 1, TQString( "%1" ).arg( az.Degrees() ) );
			if ( ! sf->argVal(0).isEmpty() ) sf->setValid( true );
		} else {
			sf->setArg( 1, "" );
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setAltAz" ) << endl;
	}
}

void ScriptBuilder::slotAlt() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "setAltAz" ) {
		//do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
		if ( argSetAltAz->AltBox->text().isEmpty() ) return;

		bool ok(false);
		dms alt = argSetAltAz->AltBox->createDms(true, &ok);
		if ( ok ) {
			setUnsavedChanges( true );

			sf->setArg( 0, TQString( "%1" ).arg( alt.Degrees() ) );
			if ( ! sf->argVal(1).isEmpty() ) sf->setValid( true );
		} else {
			sf->setArg( 0, "" );
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setAltAz" ) << endl;
	}
}

void ScriptBuilder::slotChangeDate() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "setLocalTime" ) {
		setUnsavedChanges( true );

		ExtDate date = argSetLocalTime->DateBox->date();

		sf->setArg( 0, TQString( "%1" ).arg( date.year()   ) );
		sf->setArg( 1, TQString( "%1" ).arg( date.month()  ) );
		sf->setArg( 2, TQString( "%1" ).arg( date.day()    ) );
		if ( ! sf->argVal(3).isEmpty() ) sf->setValid( true );
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setLocalTime" ) << endl;
	}
}

void ScriptBuilder::slotChangeTime() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "setLocalTime" ) {
		setUnsavedChanges( true );

		TQTime time = argSetLocalTime->TimeBox->time();

		sf->setArg( 3, TQString( "%1" ).arg( time.hour()   ) );
		sf->setArg( 4, TQString( "%1" ).arg( time.minute() ) );
		sf->setArg( 5, TQString( "%1" ).arg( time.second() ) );
		if ( ! sf->argVal(0).isEmpty() ) sf->setValid( true );
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setLocalTime" ) << endl;
	}
}

void ScriptBuilder::slotWaitFor() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "waitFor" ) {
		bool ok(false);
		int delay = argWaitFor->DelayBox->text().toInt( &ok );

		if ( ok ) {
			setUnsavedChanges( true );

			sf->setArg( 0, TQString( "%1" ).arg( delay ) );
			sf->setValid( true );
		} else {
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "waitFor" ) << endl;
	}
}

void ScriptBuilder::slotWaitForKey() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "waitForKey" ) {
		TQString sKey = argWaitForKey->WaitKeyEdit->text().stripWhiteSpace();

		//DCOP script can only use single keystrokes; make sure entry is either one character,
		//or the word 'space'
		if ( sKey.length() == 1 || sKey == "space" ) {
			setUnsavedChanges( true );

			sf->setArg( 0, sKey );
			sf->setValid( true );
		} else {
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "waitForKey" ) << endl;
	}
}

void ScriptBuilder::slotTracking() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "setTracking" ) {
		setUnsavedChanges( true );

		sf->setArg( 0, ( argSetTracking->CheckTrack->isChecked() ? i18n( "true" ) : i18n( "false" ) ) );
		sf->setValid( true );
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setTracking" ) << endl;
	}
}

void ScriptBuilder::slotViewOption() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "changeViewOption" ) {
		if ( argChangeViewOption->OptionName->currentItem() >= 0
				&& argChangeViewOption->OptionValue->text().length() ) {
			setUnsavedChanges( true );

			sf->setArg( 0, argChangeViewOption->OptionName->currentText() );
			sf->setArg( 1, argChangeViewOption->OptionValue->text() );
			sf->setValid( true );
		} else {
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "changeViewOption" ) << endl;
	}
}

void ScriptBuilder::slotChangeCity() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "setGeoLocation" ) {
		TQString city =     argSetGeoLocation->CityName->text();

		if ( city.length() ) {
			setUnsavedChanges( true );

			sf->setArg( 0, city );
			if ( sf->argVal(2).length() ) sf->setValid( true );
		} else {
			sf->setArg( 0, "" );
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setGeoLocation" ) << endl;
	}
}

void ScriptBuilder::slotChangeProvince() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "setGeoLocation" ) {
		TQString province = argSetGeoLocation->ProvinceName->text();

		if ( province.length() ) {
			setUnsavedChanges( true );

			sf->setArg( 1, province );
			if ( sf->argVal(0).length() && sf->argVal(2).length() ) sf->setValid( true );
		} else {
			sf->setArg( 1, "" );
			//might not be invalid
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setGeoLocation" ) << endl;
	}
}

void ScriptBuilder::slotChangeCountry() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "setGeoLocation" ) {
		TQString country =  argSetGeoLocation->CountryName->text();

		if ( country.length() ) {
			setUnsavedChanges( true );

			sf->setArg( 2, country );
			if ( sf->argVal(0).length() ) sf->setValid( true );
		} else {
			sf->setArg( 2, "" );
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setGeoLocation" ) << endl;
	}
}

void ScriptBuilder::slotTimeScale() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "setClockScale" ) {
		setUnsavedChanges( true );

		sf->setArg( 0, TQString( "%1" ).arg( argTimeScale->TimeScale->tsbox()->timeScale() ) );
		sf->setValid( true );
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setClockScale" ) << endl;
	}
}

void ScriptBuilder::slotZoom() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "zoom" ) {
		setUnsavedChanges( true );

		bool ok(false);
		argZoom->ZoomBox->text().toDouble(&ok);
		if ( ok ) {
			sf->setArg( 0, argZoom->ZoomBox->text() );
			sf->setValid( true );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "zoom" ) << endl;
	}
}

void ScriptBuilder::slotExportImage() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "exportImage" ) {
		setUnsavedChanges( true );
		
		sf->setArg( 0, argExportImage->ExportFileName->url() );
		sf->setArg( 1, TQString("%1").arg( argExportImage->ExportWidth->value() ) );
		sf->setArg( 2, TQString("%1").arg( argExportImage->ExportHeight->value() ) );
		sf->setValid( true );
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "exportImage" ) << endl;
	}
}

void ScriptBuilder::slotPrintImage() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "printImage" ) {
		setUnsavedChanges( true );
		
		sf->setArg( 0, ( argPrintImage->UsePrintDialog->isChecked() ? i18n( "true" ) : i18n( "false" ) ) );
		sf->setArg( 1, ( argPrintImage->UseChartColors->isChecked() ? i18n( "true" ) : i18n( "false" ) ) );
		sf->setValid( true );
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "exportImage" ) << endl;
	}
}

void ScriptBuilder::slotChangeColorName() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "setColor" ) {
		setUnsavedChanges( true );
		
		argSetColor->ColorValue->setColor( ks->data()->colorScheme()->colorAt( argSetColor->ColorName->currentItem() ) );
		sf->setArg( 0, ks->data()->colorScheme()->keyAt( argSetColor->ColorName->currentItem() ) );
		TQString cname( argSetColor->ColorValue->color().name() );
		if ( cname.at(0) == '#' ) cname = "\\" + cname; //prepend a "\" so bash doesn't think we have a comment
		sf->setArg( 1, cname );
		sf->setValid( true );
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setColor" ) << endl;
	}
}

void ScriptBuilder::slotChangeColor() {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "setColor" ) {
		setUnsavedChanges( true );
		
		sf->setArg( 0, ks->data()->colorScheme()->keyAt( argSetColor->ColorName->currentItem() ) );
		TQString cname( argSetColor->ColorValue->color().name() );
		if ( cname.at(0) == '#' ) cname = "\\" + cname; //prepend a "\" so bash doesn't think we have a comment
		sf->setArg( 1, cname );
		sf->setValid( true );
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setColor" ) << endl;
	}
}

void ScriptBuilder::slotLoadColorScheme(TQListBoxItem */*i*/) {
	ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

	if ( sf->name() == "loadColorScheme" ) {
		setUnsavedChanges( true );
		
		sf->setArg( 0, "\"" + argLoadColorScheme->SchemeList->currentText() + "\"" );
		sf->setValid( true );
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "loadColorScheme" ) << endl;
	}
}

void ScriptBuilder::slotClose() {
	saveWarning();

	if ( !UnsavedChanges ) {
		emit closeClicked();
		reject();
	}
}

void ScriptBuilder::slotINDIStartDeviceName()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "startINDI" )
  {
    setUnsavedChanges( true );
    
    lastINDIDeviceName = argStartINDI->deviceName->text();
    
    sf->setArg(0, lastINDIDeviceName);
    sf->setArg(1, argStartINDI->LocalButton->isChecked() ? "true" : "false");
    sf->setValid(true);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "startINDI" ) << endl;
  }
  
}

void ScriptBuilder::slotINDIStartDeviceMode()
{
  
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "startINDI" )
  {
    setUnsavedChanges( true );
    
    sf->setArg(1, argStartINDI->LocalButton->isChecked() ? "true" : "false");
    if (! sf->argVal(0).isEmpty()) sf->setValid(true);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "startINDI" ) << endl;
  }
  
}

void ScriptBuilder::slotINDIShutdown()
{
  
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "shutdownINDI" )
  {
    if (argShutdownINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argShutdownINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argShutdownINDI->deviceName->text());
    sf->setValid(true);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "shutdownINDI" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISwitchDeviceName()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "switchINDI" )
  {
    if (argSwitchINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSwitchINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSwitchINDI->deviceName->text());
    sf->setArg(1, argSwitchINDI->OnButton->isChecked() ? "true" : "false");
    sf->setValid(true);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "switchdownINDI" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISwitchDeviceConnection()
{
  
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "switchINDI" )
  {
    
    if (sf->argVal(1) != (argSwitchINDI->OnButton->isChecked() ? "true" : "false"))
    setUnsavedChanges( true );
    
    sf->setArg(1, argSwitchINDI->OnButton->isChecked() ? "true" : "false");
    if (! sf->argVal(0).isEmpty()) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "switchINDI" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetPortDeviceName()
{
  
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIPort" )
  {
    if (argSetPortINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSetPortINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetPortINDI->deviceName->text());
    if (! sf->argVal(1).isEmpty()) sf->setValid(true);
    else sf->setValid(false);
    
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIPort" ) << endl;
  }
  
  
}

void ScriptBuilder::slotINDISetPortDevicePort()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIPort" )
  {
    
    if (argSetPortINDI->devicePort->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(1) != argSetPortINDI->devicePort->text())
    	setUnsavedChanges( true );
    
    sf->setArg(1, argSetPortINDI->devicePort->text());
    if (! sf->argVal(0).isEmpty()) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIPort" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetTargetCoordDeviceName()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDITargetCoord" )
  {
    if (argSetTargetCoordINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSetTargetCoordINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetTargetCoordINDI->deviceName->text());
    if ((! sf->argVal(1).isEmpty()) && (! sf->argVal(2).isEmpty())) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDITargetCoord" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetTargetCoordDeviceRA()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDITargetCoord" ) {
		//do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
    if ( argSetTargetCoordINDI->RaBox->text().isEmpty() )
    {
      sf->setValid(false);
      return;
    }

    bool ok(false);
    dms ra = argSetTargetCoordINDI->RaBox->createDms(false, &ok);
    if ( ok ) {
      
      if (sf->argVal(1) != TQString( "%1" ).arg( ra.Hours() ))
      	setUnsavedChanges( true );

      sf->setArg( 1, TQString( "%1" ).arg( ra.Hours() ) );
      if ( ( ! sf->argVal(0).isEmpty() ) && ( ! sf->argVal(2).isEmpty() )) sf->setValid( true );
      else sf->setValid(false);

    } else {
      sf->setArg( 1, "" );
      sf->setValid( false );
    }
  } else {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDITargetCoord" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetTargetCoordDeviceDEC()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDITargetCoord" ) {
		//do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
    if ( argSetTargetCoordINDI->DecBox->text().isEmpty() )
    {
      sf->setValid(false);
      return;
    }

    bool ok(false);
    dms dec = argSetTargetCoordINDI->DecBox->createDms(true, &ok);
    if ( ok ) {
      
      if (sf->argVal(2) != TQString( "%1" ).arg( dec.Degrees() ))
      	setUnsavedChanges( true );

      sf->setArg( 2, TQString( "%1" ).arg( dec.Degrees() ) );
      if ( ( ! sf->argVal(0).isEmpty() ) && ( ! sf->argVal(1).isEmpty() )) sf->setValid( true );
      else sf->setValid(false);
      
    } else {
      sf->setArg( 2, "" );
      sf->setValid( false );
    }
  } else {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDITargetCoord" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetTargetNameDeviceName()
{
  
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDITargetName" )
  {
    if (argSetTargetNameINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSetTargetNameINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetTargetNameINDI->deviceName->text());
    if ((! sf->argVal(1).isEmpty())) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDITargetName" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetTargetNameObjectName()
{
  
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDITargetName" )
  {
    if (argSetTargetNameINDI->objectName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(1) != argSetTargetNameINDI->objectName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(1, argSetTargetNameINDI->objectName->text());
    if ((! sf->argVal(0).isEmpty())) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDITargetName" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetActionDeviceName()
{
  
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIAction" )
  {
    if (argSetActionINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSetActionINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetActionINDI->deviceName->text());
    if ((! sf->argVal(1).isEmpty())) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIAction") << endl;
  }
  
}
	
void ScriptBuilder::slotINDISetActionName()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIAction" )
  {
    if (argSetActionINDI->actionName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(1) != argSetActionINDI->actionName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(1, argSetActionINDI->actionName->text());
    if ((! sf->argVal(0).isEmpty())) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIAction") << endl;
  }

}

void ScriptBuilder::slotINDIWaitForActionDeviceName()
{
  
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "waitForINDIAction" )
  {
    if (argWaitForActionINDI->deviceName->text().isEmpty())
    {
      return;
      sf->setValid(false);
    }
    
    if (sf->argVal(0) != argWaitForActionINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argWaitForActionINDI->deviceName->text());
    if ((! sf->argVal(1).isEmpty())) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "waitForINDIAction") << endl;
  }
  
}
	
void ScriptBuilder::slotINDIWaitForActionName()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "waitForINDIAction" )
  {
    if (argWaitForActionINDI->actionName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(1) != argWaitForActionINDI->actionName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(1, argWaitForActionINDI->actionName->text());
    if ((! sf->argVal(0).isEmpty())) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "waitForINDIAction") << endl;
  }
  
}

void ScriptBuilder::slotINDISetFocusSpeedDeviceName()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIFocusSpeed" )
  {
    if (argSetFocusSpeedINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSetFocusSpeedINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetFocusSpeedINDI->deviceName->text());
    sf->setArg(1, TQString("%1").arg(argSetFocusSpeedINDI->speedIN->value()));
    sf->setValid(true);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIFocusSpeed") << endl;
  }
  
}

void ScriptBuilder::slotINDISetFocusSpeed()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIFocusSpeed" )
  {
    
    if (sf->argVal(1).toInt() != argSetFocusSpeedINDI->speedIN->value())
    	setUnsavedChanges( true );
    
    sf->setArg(1, TQString("%1").arg(argSetFocusSpeedINDI->speedIN->value()));
    if ((! sf->argVal(0).isEmpty())) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIFocusSpeed") << endl;
  }
  
}

void ScriptBuilder::slotINDIStartFocusDeviceName()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "startINDIFocus" )
  {
    if (argStartFocusINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argStartFocusINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argStartFocusINDI->deviceName->text());
    sf->setArg(1, argStartFocusINDI->directionCombo->currentText());
    sf->setValid(true);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "StartINDIFocus") << endl;
  }
  
}


void ScriptBuilder::slotINDIStartFocusDirection()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "startINDIFocus" )
  {
    if (sf->argVal(1) != argStartFocusINDI->directionCombo->currentText())
    	setUnsavedChanges( true );
    
    sf->setArg(1, argStartFocusINDI->directionCombo->currentText());
    if ((! sf->argVal(0).isEmpty())) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "startINDIFocus") << endl;
  }
  
}

void ScriptBuilder::slotINDISetFocusTimeoutDeviceName()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIFocusTimeout" )
  {
    if (argSetFocusTimeoutINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSetFocusTimeoutINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetFocusTimeoutINDI->deviceName->text());
    sf->setArg(1, TQString("%1").arg(argSetFocusTimeoutINDI->timeOut->value()));
    sf->setValid(true);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIFocusTimeout") << endl;
  }
  
}

void ScriptBuilder::slotINDISetFocusTimeout()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIFocusTimeout" )
  {
    if (sf->argVal(1).toInt() != argSetFocusTimeoutINDI->timeOut->value())
    	setUnsavedChanges( true );
    
    sf->setArg(1, TQString("%1").arg(argSetFocusTimeoutINDI->timeOut->value()));
    if (! sf->argVal(0).isEmpty()) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIFocusTimeout") << endl;
  }
  
}

void ScriptBuilder::slotINDISetGeoLocationDeviceName()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIGeoLocation" )
  {
    if (argSetGeoLocationINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSetGeoLocationINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetGeoLocationINDI->deviceName->text());
    if ((! sf->argVal(1).isEmpty()) && (! sf->argVal(2).isEmpty())) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIGeoLocation" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetGeoLocationDeviceLong()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIGeoLocation" ) {
		//do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
    if ( argSetGeoLocationINDI->longBox->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }

    bool ok(false);
    dms longitude = argSetGeoLocationINDI->longBox->createDms(true, &ok);
    if ( ok ) {
      
      if (sf->argVal(1) != TQString( "%1" ).arg( longitude.Degrees()))
      	setUnsavedChanges( true );

      sf->setArg( 1, TQString( "%1" ).arg( longitude.Degrees() ) );
      if ( ( ! sf->argVal(0).isEmpty() ) && ( ! sf->argVal(2).isEmpty() )) sf->setValid( true );
      else sf->setValid(false);

    } else {
      sf->setArg( 1, "" );
      sf->setValid( false );
    }
  } else {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIGeoLocation" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetGeoLocationDeviceLat()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIGeoLocation" ) {
		//do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
    if ( argSetGeoLocationINDI->latBox->text().isEmpty() )
    {
      sf->setValid(false);
      return;
    }

    bool ok(false);
    dms latitude = argSetGeoLocationINDI->latBox->createDms(true, &ok);
    if ( ok ) {
      
      if (sf->argVal(2) != TQString( "%1" ).arg( latitude.Degrees()))
      	setUnsavedChanges( true );

      sf->setArg( 2, TQString( "%1" ).arg( latitude.Degrees() ) );
      if ( ( ! sf->argVal(0).isEmpty() ) && ( ! sf->argVal(1).isEmpty() )) sf->setValid( true );
      else sf->setValid(false);
      
    } else {
      sf->setArg( 2, "" );
      sf->setValid( false );
    }
  } else {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIGeoLocation" ) << endl;
  }
  
}

void ScriptBuilder::slotINDIStartExposureDeviceName()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "startINDIExposure" )
  {
    if (argStartExposureINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argStartExposureINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argStartExposureINDI->deviceName->text());
    sf->setArg(1, TQString("%1").arg(argStartExposureINDI->timeOut->value()));
    sf->setValid(true);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "startINDIExposure") << endl;
  }
  
}

void ScriptBuilder::slotINDIStartExposureTimeout()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "startINDIExposure" )
  {
    
    if (sf->argVal(1).toInt() != argStartExposureINDI->timeOut->value())
    	setUnsavedChanges( true );
    
    sf->setArg(1, TQString("%1").arg(argStartExposureINDI->timeOut->value()));
    if (! sf->argVal(0).isEmpty()) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "startINDIExposure") << endl;
  }
  
}

void ScriptBuilder::slotINDISetUTCDeviceName()
{
  
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIUTC" )
  {
    if (argSetUTCINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSetUTCINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetUTCINDI->deviceName->text());
    if (! sf->argVal(1).isEmpty()) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIUTC" ) << endl;
  }
  
  
}

void ScriptBuilder::slotINDISetUTC()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIUTC" )
  {
    
    if (argSetUTCINDI->UTC->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(1) != argSetUTCINDI->UTC->text())
    setUnsavedChanges( true );
    
    sf->setArg(1, argSetUTCINDI->UTC->text());
    if (! sf->argVal(0).isEmpty()) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIUTC" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetScopeActionDeviceName()
{
  
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIScopeAction" )
  {
    if (argSetScopeActionINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSetScopeActionINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetScopeActionINDI->deviceName->text());
    sf->setArg(1, argSetScopeActionINDI->actionCombo->currentText());
    sf->setINDIProperty("CHECK");
    sf->setValid(true);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIScopeAction" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetScopeAction()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIScopeAction" )
  {
    
    if (sf->argVal(1) != argSetScopeActionINDI->actionCombo->currentText())
    	setUnsavedChanges( true );
    
    sf->setArg(1, argSetScopeActionINDI->actionCombo->currentText());
    sf->setINDIProperty("CHECK");
    if ((! sf->argVal(0).isEmpty())) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIScopeAction") << endl;
  }
  
}

void ScriptBuilder::slotINDISetFrameTypeDeviceName()
{
  
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIFrameType" )
  {
    if (argSetFrameTypeINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSetFrameTypeINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetFrameTypeINDI->deviceName->text());
    sf->setArg(1, argSetFrameTypeINDI->typeCombo->currentText());
    sf->setValid(true);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIFrameType" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetFrameType()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIFrameType" )
  {
    
    if (sf->argVal(1) != argSetFrameTypeINDI->typeCombo->currentText())
    	setUnsavedChanges( true );
    
    sf->setArg(1, argSetFrameTypeINDI->typeCombo->currentText());
    if ((! sf->argVal(0).isEmpty())) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIFrameType") << endl;
  }
  
}

void ScriptBuilder::slotINDISetCCDTempDeviceName()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDICCDTemp" )
  {
    if (argSetCCDTempINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSetCCDTempINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetCCDTempINDI->deviceName->text());
    sf->setArg(1, TQString("%1").arg(argSetCCDTempINDI->temp->value()));
    sf->setValid(true);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDICCDTemp") << endl;
  }
  
}

void ScriptBuilder::slotINDISetCCDTemp()
{
  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDICCDTemp" )
  {
    
    if (sf->argVal(1).toInt() != argSetCCDTempINDI->temp->value())
    	setUnsavedChanges( true );
    
    sf->setArg(1, TQString("%1").arg(argSetCCDTempINDI->temp->value()));
    if (! sf->argVal(0).isEmpty()) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDICCDTemp") << endl;
  }
  
}

void ScriptBuilder::slotINDISetFilterNumDeviceName()
{

  ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIFilterNum" )
  {
    if (argSetFilterNumINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSetFilterNumINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetFilterNumINDI->deviceName->text());
    sf->setArg(1, TQString("%1").arg(argSetFilterNumINDI->filter_num->value()));
    sf->setValid(true);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIFilterNum") << endl;
  }
}

void ScriptBuilder::slotINDISetFilterNum()
{

 ScriptFunction *sf = ScriptList.at( sb->ScriptListBox->currentItem() );

  if ( sf->name() == "setINDIFilterNum" )
  {
    
    if (sf->argVal(1).toInt() != argSetFilterNumINDI->filter_num->value())
    	setUnsavedChanges( true );
    
    sf->setArg(1, TQString("%1").arg(argSetFilterNumINDI->filter_num->value()));
    if (! sf->argVal(0).isEmpty()) sf->setValid(true);
    else sf->setValid(false);
  }
  else
  {
    kdWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIFilterNum") << endl;
  }


}
	
#include "scriptbuilder.moc"
