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
#include <klocale.h>
#include <kcombobox.h>
#include <kicontheme.h>
#include <kiconloader.h>
#include <klistbox.h>
#include <klistview.h>
#include <ktextedit.h>
#include <kdatewidget.h>
#include <kmessagebox.h>
#include <kfiledialog.h>

#include <qcheckbox.h>
#include <qdatetimeedit.h>
#include <qwidgetstack.h>

#include <kdebug.h>

#include "kstars.h"
#include "finddialog.h"
#include "locationdialog.h"
#include "scriptbuilder.h"

ScriptBuilder::ScriptBuilder( QWidget *parent, const char *name )
 : ScriptBuilderUI( parent, name ), UnsavedChanges(false), currentFileURL(),
		currentPath( QDir::homeDirPath() ), currentScriptName(), currentAuthor() {

	ks = (KStars*)parent;
	
	FunctionList.setAutoDelete( TRUE );
	ScriptList.setAutoDelete( TRUE );
	
	//Initialize function templates and descriptions
	FunctionList.append( new ScriptFunction( "lookTowards", i18n( "Point the display at the specified location. %1 can be the name of an object, a cardinal point on the compass, or 'zenith'" ),
			false, "QString", "dir" ) );
	FunctionList.append( new ScriptFunction( "setRaDec", i18n( "Point the display at the specified RA/Dec coordinates.  %1 is expressed in Hours; %2 is expressed in Degrees" ),
			false, "double", "ra", "double", "dec" ) );
	FunctionList.append( new ScriptFunction( "setAltAz", i18n( "Point the display at the specified Alt/Az coordinates.  %1 and %2 are expressed in Degrees" ),
			false, "double", "alt", "double", "az" ) );
	FunctionList.append( new ScriptFunction( "zoomIn", i18n( "Increase the display Zoom Level." ), false ) );
	FunctionList.append( new ScriptFunction( "zoomOut", i18n( "Increase the display Zoom Level." ), false ) );
	FunctionList.append( new ScriptFunction( "defaultZoom", i18n( "Set the display Zoom Level to its default value." ), false ) );
	FunctionList.append( new ScriptFunction( "setLocalTime", i18n( "Set the system clock to the specified Local Time." ),
			false, "int", "year", "int", "month", "int", "day", "int", "hour", "int", "minute", "int", "second" ) );
	FunctionList.append( new ScriptFunction( "waitFor", i18n( "Pause script execution for %1 seconds" ), false, "double", "sec" ) );
	FunctionList.append( new ScriptFunction( "waitForKey", i18n( "Halt script execution until the key %1 is pressed.  Only single-key strokes are possible; use 'space' for the spacebar" ), 
			false, "QString", "key" ) );
	FunctionList.append( new ScriptFunction( "setTracking", i18n( "Set whether the display is tracking the current location." ), false, "bool", "track" ) );
	FunctionList.append( new ScriptFunction( "changeViewOption", i18n( "Change view option named %1 to value %2." ), false, "QString", "opName", "QString", "opValue" ) );
	FunctionList.append( new ScriptFunction( "setGeoLocation", i18n( "Set the geographic location to the city specified by %1, %2 and %3." ), 
			false, "QString", "cityName", "QString", "provinceName", "QString", "countryName" ) );
	FunctionList.append( new ScriptFunction( "stop", i18n( "Halt the simulation clock." ), true ) );
	FunctionList.append( new ScriptFunction( "start", i18n( "Start the simulation clock." ), true ) );
	FunctionList.append( new ScriptFunction( "setClockScale", i18n( "Set the timescale of the simulation clock to %1.  1.0 means real-time; 2.0 means twice real-time; etc." ), true, "double", "scale" ) );
	
	//Fill in the FuncListBox KListBox
	for ( ScriptFunction *sf = FunctionList.first(); sf; sf = FunctionList.next() ) 
		FuncListBox->insertItem( sf->prototype() );

	//Add icons to Push Buttons
	KIconLoader *icons = KGlobal::iconLoader();
	NewButton->setPixmap( icons->loadIcon( "filenew", KIcon::Toolbar ) );
	OpenButton->setPixmap( icons->loadIcon( "fileopen", KIcon::Toolbar ) );
	SaveButton->setPixmap( icons->loadIcon( "filesave", KIcon::Toolbar ) );
	SaveAsButton->setPixmap( icons->loadIcon( "filesaveas", KIcon::Toolbar ) );
	CopyButton->setPixmap( icons->loadIcon( "reload", KIcon::Toolbar ) );
	AddButton->setPixmap( icons->loadIcon( "back", KIcon::Toolbar ) );
	RemoveButton->setPixmap( icons->loadIcon( "forward", KIcon::Toolbar ) );
	UpButton->setPixmap( icons->loadIcon( "up", KIcon::Toolbar ) );
	DownButton->setPixmap( icons->loadIcon( "down", KIcon::Toolbar ) );
	
	//Prepare the widget stack
	argBlank = new QWidget( ArgStack );
	argLookToward = new ArgLookToward( ArgStack );
	argSetRaDec = new ArgSetRaDec( ArgStack );
	argSetAltAz = new ArgSetAltAz( ArgStack );
	argSetLocalTime = new ArgSetLocalTime( ArgStack );
	argWaitFor = new ArgWaitFor( ArgStack );
	argWaitForKey = new ArgWaitForKey( ArgStack );
	argSetTracking = new ArgSetTrack( ArgStack );
	argChangeViewOption = new ArgChangeViewOption( ArgStack );
	argSetGeoLocation = new ArgSetGeoLocation( ArgStack );
	argTimeScale = new ArgTimeScale( ArgStack );

	ArgStack->addWidget( argBlank );
	ArgStack->addWidget( argLookToward );
	ArgStack->addWidget( argSetRaDec );
	ArgStack->addWidget( argSetAltAz );
	ArgStack->addWidget( argSetLocalTime );
	ArgStack->addWidget( argWaitFor );
	ArgStack->addWidget( argWaitForKey );
	ArgStack->addWidget( argSetTracking );
	ArgStack->addWidget( argChangeViewOption );
	ArgStack->addWidget( argSetGeoLocation );
	ArgStack->addWidget( argTimeScale );
	
	ArgStack->raiseWidget( 0 );
	
	snd = new ScriptNameDialog( ks );
	otv = new OptionsTreeView( ks );
	
	initViewOptions();

	//Connections for Arg Widgets
	connect( argSetGeoLocation->FindCityButton, SIGNAL( clicked() ), this, SLOT( slotFindCity() ) );
	connect( argLookToward->FindButton, SIGNAL( clicked() ), this, SLOT( slotFindObject() ) );
	connect( argChangeViewOption->TreeButton, SIGNAL( clicked() ), this, SLOT( slotShowOptions() ) );
	
	connect( argLookToward->FocusEdit, SIGNAL( textChanged(const QString &) ), this, SLOT( slotLookToward() ) );
	connect( argSetRaDec->RaBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotRa() ) );
	connect( argSetRaDec->DecBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotDec() ) );
	connect( argSetAltAz->AltBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotAlt() ) );
	connect( argSetAltAz->AzBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotAz() ) );
	connect( argSetLocalTime->DateBox, SIGNAL( changed(QDate) ), this, SLOT( slotChangeDate() ) );
	connect( argSetLocalTime->TimeBox, SIGNAL( valueChanged(const QTime&) ), this, SLOT( slotChangeTime() ) );
	connect( argWaitFor->DelayBox, SIGNAL( valueChanged(int) ), this, SLOT( slotWaitFor() ) );
	connect( argWaitForKey->WaitKeyEdit, SIGNAL( textChanged(const QString &) ), this, SLOT( slotWaitForKey() ) );
	connect( argSetTracking->CheckTrack, SIGNAL( stateChanged(int) ), this, SLOT( slotTracking() ) );
	connect( argChangeViewOption->OptionName, SIGNAL( textChanged(const QString &) ), this, SLOT( slotViewOption() ) );
	connect( argChangeViewOption->OptionValue, SIGNAL( textChanged(const QString &) ), this, SLOT( slotViewOption() ) );
	connect( argSetGeoLocation->CityName, SIGNAL( textChanged(const QString &) ), this, SLOT( slotChangeCity() ) );
	connect( argSetGeoLocation->ProvinceName, SIGNAL( textChanged(const QString &) ), this, SLOT( slotChangeProvince() ) );
	connect( argSetGeoLocation->CountryName, SIGNAL( textChanged(const QString &) ), this, SLOT( slotChangeCountry() ) );
	connect( argTimeScale->TimeScale, SIGNAL( scaleChanged(float) ), this, SLOT( slotTimeScale() ) );

	connect( snd->ScriptName, SIGNAL( textChanged(const QString &) ), this, SLOT( slotEnableScriptNameOK() ) );
	
	//disbale some buttons
	CopyButton->setEnabled( false );
	AddButton->setEnabled( false );
	RemoveButton->setEnabled( false );
	UpButton->setEnabled( false );
	DownButton->setEnabled( false );
	SaveButton->setEnabled( false );
	SaveAsButton->setEnabled( false );

}

ScriptBuilder::~ScriptBuilder()
{
}

void ScriptBuilder::initViewOptions() {
	otv->OptionsList->setRootIsDecorated( true );
	
	//InfoBoxes
	opsGUI = new QListViewItem( otv->OptionsList, i18n( "InfoBoxes" ) );
	new QListViewItem( opsGUI, "ShowInfoBoxes", i18n( "Toggle display of all InfoBoxes" ), i18n( "bool" ) );
	new QListViewItem( opsGUI, "ShowTimeBox", i18n( "Toggle display of Time InfoBox" ), i18n( "bool" ) );
	new QListViewItem( opsGUI, "ShowGeoBox", i18n( "Toggle display of Geographic InfoBox" ), i18n( "bool" ) );
	new QListViewItem( opsGUI, "ShowFocusBox", i18n( "Toggle display of Focus InfoBox" ), i18n( "bool" ) );
	new QListViewItem( opsGUI, "ShadeTimeBox", i18n( "(un)Shade Time InfoBox" ), i18n( "bool" ) );
	new QListViewItem( opsGUI, "ShadeGeoBox", i18n( "(un)Shade Geographic InfoBox" ), i18n( "bool" ) );
	new QListViewItem( opsGUI, "ShadeFocusBox", i18n( "(un)Shade Focus InfoBox" ), i18n( "bool" ) );
	argChangeViewOption->OptionName->insertItem( "ShowInfoBoxes" );
	argChangeViewOption->OptionName->insertItem( "ShowTimeBox" );
	argChangeViewOption->OptionName->insertItem( "ShowGeoBox" );
	argChangeViewOption->OptionName->insertItem( "ShowFocusBox" );
	argChangeViewOption->OptionName->insertItem( "ShadeTimeBox" );
	argChangeViewOption->OptionName->insertItem( "ShadeGeoBox" );
	argChangeViewOption->OptionName->insertItem( "ShadeFocusBox" );

	//Toolbars
	opsToolbar = new QListViewItem( otv->OptionsList, i18n( "Toolbars" ) );
	new QListViewItem( opsToolbar, "ShowMainToolBar", i18n( "Toggle display of main toolbar" ), i18n( "bool" ) );
	new QListViewItem( opsToolbar, "ShowViewToolBar", i18n( "Toggle display of view toolbar" ), i18n( "bool" ) );
	argChangeViewOption->OptionName->insertItem( "ShowMainToolBar" );
	argChangeViewOption->OptionName->insertItem( "ShowViewToolBar" );
	
	//Show Objects
	opsShowObj = new QListViewItem( otv->OptionsList, i18n( "Show Objects" ) );
	new QListViewItem( opsShowObj, "ShowSAO", i18n( "Toggle display of Stars" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowDeepSky", i18n( "Toggle display of all deep-sky objects" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowMess", i18n( "Toggle display of Messier object symbols" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowMessImages", i18n( "Toggle display of Messier object images" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowNGC", i18n( "Toggle display of NGC objects" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowIC", i18n( "Toggle display of IC objects" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowPlanets", i18n( "Toggle display of all solar system bodies" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowSun", i18n( "Toggle display of Sun" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowMoon", i18n( "Toggle display of Moon" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowMercury", i18n( "Toggle display of Mercury" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowVenus", i18n( "Toggle display of Venus" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowMars", i18n( "Toggle display of Mars" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowJupiter", i18n( "Toggle display of Jupiter" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowSaturn", i18n( "Toggle display of Saturn" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowUranus", i18n( "Toggle display of Uranus" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowNeptune", i18n( "Toggle display of Neptune" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowPluto", i18n( "Toggle display of Pluto" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowAsteroids", i18n( "Toggle display of Asteroids" ), i18n( "bool" ) );
	new QListViewItem( opsShowObj, "ShowComets", i18n( "Toggle display of Comets" ), i18n( "bool" ) );
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

	opsShowOther = new QListViewItem( otv->OptionsList, i18n( "Show Other" ) );
	new QListViewItem( opsShowOther, "ShowCLines", i18n( "Toggle display of constellation lines" ), i18n( "bool" ) );
	new QListViewItem( opsShowOther, "ShowCNames", i18n( "Toggle display of constellation names" ), i18n( "bool" ) );
	new QListViewItem( opsShowOther, "ShowMilkyWay", i18n( "Toggle display of Milky Way" ), i18n( "bool" ) );
	new QListViewItem( opsShowOther, "ShowGrid", i18n( "Toggle display of the coordinate grid" ), i18n( "bool" ) );
	new QListViewItem( opsShowOther, "ShowEquator", i18n( "Toggle display of the celestial equator" ), i18n( "bool" ) );
	new QListViewItem( opsShowOther, "ShowEcliptic", i18n( "Toggle display of the ecliptic" ), i18n( "bool" ) );
	new QListViewItem( opsShowOther, "ShowHorizon", i18n( "Toggle display of the horizon line" ), i18n( "bool" ) );
	new QListViewItem( opsShowOther, "ShowGround", i18n( "Toggle display of the opaque ground" ), i18n( "bool" ) );
	new QListViewItem( opsShowOther, "drawStarName", i18n( "Toggle display of star name labels" ), i18n( "bool" ) );
	new QListViewItem( opsShowOther, "drawStarMagnitude", i18n( "Toggle display of star magnitude labels" ), i18n( "bool" ) );
	new QListViewItem( opsShowOther, "drawAsteroidName", i18n( "Toggle display of asteroid name labels" ), i18n( "bool" ) );
	new QListViewItem( opsShowOther, "drawCometName", i18n( "Toggle display of comet name labels" ), i18n( "bool" ) );
	new QListViewItem( opsShowOther, "drawPlanetName", i18n( "Toggle display of planet name labels" ), i18n( "bool" ) );
	new QListViewItem( opsShowOther, "drawPlanetImage", i18n( "Toggle display of planet images" ), i18n( "bool" ) );
	argChangeViewOption->OptionName->insertItem( "ShowCLines" );
	argChangeViewOption->OptionName->insertItem( "ShowCNames" );
	argChangeViewOption->OptionName->insertItem( "ShowMilkyWay" );
	argChangeViewOption->OptionName->insertItem( "ShowGrid" );
	argChangeViewOption->OptionName->insertItem( "ShowEquator" );
	argChangeViewOption->OptionName->insertItem( "ShowEcliptic" );
	argChangeViewOption->OptionName->insertItem( "ShowHorizon" );
	argChangeViewOption->OptionName->insertItem( "ShowGround" );
	argChangeViewOption->OptionName->insertItem( "drawStarName" );
	argChangeViewOption->OptionName->insertItem( "drawStarMagnitude" );
	argChangeViewOption->OptionName->insertItem( "drawAsteroidName" );
	argChangeViewOption->OptionName->insertItem( "drawCometName" );
	argChangeViewOption->OptionName->insertItem( "drawPlanetName" );
	argChangeViewOption->OptionName->insertItem( "drawPlanetImage" );
	
	opsCName = new QListViewItem( otv->OptionsList, i18n( "Constellation Names" ) );
	new QListViewItem( opsCName, "UseLatinConstellationNames", i18n( "Show Latin constellation names" ), i18n( "bool" ) );
	new QListViewItem( opsCName, "UseLocalConstellationNames", i18n( "Show constellation names in local language" ), i18n( "bool" ) );
	new QListViewItem( opsCName, "UseAbbrevConstellationNames", i18n( "Show IAU-standard constellation abbreviations" ), i18n( "bool" ) );
	argChangeViewOption->OptionName->insertItem( "UseLatinConstellationNames" );
	argChangeViewOption->OptionName->insertItem( "UseLocalConstellationNames" );
	argChangeViewOption->OptionName->insertItem( "UseAbbrevConstellationNames" );
	
	opsHide = new QListViewItem( otv->OptionsList, i18n( "Hide Items" ) );
	new QListViewItem( opsHide, "HideOnSlew", i18n( "Toggle whether objects hidden while slewing display" ), i18n( "bool" ) );
	new QListViewItem( opsHide, "SlewTimeScale", i18n( "Timestep threshhold (in seconds) for hiding objects" ), i18n( "double" ) );
	new QListViewItem( opsHide, "HideStars", i18n( "Hide faint stars while slewing?" ), i18n( "bool" ) );
	new QListViewItem( opsHide, "HidePlanets", i18n( "Hide solar system bodies while slewing?" ), i18n( "bool" ) );
	new QListViewItem( opsHide, "HideMess", i18n( "Hide Messier objects while slewing?" ), i18n( "bool" ) );
	new QListViewItem( opsHide, "HideNGC", i18n( "Hide NGC objects while slewing?" ), i18n( "bool" ) );
	new QListViewItem( opsHide, "HideIC", i18n( "Hide IC objects while slewing?" ), i18n( "bool" ) );
	new QListViewItem( opsHide, "HideMW", i18n( "Hide Milky Way while slewing?" ), i18n( "bool" ) );
	new QListViewItem( opsHide, "HideCNames", i18n( "Hide constellation names while slewing?" ), i18n( "bool" ) );
	new QListViewItem( opsHide, "HideCLines", i18n( "Hide constellation lines while slewing?" ), i18n( "bool" ) );
	new QListViewItem( opsHide, "HideGrid", i18n( "Hide coordinate grid while slewing?" ), i18n( "bool" ) );
	argChangeViewOption->OptionName->insertItem( "HideOnSlew" );
	argChangeViewOption->OptionName->insertItem( "SlewTimeScale" );
	argChangeViewOption->OptionName->insertItem( "HideStars" );
	argChangeViewOption->OptionName->insertItem( "HidePlanets" );
	argChangeViewOption->OptionName->insertItem( "HideMess" );
	argChangeViewOption->OptionName->insertItem( "HideNGC" );
	argChangeViewOption->OptionName->insertItem( "HideIC" );
	argChangeViewOption->OptionName->insertItem( "HideMW" );
	argChangeViewOption->OptionName->insertItem( "HideCNames" );
	argChangeViewOption->OptionName->insertItem( "HideCLines" );
	argChangeViewOption->OptionName->insertItem( "HideGrid" );
	
	opsSkymap = new QListViewItem( otv->OptionsList, i18n( "Skymap options" ) );
	new QListViewItem( opsSkymap, "UseAltAz", i18n( "Use Horizontal coordinates? (otherwise, use Equatorial)" ), i18n( "bool" ) );
	new QListViewItem( opsSkymap, "ZoomLevel", i18n( "Set the Zoom Level" ), i18n( "int" ) );
	new QListViewItem( opsSkymap, "Target Symbol", i18n( "Select Target symbol (0=none, 1=circle, 2=crosshairs, 3=bullseye, 4=rectangle)" ), i18n( "int" ) );
	new QListViewItem( opsSkymap, "AnimateSlewing", i18n( "Use animated slewin? (otherwise, \"snap\" to new focus)" ), i18n( "bool" ) );
	new QListViewItem( opsSkymap, "UseRefraction", i18n( "Correct for atmospheric refraction?" ), i18n( "bool" ) );
	new QListViewItem( opsSkymap, "UseAutoLabel", i18n( "Automatically attach name label to centered object?" ), i18n( "bool" ) );
	new QListViewItem( opsSkymap, "UseAutoTrail", i18n( "Automatically add trail to centered solar system body?" ), i18n( "bool" ) );
	new QListViewItem( opsSkymap, "FadePlanetTrails", i18n( "Planet trails fade to sky color? (otherwise color is constant)" ), i18n( "bool" ) );
	argChangeViewOption->OptionName->insertItem( "UseAltAz" );
	argChangeViewOption->OptionName->insertItem( "ZoomLevel" );
	argChangeViewOption->OptionName->insertItem( "TargetSymbol" );
	argChangeViewOption->OptionName->insertItem( "UseRefraction" );
	argChangeViewOption->OptionName->insertItem( "UseAutoLabel" );
	argChangeViewOption->OptionName->insertItem( "UseAutoTrail" );
	argChangeViewOption->OptionName->insertItem( "AnimateSlewing" );
	argChangeViewOption->OptionName->insertItem( "FadePlanetTrails" );
	
	opsLimit = new QListViewItem( otv->OptionsList, i18n( "Limits" ) );
	new QListViewItem( opsLimit, "magLimitDrawStar", i18n( "magnitude of faintest star drawn on map" ), i18n( "double" ) );
	new QListViewItem( opsLimit, "magLimitDrawStarInfo", i18n( "magnitude of faintest star labeled on map" ), i18n( "double" ) );
	new QListViewItem( opsLimit, "magLimitHideStar", i18n( "magnitude of brightest star hidden while slewing" ), i18n( "double" ) );
	new QListViewItem( opsLimit, "magLimitAsteroid", i18n( "magnitude of faintest asteroid drawn on map" ), i18n( "double" ) );
	new QListViewItem( opsLimit, "magLimitAsteroidName", i18n( "magnitude of faintest asteroid labeled on map" ), i18n( "double" ) );
	new QListViewItem( opsLimit, "maxRadCometName", i18n( "comets nearer to the Sun than this (in AU) are labeled on map" ), i18n( "double" ) );
	argChangeViewOption->OptionName->insertItem( "magLimitDrawStar" );
	argChangeViewOption->OptionName->insertItem( "magLimitDrawStarInfo" );
	argChangeViewOption->OptionName->insertItem( "magLimitHideStar" );
	argChangeViewOption->OptionName->insertItem( "magLimitAsteroid" );
	argChangeViewOption->OptionName->insertItem( "magLimitAsteroidName" );
	argChangeViewOption->OptionName->insertItem( "maxRadCometName" );
	
}

//Slots defined in ScriptBuilderUI
void ScriptBuilder::slotNew() {
	saveWarning();
	if ( !UnsavedChanges ) {
		ScriptList.clear();
		ScriptListBox->clear();
		ArgStack->raiseWidget( argBlank );
		
		CopyButton->setEnabled( false );
		RemoveButton->setEnabled( false );
		currentFileURL = "";
		currentScriptName = "";
	}
}

void ScriptBuilder::slotOpen() {
	saveWarning();
	
	if ( !UnsavedChanges ) {
		ScriptList.clear();
		ScriptListBox->clear();
		ArgStack->raiseWidget( argBlank );
		
		currentFileURL = KFileDialog::getOpenURL( currentPath, "*.kstars|KStars Scripts (*.kstars)" );
		
		if ( currentFileURL.isValid() && currentFileURL.isLocalFile() ) {
			currentPath = currentFileURL.path();
			
			//Remove the leading 'file:', which confuses QFile :(
			QString fname = currentFileURL.url().mid(5);
			QFile f( fname );
			if ( !f.open( IO_ReadOnly) ) {
				QString message = i18n( "Could not open file %1" ).arg( f.name() );
				KMessageBox::sorry( 0, message, i18n( "Could not Open File" ) );
				currentFileURL = "";
				return;
			} 
			
			QTextStream istream(&f);
			readScript( istream );
			
			f.close();
		} else if ( ! currentFileURL.url().isEmpty() ) {
			QString message = i18n( "Invalid URL: %1" ).arg( currentFileURL.url() );
			KMessageBox::sorry( 0, message, i18n( "Invalid URL" ) );
			currentFileURL = "";
		}
	}
}

void ScriptBuilder::slotSave() {
	if ( currentScriptName.isEmpty() ) {
		//Get Script Name and Author info
		if ( snd->exec() == QDialog::Accepted ) {
			currentScriptName = snd->ScriptName->text();
			currentAuthor = snd->AuthorName->text();
		} else {
			return;
		}
	}
	
	if ( currentFileURL.isEmpty() ) 
		currentFileURL = KFileDialog::getSaveURL( currentPath, "*.kstars|KStars Scripts (*.kstars)" );
	
	if ( currentFileURL.isValid() && currentFileURL.isLocalFile() ) {
		currentPath = currentFileURL.path();
		
		//Remove the leading 'file:', which confuses QFile :(
		//Also, possibly append ".kstars"
		QString fname = currentFileURL.url().mid(5);
		if ( fname.right( 7 ).lower() != ".kstars" ) fname += ".kstars";
		
		QFile f( fname );
		if ( !f.open( IO_WriteOnly) ) {
			QString message = i18n( "Could not open file %1" ).arg( f.name() );
			KMessageBox::sorry( 0, message, i18n( "Could not Open File" ) );
			currentFileURL = "";
			return;
		} 
		
		QTextStream ostream(&f);
		writeScript( ostream );
		f.close();
		
		//set rwx for owner, rx for group, rx for other
		chmod( fname.ascii(), 755 ); 
		
		setUnsavedChanges( false );
		
	} else {
		QString message = i18n( "Invalid URL: %1" ).arg( currentFileURL.url() );
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
		QString caption = i18n( "Save changes to script?" );
		QString message = i18n( "The current script has unsaved changes.  Would you like to save before closing it?" );
		QString ybut = i18n( "&Save" );
		QString nbut = i18n( "&Discard" );
		int ans = KMessageBox::warningYesNoCancel( 0, message, caption, ybut, nbut );
		if ( ans == KMessageBox::Yes ) {
			slotSave();
			setUnsavedChanges( false );
		} else if ( ans == KMessageBox::No ) {
			setUnsavedChanges( false );
		}
		
		//Do nothing if 'cancel' selected
	}
}

void ScriptBuilder::writeScript( QTextStream &ostream ) {
	QString mainpre  = "dcop $KSTARS $MAIN  ";
	QString clockpre = "dcop $KSTARS $CLOCK ";
	
	//Write script header
	ostream << "#!/bin/bash" << endl;
	ostream << "#KStars DCOP script: " << currentScriptName << endl;
	ostream << "#by " << currentAuthor << endl;
	ostream << "#last modified: " << QDateTime::currentDateTime().toString() << endl;
	ostream << "#" << endl;
	ostream << "KSTARS=`dcopfind -a 'kstars*'`" << endl;
	ostream << "MAIN=KStarsInterface" << endl;
	ostream << "CLOCK=clock#1" << endl;
	
	for ( ScriptFunction *sf = ScriptList.first(); sf; sf = ScriptList.next() ) {
		if ( sf->isClockFunction() ) {
			ostream << clockpre << sf->scriptLine() << endl;
		} else {
			ostream << mainpre  << sf->scriptLine() << endl;
		}
	}
	
	//Write script footer
	ostream << "##" << endl;
}

void ScriptBuilder::readScript( QTextStream &istream ) {
	QString line;
	
	while ( ! istream.eof() ) {
		line = istream.readLine();
		
		//look for name of script
		if ( line.contains( "#KStars DCOP script: " ) ) 
			currentScriptName = line.mid( 21 ).stripWhiteSpace();

		//look for author of scriptbuilder
		if ( line.contains( "#by " ) )
			currentAuthor = line.mid( 4 ).stripWhiteSpace();

		//Actual script functions
		if ( line.left(4) == "dcop" ) {
			
		//is ClockFunction?
			bool clockfcn( false );
			if ( line.contains( "$CLOCK" ) ) clockfcn = true;
			 
			//remove leading dcop prefix
			line = line.mid( 20 );
			
			//construct a stringlist that is fcn name and its arg name/value pairs
			QStringList fn = QStringList::split( " ", line );
			if ( parseFunction( fn ) ) ScriptListBox->insertItem( ScriptList.current()->name() );
			else kdWarning() << i18n( "Could not parse script.  Line was: %1" ).arg( line ) << endl; 
		
		} // end if left(4) == "dcop"
	} // end while !eof()
}

bool ScriptBuilder::parseFunction( QStringList &fn ) {
	//loop over known functions to find a name match
	for ( ScriptFunction *sf = FunctionList.first(); sf; sf = FunctionList.next() ) {
		if ( fn[0] == sf->name() ) {
			
			if ( fn[0] == "setGeoLocation" ) {
				QString city( fn[1] ), prov( "" ), cntry( fn[2] );
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
	}

	//if we get here, no function-name match was found
	return false;
}

void ScriptBuilder::setUnsavedChanges( bool b ) {
	UnsavedChanges = b;
	SaveButton->setEnabled( b );
	SaveAsButton->setEnabled( b );
}

void ScriptBuilder::slotEnableScriptNameOK() {
	snd->OKButton->setEnabled( ! snd->ScriptName->text().isEmpty() );
}

void ScriptBuilder::slotCopyFunction() {
	if ( ! UnsavedChanges ) setUnsavedChanges( true );
	
	int Pos = ScriptListBox->currentItem() + 1;
	ScriptList.insert( Pos, new ScriptFunction( ScriptList.at( Pos-1 ) ) );
	//copy ArgVals
	for ( unsigned int i=0; i < ScriptList.at( Pos-1 )->numArgs(); ++i )
		ScriptList.at(Pos)->setArg(i, ScriptList.at( Pos-1 )->argVal(i) );
	
	ScriptListBox->insertItem( ScriptList.current()->name(), Pos );
	ScriptListBox->setSelected( Pos, true );
}

void ScriptBuilder::slotRemoveFunction() {
	if ( ! UnsavedChanges ) setUnsavedChanges( true );
	
	int Pos = ScriptListBox->currentItem();
	ScriptList.remove( Pos );
	ScriptListBox->removeItem( Pos );
	if ( ScriptListBox->count() == 0 ) {
		ArgStack->raiseWidget( argBlank );
		CopyButton->setEnabled( false );
		RemoveButton->setEnabled( false );
	} else {
		ScriptListBox->setSelected( Pos, true );
	}
}

void ScriptBuilder::slotAddFunction() {
	if ( FuncListBox->currentItem() > -1 ) {
		if ( ! UnsavedChanges ) setUnsavedChanges( true );
		
		int Pos = ScriptListBox->currentItem() + 1;
		
		ScriptList.insert( Pos, new ScriptFunction( FunctionList.at( FuncListBox->currentItem() ) ) );
		ScriptListBox->insertItem( ScriptList.current()->name(), Pos );
		ScriptListBox->setSelected( Pos, true );
	}
}

void ScriptBuilder::slotMoveFunctionUp() {
	if ( ScriptListBox->currentItem() > 0 ) {
		if ( ! UnsavedChanges ) setUnsavedChanges( true );
		
		QString t = ScriptListBox->currentText();
		unsigned int n = ScriptListBox->currentItem();
		
		ScriptFunction *tmp = ScriptList.take( n );
		ScriptList.insert( n-1, tmp );
		
		ScriptListBox->removeItem( n );
		ScriptListBox->insertItem( t, n-1 );
		ScriptListBox->setSelected( n-1, true );
	}
}

void ScriptBuilder::slotMoveFunctionDown() {
	if ( ScriptListBox->currentItem() > -1 && 
				ScriptListBox->currentItem() < ScriptListBox->count()-1 ) {
		if ( ! UnsavedChanges ) setUnsavedChanges( true );
		
		QString t = ScriptListBox->currentText();
		unsigned int n = ScriptListBox->currentItem();
		
		ScriptFunction *tmp = ScriptList.take( n );
		ScriptList.insert( n+1, tmp );
		
		ScriptListBox->removeItem( n );
		ScriptListBox->insertItem( t, n+1 );
		ScriptListBox->setSelected( n+1, true );
	}
}

void ScriptBuilder::slotArgWidget() {
	//First, setEnabled on buttons that act on the selected script function
	if ( ScriptListBox->currentItem() == -1 ) { //no selection
		CopyButton->setEnabled( false );
		RemoveButton->setEnabled( false );
		UpButton->setEnabled( false );
		DownButton->setEnabled( false );
	} else if ( ScriptListBox->count() == 1 ) { //only one item, so disable up/down buttons
		CopyButton->setEnabled( true );
		RemoveButton->setEnabled( true );
		UpButton->setEnabled( false );
		DownButton->setEnabled( false );
	} else if ( ScriptListBox->currentItem() == 0 ) { //first item selected
		CopyButton->setEnabled( true );
		RemoveButton->setEnabled( true );
		UpButton->setEnabled( false );
		DownButton->setEnabled( true );
	} else if ( ScriptListBox->currentItem() == ScriptListBox->count()-1 ) { //last item selected
		CopyButton->setEnabled( true );
		RemoveButton->setEnabled( true );
		UpButton->setEnabled( true );
		DownButton->setEnabled( false );
	} else { //other item selected
		CopyButton->setEnabled( true );
		RemoveButton->setEnabled( true );
		UpButton->setEnabled( true );
		DownButton->setEnabled( true );
	}
	
	//Display the function's arguments widget
	if ( ScriptListBox->currentItem() > -1 && 
				ScriptListBox->currentItem() < ScriptListBox->count() ) {
		QString t = ScriptListBox->currentText();
		unsigned int n = ScriptListBox->currentItem();
		ScriptFunction *sf = ScriptList.at( n );

		if ( sf->name() == "lookTowards" ) {
			ArgStack->raiseWidget( argLookToward );
			QString s = sf->argVal(0);
			argLookToward->FocusEdit->setCurrentText( s );
			
		} else if ( sf->name() == "setRaDec" ) {
			bool ok(false);
			double r(0.0),d(0.0);
			dms ra(0.0);
			
			ArgStack->raiseWidget( argSetRaDec );
			argSetRaDec->RaBox->clear();
			argSetRaDec->DecBox->clear();
			
			ok = !sf->argVal(0).isEmpty();
			if (ok) r = sf->argVal(0).toDouble(&ok);
			if (ok) { ra.setH(r); argSetRaDec->RaBox->showInHours( ra ); }
			
			ok = !sf->argVal(1).isEmpty();
			if (ok) d = sf->argVal(1).toDouble(&ok);
			if (ok) argSetRaDec->DecBox->showInDegrees( dms(d) ); 
		
		} else if ( sf->name() == "setAltAz" ) {
			bool ok(false);
			double x(0.0),y(0.0);
			
			ArgStack->raiseWidget( argSetAltAz );
			argSetAltAz->AzBox->clear();
			argSetAltAz->AltBox->clear();
			
			ok = !sf->argVal(0).isEmpty();
			if (ok) y = sf->argVal(0).toDouble(&ok);
			if (ok) argSetAltAz->AltBox->showInDegrees( dms(y) ); 
			
			ok = !sf->argVal(1).isEmpty();
			x = sf->argVal(1).toDouble(&ok);
			if (ok) argSetAltAz->AzBox->showInDegrees( dms(x) ); 
			
		} else if ( sf->name() == "zoomIn" ) {
			ArgStack->raiseWidget( argBlank );
			//no Args
			
		} else if ( sf->name() == "zoomOut" ) {
			ArgStack->raiseWidget( argBlank );
			//no Args
			
		} else if ( sf->name() == "defaultZoom" ) {
			ArgStack->raiseWidget( argBlank );
			//no Args
			
		} else if ( sf->name() == "setLocalTime" ) {
			ArgStack->raiseWidget( argSetLocalTime );
			bool ok(false);
			int year, month, day, hour, min, sec;
			
			year = sf->argVal(0).toInt(&ok);
			if (ok) month = sf->argVal(1).toInt(&ok);
			if (ok) day   = sf->argVal(2).toInt(&ok);
			if (ok) argSetLocalTime->DateBox->setDate( QDate( year, month, day ) );
			else argSetLocalTime->DateBox->setDate( QDate::currentDate() );
			
			hour = sf->argVal(3).toInt(&ok);
			if ( sf->argVal(3).isEmpty() ) ok = false;
			if (ok) min = sf->argVal(4).toInt(&ok);
			if (ok) sec = sf->argVal(5).toInt(&ok);
			if (ok) argSetLocalTime->TimeBox->setTime( QTime( hour, min, sec ) );
			else argSetLocalTime->TimeBox->setTime( QTime( QTime::currentTime() ) );
			
		} else if ( sf->name() == "waitFor" ) {
			ArgStack->raiseWidget( argWaitFor );
			bool ok(false);
			int sec = sf->argVal(0).toInt(&ok);
			if (ok) argWaitFor->DelayBox->setValue( sec );
			else argWaitFor->DelayBox->setValue( 0 );
			
		} else if ( sf->name() == "waitForKey" ) {
			ArgStack->raiseWidget( argWaitForKey );
			if ( sf->argVal(0).length()==1 || sf->argVal(0).lower() == "space" )
				argWaitForKey->WaitKeyEdit->setText( sf->argVal(0) );
			else argWaitForKey->WaitKeyEdit->setText( "" );
			
		} else if ( sf->name() == "setTracking" ) {
			ArgStack->raiseWidget( argSetTracking );
			if ( sf->argVal(0) == "true"  ) argSetTracking->CheckTrack->setChecked( true  );
			else argSetTracking->CheckTrack->setChecked( false );
		
		} else if ( sf->name() == "changeViewOption" ) {
			ArgStack->raiseWidget( argChangeViewOption );
			//find argVal(0) in the combobox...if it isn't there, it will select nothing
			argChangeViewOption->OptionName->setCurrentItem( sf->argVal(0) );
			argChangeViewOption->OptionValue->setText( sf->argVal(1) );
			
		} else if ( sf->name() == "setGeoLocation" ) {
			ArgStack->raiseWidget( argSetGeoLocation );
			argSetGeoLocation->CityName->setText( sf->argVal(0) );
			argSetGeoLocation->ProvinceName->setText( sf->argVal(1) );
			argSetGeoLocation->CountryName->setText( sf->argVal(2) );
			
		} else if ( sf->name() == "stop" ) {
			ArgStack->raiseWidget( argBlank );
			//no Args
			
		} else if ( sf->name() == "start" ) {
			ArgStack->raiseWidget( argBlank );
			//no Args
			
		} else if ( sf->name() == "setClockScale" ) {
			ArgStack->raiseWidget( argTimeScale );
			bool ok(false);
			double ts = sf->argVal(0).toDouble(&ok);
			if (ok) argTimeScale->TimeScale->tsbox()->changeScale( float(ts) );
			else argTimeScale->TimeScale->tsbox()->changeScale( 0.0 );
			
		}
	}
}

void ScriptBuilder::slotShowDoc() {
	int n = FuncListBox->currentItem();
	
	if ( n >= 0 && n < FunctionList.count() ) {
		AddButton->setEnabled( true );
		FuncDoc->setText( FunctionList.at( n )->description() );
	} else {
		AddButton->setEnabled( false );
		kdWarning() << i18n( "Function index out of bounds." ) << endl;
	}
}

void ScriptBuilder::closeEvent( QCloseEvent *e ) {
	//warn user if there are unsaved changes before closing.
	saveWarning();
	
	//if user selects 'Save' or 'Discard', UnsavedChanges is set false
	if ( !UnsavedChanges ) e->accept(); 
}

//Slots for Arg Widgets
void ScriptBuilder::slotFindCity() {
	LocationDialog ld( ks );
	
	if ( ld.exec() == QDialog::Accepted ) {
		int ii = ld.getCityIndex();
		if ( ii >= 0 ) {
			// set new location names
			argSetGeoLocation->CityName->setText( ld.selectedCity() );
			argSetGeoLocation->ProvinceName->setText( ld.selectedProvince() );
			argSetGeoLocation->CountryName->setText( ld.selectedCountry() );
			
			ScriptFunction *sf = ScriptList.at( ScriptListBox->currentItem() );
			if ( sf->name() == "setGeoLocation" ) {
				if ( ! UnsavedChanges ) setUnsavedChanges( true );
				
				sf->setArg( 0, ld.selectedCity() );
				sf->setArg( 1, ld.selectedProvince() );
				sf->setArg( 2, ld.selectedCountry() );
			} else {
				kdWarning() << i18n( "Mismatch between function and Arg widget! (expected %1)" ).arg( "setGeoLocation" ) << endl;
			}
		}
	}
}

void ScriptBuilder::slotFindObject() {
	FindDialog fd( ks );
	
	if ( fd.exec() == QDialog::Accepted && fd.currentItem() ) {
		if ( ! UnsavedChanges ) setUnsavedChanges( true );
		
		argLookToward->FocusEdit->setCurrentText( fd.currentItem()->objName()->translatedText() );
	}
}

void ScriptBuilder::slotShowOptions() {
	//Show tree-view of view options
	if ( otv->exec() == QDialog::Accepted ) {
		argChangeViewOption->OptionName->setCurrentItem( otv->OptionsList->currentItem()->text(0) );
	}
}

void ScriptBuilder::slotLookToward() {
	ScriptFunction *sf = ScriptList.at( ScriptListBox->currentItem() );
	
	if ( sf->name() == "lookTowards" ) {
		if ( ! UnsavedChanges ) setUnsavedChanges( true );
		
		sf->setArg( 0, argLookToward->FocusEdit->currentText() );
		sf->setValid(true);
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget! (expected %1)" ).arg( "lookTowards" ) << endl;
	}
}

void ScriptBuilder::slotRa() {
	ScriptFunction *sf = ScriptList.at( ScriptListBox->currentItem() );
	
	if ( sf->name() == "setRaDec" ) {
		//do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
		if ( argSetRaDec->RaBox->text().isEmpty() ) return;
		
		bool ok(false);
		dms ra = argSetRaDec->RaBox->createDms(false, &ok);
		if ( ok ) {
			if ( ! UnsavedChanges ) setUnsavedChanges( true );
			
			sf->setArg( 0, QString( "%1" ).arg( ra.Hours() ) );
			if ( ! sf->argVal(1).isEmpty() ) sf->setValid( true );
			
		} else {
			sf->setArg( 0, "" );
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget! (expected %1)" ).arg( "setRaDec" ) << endl;
	}
}

void ScriptBuilder::slotDec() {
	ScriptFunction *sf = ScriptList.at( ScriptListBox->currentItem() );
	
	if ( sf->name() == "setRaDec" ) {
		//do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
		if ( argSetRaDec->DecBox->text().isEmpty() ) return;
		
		bool ok(false);
		dms dec = argSetRaDec->DecBox->createDms(true, &ok); 
		if ( ok ) {
			if ( ! UnsavedChanges ) setUnsavedChanges( true );
			
			sf->setArg( 1, QString( "%1" ).arg( dec.Degrees() ) );
			if ( ! sf->argVal(0).isEmpty() ) sf->setValid( true );
			
		} else {
			sf->setArg( 1, "" );
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget! (expected %1)" ).arg( "setRaDec" ) << endl;
	}
}

void ScriptBuilder::slotAz() {
	ScriptFunction *sf = ScriptList.at( ScriptListBox->currentItem() );
	
	if ( sf->name() == "setAltAz" ) {
		//do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
		if ( argSetAltAz->AzBox->text().isEmpty() ) return;
		
		bool ok(false);
		dms az = argSetAltAz->AzBox->createDms(true, &ok); 
		if ( ok ) {
			if ( ! UnsavedChanges ) setUnsavedChanges( true );
			sf->setArg( 1, QString( "%1" ).arg( az.Degrees() ) );
			if ( ! sf->argVal(0).isEmpty() ) sf->setValid( true );
		} else {
			sf->setArg( 1, "" );
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget! (expected %1)" ).arg( "setAltAz" ) << endl;
	}
}

void ScriptBuilder::slotAlt() {
	ScriptFunction *sf = ScriptList.at( ScriptListBox->currentItem() );
	
	if ( sf->name() == "setAltAz" ) {
		//do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
		if ( argSetAltAz->AltBox->text().isEmpty() ) return;
		
		bool ok(false);
		dms alt = argSetAltAz->AltBox->createDms(true, &ok);
		if ( ok ) {
			if ( ! UnsavedChanges ) setUnsavedChanges( true );
			
			sf->setArg( 0, QString( "%1" ).arg( alt.Degrees() ) );
			if ( ! sf->argVal(1).isEmpty() ) sf->setValid( true );
		} else {
			sf->setArg( 0, "" );
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget! (expected %1)" ).arg( "setAltAz" ) << endl;
	}
}

void ScriptBuilder::slotChangeDate() {
	ScriptFunction *sf = ScriptList.at( ScriptListBox->currentItem() );
	
	if ( sf->name() == "setLocalTime" ) {
		if ( ! UnsavedChanges ) setUnsavedChanges( true );
		
		QDate date = argSetLocalTime->DateBox->date();
		
		sf->setArg( 0, QString( "%1" ).arg( date.year()   ) );  
		sf->setArg( 1, QString( "%1" ).arg( date.month()  ) );  
		sf->setArg( 2, QString( "%1" ).arg( date.day()    ) );
		if ( ! sf->argVal(3).isEmpty() ) sf->setValid( true );
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget! (expected %1)" ).arg( "setLocalTime" ) << endl;
	}
}

void ScriptBuilder::slotChangeTime() {
	ScriptFunction *sf = ScriptList.at( ScriptListBox->currentItem() );
	
	if ( sf->name() == "setLocalTime" ) {
		if ( ! UnsavedChanges ) setUnsavedChanges( true );
		
		QTime time = argSetLocalTime->TimeBox->time();
		
		sf->setArg( 3, QString( "%1" ).arg( time.hour()   ) );  
		sf->setArg( 4, QString( "%1" ).arg( time.minute() ) );  
		sf->setArg( 5, QString( "%1" ).arg( time.second() ) );
		if ( ! sf->argVal(0).isEmpty() ) sf->setValid( true );
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget! (expected %1)" ).arg( "setLocalTime" ) << endl;
	}
}

void ScriptBuilder::slotWaitFor() {
	ScriptFunction *sf = ScriptList.at( ScriptListBox->currentItem() );
	
	if ( sf->name() == "waitFor" ) {
		bool ok(false);
		int delay = argWaitFor->DelayBox->text().toInt( &ok );
		
		if ( ok ) {
			if ( ! UnsavedChanges ) setUnsavedChanges( true );
			
			sf->setArg( 0, QString( "%1" ).arg( delay ) ); 
			sf->setValid( true );
		} else {
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget! (expected %1)" ).arg( "waitFor" ) << endl;
	}
}

void ScriptBuilder::slotWaitForKey() {
	ScriptFunction *sf = ScriptList.at( ScriptListBox->currentItem() );
	
	if ( sf->name() == "waitForKey" ) {
		QString sKey = argWaitForKey->WaitKeyEdit->text().stripWhiteSpace();
		
		//DCOP script can only use single keystrokes; make sure entry is either one character,
		//or the word 'space'
		if ( sKey.length() == 1 || sKey == "space" ) {
			if ( ! UnsavedChanges ) setUnsavedChanges( true );
			
			sf->setArg( 0, sKey ); 
			sf->setValid( true );
		} else {
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget! (expected %1)" ).arg( "waitForKey" ) << endl;
	}
}

void ScriptBuilder::slotTracking() {
	ScriptFunction *sf = ScriptList.at( ScriptListBox->currentItem() );
	
	if ( sf->name() == "setTracking" ) {
		if ( ! UnsavedChanges ) setUnsavedChanges( true );
		
		if ( argSetTracking->CheckTrack->isChecked() ) {
			sf->setArg( 0, "true" );
		} else {
			sf->setArg( 0, "false" );
		}
		sf->setValid( true );
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget! (expected %1)" ).arg( "setTracking" ) << endl;
	}
}

void ScriptBuilder::slotViewOption() {
	ScriptFunction *sf = ScriptList.at( ScriptListBox->currentItem() );
	
	if ( sf->name() == "changeViewOption" ) {
		if ( argChangeViewOption->OptionName->currentItem() >= 0 
				&& argChangeViewOption->OptionValue->text().length() ) {
			if ( ! UnsavedChanges ) setUnsavedChanges( true );
			
			sf->setArg( 0, argChangeViewOption->OptionName->currentText() );
			sf->setArg( 1, argChangeViewOption->OptionValue->text() );
			sf->setValid( true );
		} else {
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget! (expected %1)" ).arg( "changeViewOption" ) << endl;
	}
}

void ScriptBuilder::slotChangeCity() {
	ScriptFunction *sf = ScriptList.at( ScriptListBox->currentItem() );
	
	if ( sf->name() == "setGeoLocation" ) {
		QString city =     argSetGeoLocation->CityName->text();
		
		if ( city.length() ) {
			if ( ! UnsavedChanges ) setUnsavedChanges( true );
			
			sf->setArg( 0, city );
			if ( sf->argVal(2).length() ) sf->setValid( true );
		} else {
			sf->setArg( 0, "" );
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget! (expected %1)" ).arg( "setGeoLocation" ) << endl;
	}
}

void ScriptBuilder::slotChangeProvince() {
	ScriptFunction *sf = ScriptList.at( ScriptListBox->currentItem() );
	
	if ( sf->name() == "setGeoLocation" ) {
		QString province = argSetGeoLocation->ProvinceName->text();
		
		if ( province.length() ) {
			if ( ! UnsavedChanges ) setUnsavedChanges( true );
			
			sf->setArg( 1, province );
			if ( sf->argVal(0).length() && sf->argVal(2).length() ) sf->setValid( true );
		} else {
			sf->setArg( 1, "" );
			//might not be invalid
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget! (expected %1)" ).arg( "setGeoLocation" ) << endl;
	}
}

void ScriptBuilder::slotChangeCountry() {
	ScriptFunction *sf = ScriptList.at( ScriptListBox->currentItem() );
	
	if ( sf->name() == "setGeoLocation" ) {
		QString country =  argSetGeoLocation->CountryName->text();
		
		if ( country.length() ) {
			if ( ! UnsavedChanges ) setUnsavedChanges( true );
			
			sf->setArg( 2, country );
			if ( sf->argVal(0).length() ) sf->setValid( true );
		} else {
			sf->setArg( 2, "" );
			sf->setValid( false );
		}
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget! (expected %1)" ).arg( "setGeoLocation" ) << endl;
	}
}

void ScriptBuilder::slotTimeScale() {
	ScriptFunction *sf = ScriptList.at( ScriptListBox->currentItem() );
	
	if ( sf->name() == "setClockScale" ) {
		if ( ! UnsavedChanges ) setUnsavedChanges( true );
		
		sf->setArg( 0, QString( "%1" ).arg( argTimeScale->TimeScale->tsbox()->timeScale() ) );
		sf->setValid( true );
	} else {
		kdWarning() << i18n( "Mismatch between function and Arg widget! (expected %1)" ).arg( "setClockScale" ) << endl;
	}
}
