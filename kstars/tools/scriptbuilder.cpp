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

#include "scriptbuilder.h"

//needed in slotSave() for chmod() syscall
#include <sys/stat.h>

#include <QApplication>
#include <QFontMetrics>
#include <QTreeWidget>
#include <QTextStream>

#include <QDebug>
#include <KLocalizedString>
#include <kprocess.h>
#include <kstandardguiitem.h>

#include <kmessagebox.h>
#include <QFileDialog>
#include <QStandardPaths>

#include "scriptfunction.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "kstarsdatetime.h"
#include "dialogs/finddialog.h"
#include "dialogs/locationdialog.h"
#include "widgets/dmsbox.h"
#include "widgets/timestepbox.h"

OptionsTreeViewWidget::OptionsTreeViewWidget( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

OptionsTreeView::OptionsTreeView( QWidget *p )
        : QDialog( p )
{
    otvw = new OptionsTreeViewWidget( this );

    QVBoxLayout *mainLayout = new QVBoxLayout;

    mainLayout->addWidget(otvw);
    setLayout(mainLayout);

    //setMainWidget( otvw );
    setWindowTitle( xi18n( "Options" ) );
    // FIXME needs porting to KF5
    //setButtons( QDialog::Ok|QDialog::Cancel );
    setModal( false );
}

OptionsTreeView::~OptionsTreeView() {
    delete otvw;
}

void OptionsTreeView::resizeColumns() {
    //Size each column to the maximum width of items in that column
    int maxwidth[ 3 ] = { 0, 0, 0 };
    QFontMetrics qfm = optionsList()->fontMetrics();

    for ( int i=0; i < optionsList()->topLevelItemCount(); ++i ) {
        QTreeWidgetItem *topitem = optionsList()->topLevelItem( i );
        topitem->setExpanded( true );

        for ( int j=0; j < topitem->childCount(); ++j ) {
            QTreeWidgetItem *child = topitem->child( j );

            for ( int icol=0; icol<3; ++icol ) {
                child->setExpanded( true );

                int w = qfm.width( child->text( icol ) ) + 4;
                if ( icol == 0 ) {
                    w += 2*optionsList()->indentation();
                }

                if ( w > maxwidth[icol] ) {
                    maxwidth[icol] = w;
                }
            }
        }
    }

    for ( int icol=0; icol < 3; ++icol ) {
        //DEBUG
        qDebug() << QString("max width of column %1: %2").arg(icol).arg(maxwidth[icol]) << endl;

        optionsList()->setColumnWidth( icol, maxwidth[icol] );
    }
}

ScriptNameWidget::ScriptNameWidget( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ScriptNameDialog::ScriptNameDialog( QWidget *p )
        : QDialog( p )
{
    snw = new ScriptNameWidget( this );

    QVBoxLayout *mainLayout = new QVBoxLayout;

    mainLayout->addWidget(snw);
    setLayout(mainLayout);

    setWindowTitle( xi18n( "Script Data" ) );

    //FIXME needs porting to KF5
    //setMainWidget( snw );
    //setButtons( QDialog::Ok|QDialog::Cancel );

    connect( snw->ScriptName, SIGNAL( textChanged(const QString &) ), this, SLOT( slotEnableOkButton() ) );
}

ScriptNameDialog::~ScriptNameDialog() {
    delete snw;
}

void ScriptNameDialog::slotEnableOkButton()
{
    //FIXME Needs porting to KF5
    //enableButtonOk( ! snw->ScriptName->text().isEmpty() );
}

ScriptBuilderUI::ScriptBuilderUI( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

ScriptBuilder::ScriptBuilder( QWidget *parent )
        : QDialog( parent ), UnsavedChanges(false), checkForChanges(true),
        currentFileURL(), currentDir( QDir::homePath() ),
        currentScriptName(), currentAuthor()
{
    ks = (KStars*)parent;
    sb = new ScriptBuilderUI(this);

    QVBoxLayout *mainLayout = new QVBoxLayout;

    mainLayout->addWidget(sb);
    setLayout(mainLayout);

    //setMainWidget(sb);
    setWindowTitle( xi18n( "Script Builder" ) );

    //FIXME Needs porting to KF5
    //setButtons( QDialog::User1 );
    //setButtonGuiItem( QDialog::User1, KGuiItem( xi18n("&Close"), "dialog-close", xi18n("Close the dialog") ) );

    sb->FuncDoc->setTextInteractionFlags( Qt::NoTextInteraction );

    //Initialize function templates and descriptions
    KStarsFunctionList.append( new ScriptFunction( "lookTowards", xi18n( "Point the display at the specified location. %1 can be the name of an object, a cardinal point on the compass, or 'zenith'.", QString( "dir" ) ), false, "QString", "dir" ) );
    KStarsFunctionList.append( new ScriptFunction( "addLabel", xi18n( "Add a name label to the object named %1.", QString( "name" ) ), false, "QString", "name" ) );
    KStarsFunctionList.append( new ScriptFunction( "removeLabel", xi18n( "Remove the name label from the object named %1.", QString( "name" ) ), false, "QString", "name" ) );
    KStarsFunctionList.append( new ScriptFunction( "addTrail", xi18n( "Add a trail to the solar system body named %1.", QString( "name" ) ), false, "QString", "name" ) );
    KStarsFunctionList.append( new ScriptFunction( "removeTrail", xi18n( "Remove the trail from the solar system body named %1.", QString( "name" ) ), false, "QString", "name" ) );
    KStarsFunctionList.append( new ScriptFunction( "setRaDec", xi18n( "Point the display at the specified RA/Dec coordinates.  RA is expressed in Hours; Dec is expressed in Degrees." ),
                               false, "double", "ra", "double", "dec" ) );
    KStarsFunctionList.append( new ScriptFunction( "setAltAz", xi18n( "Point the display at the specified Alt/Az coordinates.  Alt and Az are expressed in Degrees." ),
                               false, "double", "alt", "double", "az" ) );
    KStarsFunctionList.append( new ScriptFunction( "zoomIn", xi18n( "Increase the display Zoom Level." ), false ) );
    KStarsFunctionList.append( new ScriptFunction( "zoomOut", xi18n( "Decrease the display Zoom Level." ), false ) );
    KStarsFunctionList.append( new ScriptFunction( "defaultZoom", xi18n( "Set the display Zoom Level to its default value." ), false ) );
    KStarsFunctionList.append( new ScriptFunction( "zoom", xi18n( "Set the display Zoom Level manually." ), false, "double", "z" ) );
    KStarsFunctionList.append( new ScriptFunction( "setLocalTime", xi18n( "Set the system clock to the specified Local Time." ),
                               false, "int", "year", "int", "month", "int", "day", "int", "hour", "int", "minute", "int", "second" ) );
    KStarsFunctionList.append( new ScriptFunction( "waitFor", xi18n( "Pause script execution for specified number of seconds." ), false, "double", "sec" ) );
    KStarsFunctionList.append( new ScriptFunction( "waitForKey", xi18n( "Halt script execution until the specified key is pressed.  Only single-key strokes are possible; use 'space' for the spacebar." ),
                               false, "QString", "key" ) );
    KStarsFunctionList.append( new ScriptFunction( "setTracking", xi18n( "Set whether the display is tracking the current location." ), false, "bool", "track" ) );
    KStarsFunctionList.append( new ScriptFunction( "changeViewOption", xi18n( "Change view option named %1 to value %2.", QString( "opName" ), QString( "opValue" ) ), false, "QString", "opName", "QString", "opValue" ) );
    KStarsFunctionList.append( new ScriptFunction( "setGeoLocation", xi18n( "Set the geographic location to the city specified by city, province and country." ),
                               false, "QString", "cityName", "QString", "provinceName", "QString", "countryName" ) );
    KStarsFunctionList.append( new ScriptFunction( "setColor", xi18n( "Set the color named %1 to the value %2.", QString("colorName"), QString("value") ), false, "QString", "colorName", "QString", "value" ) );
    KStarsFunctionList.append( new ScriptFunction( "loadColorScheme", xi18n( "Load the color scheme specified by name." ), false, "QString", "name" ) );
    KStarsFunctionList.append( new ScriptFunction( "exportImage", xi18n( "Export the sky image to the file, with specified width and height."), false, "QString", "fileName", "int", "width", "int", "height" ) );
    KStarsFunctionList.append( new ScriptFunction( "printImage", xi18n( "Print the sky image to a printer or file.  If %1 is true, it will show the print dialog.  If %2 is true, it will use the Star Chart color scheme for printing.", QString( "usePrintDialog" ), QString( "useChartColors" ) ), false, "bool", "usePrintDialog", "bool", "useChartColors" ) );
    SimClockFunctionList.append( new ScriptFunction( "stop", xi18n( "Halt the simulation clock." ), true ) );
    SimClockFunctionList.append( new ScriptFunction( "start", xi18n( "Start the simulation clock." ), true ) );
    SimClockFunctionList.append( new ScriptFunction( "setClockScale", xi18n( "Set the timescale of the simulation clock to specified scale.  1.0 means real-time; 2.0 means twice real-time; etc." ), true, "double", "scale" ) );

    //TODO JM: INDI Scripting to be supported in KDE 4.1
    // INDI functions
	#if 0
    ScriptFunction *startINDIFunc(NULL), *shutdownINDIFunc(NULL), *switchINDIFunc(NULL), *setINDIPortFunc(NULL), *setINDIScopeActionFunc(NULL), *setINDITargetCoordFunc(NULL), *setINDITargetNameFunc(NULL), *setINDIGeoLocationFunc(NULL), *setINDIUTCFunc(NULL), *setINDIActionFunc(NULL), *waitForINDIActionFunc(NULL), *setINDIFocusSpeedFunc(NULL), *startINDIFocusFunc(NULL), *setINDIFocusTimeoutFunc(NULL), *setINDICCDTempFunc(NULL), *setINDIFilterNumFunc(NULL), *setINDIFrameTypeFunc(NULL), *startINDIExposureFunc(NULL), *setINDIDeviceFunc(NULL);

    startINDIFunc = new ScriptFunction( "startINDI", xi18n("Establish an INDI device either in local mode or server mode."), false, "QString", "deviceName", "bool", "useLocal");
    INDIFunctionList.append ( startINDIFunc );

    setINDIDeviceFunc = new ScriptFunction( "setINDIDevice", xi18n("Change current active device. All subsequent function calls will communicate with this device until changed"), false, "QString", "deviceName");
    INDIFunctionList.append(setINDIDeviceFunc);

    shutdownINDIFunc = new ScriptFunction( "shutdownINDI", xi18n("Shutdown an INDI device."), false, "QString", "deviceName");
    INDIFunctionList.append ( shutdownINDIFunc);

    switchINDIFunc = new ScriptFunction( "switchINDI", xi18n("Connect or Disconnect an INDI device."), false, "bool", "turnOn");
    switchINDIFunc->setINDIProperty("CONNECTION");
    switchINDIFunc->setArg(0, "true");
    INDIFunctionList.append ( switchINDIFunc);

    setINDIPortFunc = new ScriptFunction( "setINDIPort", xi18n("Set INDI's device connection port."), false, "QString", "port");
    setINDIPortFunc->setINDIProperty("DEVICE_PORT");
    INDIFunctionList.append ( setINDIPortFunc);

    setINDIScopeActionFunc = new ScriptFunction( "setINDIScopeAction", xi18n("Set the telescope action. Available actions are SLEW, TRACK, SYNC, PARK, and ABORT."), false, "QString", "action");
    setINDIScopeActionFunc->setINDIProperty("CHECK");
    setINDIScopeActionFunc->setArg(0, "SLEW");
    INDIFunctionList.append( setINDIScopeActionFunc);

    setINDITargetCoordFunc = new ScriptFunction ( "setINDITargetCoord", xi18n( "Set the telescope target coordinates to the RA/Dec coordinates.  RA is expressed in Hours; DEC is expressed in Degrees." ), false, "double", "RA", "double", "DEC" );
    setINDITargetCoordFunc->setINDIProperty("EQUATORIAL_EOD_COORD");
    INDIFunctionList.append ( setINDITargetCoordFunc );

    setINDITargetNameFunc = new ScriptFunction( "setINDITargetName", xi18n("Set the telescope target coordinates to the RA/Dec coordinates of the selected object."), false, "QString", "targetName");
    setINDITargetNameFunc->setINDIProperty("EQUATORIAL_EOD_COORD");
    INDIFunctionList.append( setINDITargetNameFunc);

    setINDIGeoLocationFunc = new ScriptFunction ( "setINDIGeoLocation", xi18n("Set the telescope longitude and latitude. The longitude is measured east from Greenwich, UK."), false, "double", "long", "double", "lat");
    setINDIGeoLocationFunc->setINDIProperty("GEOGRAPHIC_COORD");
    INDIFunctionList.append( setINDIGeoLocationFunc);

    setINDIUTCFunc = new ScriptFunction ( "setINDIUTC", xi18n("Set the device UTC time in ISO 8601 format YYYY/MM/DDTHH:MM:SS."), false, "QString", "UTCDateTime");
    setINDIUTCFunc->setINDIProperty("TIME_UTC");
    INDIFunctionList.append( setINDIUTCFunc);

    setINDIActionFunc = new ScriptFunction( "setINDIAction", xi18n("Activate an INDI action. The action is the name of any INDI switch property element supported by the device."), false, "QString", "actionName");
    INDIFunctionList.append( setINDIActionFunc);

    waitForINDIActionFunc = new ScriptFunction ("waitForINDIAction", xi18n("Pause script execution until action returns with OK status. The action can be the name of any INDI property supported by the device."), false, "QString", "actionName");
    INDIFunctionList.append( waitForINDIActionFunc );

    setINDIFocusSpeedFunc = new ScriptFunction ("setINDIFocusSpeed", xi18n("Set the telescope focuser speed. Set speed to 0 to halt the focuser. 1-3 correspond to slow, medium, and fast speeds respectively."), false, "unsigned int", "speed");
    setINDIFocusSpeedFunc->setINDIProperty("FOCUS_SPEED");
    INDIFunctionList.append( setINDIFocusSpeedFunc );

    startINDIFocusFunc = new ScriptFunction ("startINDIFocus", xi18n("Start moving the focuser in the direction Dir, and for the duration specified by setINDIFocusTimeout."), false, "QString", "Dir");
    startINDIFocusFunc->setINDIProperty("FOCUS_MOTION");
    startINDIFocusFunc->setArg(0, "IN");
    INDIFunctionList.append( startINDIFocusFunc);

    setINDIFocusTimeoutFunc = new ScriptFunction ( "setINDIFocusTimeout", xi18n("Set the telescope focuser timer in seconds. This is the duration of any focusing procedure performed by calling startINDIFocus."), false, "int", "timeout");
    setINDIFocusTimeoutFunc->setINDIProperty("FOCUS_TIMER");
    INDIFunctionList.append( setINDIFocusTimeoutFunc);

    setINDICCDTempFunc = new ScriptFunction( "setINDICCDTemp", xi18n("Set the target CCD chip temperature."), false, "int", "temp");
    setINDICCDTempFunc->setINDIProperty("CCD_TEMPERATURE");
    INDIFunctionList.append( setINDICCDTempFunc);

    setINDIFilterNumFunc = new ScriptFunction( "setINDIFilterNum", xi18n("Set the target filter position."), false, "int", "filter_num");
    setINDIFilterNumFunc->setINDIProperty("FILTER_SLOT");
    INDIFunctionList.append ( setINDIFilterNumFunc);

    setINDIFrameTypeFunc = new ScriptFunction( "setINDIFrameType", xi18n("Set the CCD camera frame type. Available options are FRAME_LIGHT, FRAME_BIAS, FRAME_DARK, and FRAME_FLAT."), false, "QString", "type");
    setINDIFrameTypeFunc->setINDIProperty("FRAME_TYPE");
    setINDIFrameTypeFunc->setArg(0, "FRAME_LIGHT");
    INDIFunctionList.append( setINDIFrameTypeFunc);

    startINDIExposureFunc = new ScriptFunction ( "startINDIExposure", xi18n("Start Camera/CCD exposure. The duration is in seconds."), false, "int", "timeout");
    startINDIExposureFunc->setINDIProperty("CCD_EXPOSE_DURATION");
    INDIFunctionList.append( startINDIExposureFunc);
	#endif

    // JM: We're using QTreeWdiget for Qt4 now
    QTreeWidgetItem *kstars_tree = new QTreeWidgetItem( sb->FunctionTree, QStringList("KStars"));
    QTreeWidgetItem *simclock_tree = new QTreeWidgetItem( sb->FunctionTree, QStringList("SimClock"));

    for ( int i=0; i < KStarsFunctionList.size(); ++i )
        new QTreeWidgetItem (kstars_tree, QStringList( KStarsFunctionList[i]->prototype()) );

    for ( int i=0; i < SimClockFunctionList.size(); ++i )
        new QTreeWidgetItem (simclock_tree, QStringList( SimClockFunctionList[i]->prototype()) );

    kstars_tree->sortChildren( 0, Qt::AscendingOrder );
    simclock_tree->sortChildren( 0, Qt::AscendingOrder );

    sb->FunctionTree->setColumnCount(1);
    sb->FunctionTree->setHeaderLabels( QStringList(xi18n("Functions")) );
    sb->FunctionTree->setSortingEnabled( false );

	#if 0
    QTreeWidgetItem *INDI_tree = new QTreeWidgetItem( sb->FunctionTree, QStringList("INDI"));
    QTreeWidgetItem *INDI_general = new QTreeWidgetItem( INDI_tree, QStringList("General"));
    QTreeWidgetItem *INDI_telescope = new QTreeWidgetItem( INDI_tree, QStringList("Telescope"));
    QTreeWidgetItem *INDI_ccd = new QTreeWidgetItem( INDI_tree, QStringList("Camera/CCD"));
    QTreeWidgetItem *INDI_focuser = new QTreeWidgetItem( INDI_tree, QStringList("Focuser"));
    QTreeWidgetItem *INDI_filter = new QTreeWidgetItem( INDI_tree, QStringList("Filter"));

    // General
    new QTreeWidgetItem(INDI_general, QStringList(startINDIFunc->prototype()));
    new QTreeWidgetItem(INDI_general, QStringList(shutdownINDIFunc->prototype()));
    new QTreeWidgetItem(INDI_general, QStringList(setINDIDeviceFunc->prototype()));
    new QTreeWidgetItem(INDI_general, QStringList(switchINDIFunc->prototype()));
    new QTreeWidgetItem(INDI_general, QStringList(setINDIPortFunc->prototype()));
    new QTreeWidgetItem(INDI_general, QStringList(setINDIActionFunc->prototype()));
    new QTreeWidgetItem(INDI_general, QStringList(waitForINDIActionFunc->prototype()));

    // Telescope
    new QTreeWidgetItem(INDI_telescope, QStringList(setINDIScopeActionFunc->prototype()));
    new QTreeWidgetItem(INDI_telescope, QStringList(setINDITargetCoordFunc->prototype()));
    new QTreeWidgetItem(INDI_telescope, QStringList(setINDITargetNameFunc->prototype()));
    new QTreeWidgetItem(INDI_telescope, QStringList(setINDIGeoLocationFunc->prototype()));
    new QTreeWidgetItem(INDI_telescope, QStringList(setINDIUTCFunc->prototype()));

    // CCD
    new QTreeWidgetItem(INDI_ccd, QStringList(setINDICCDTempFunc->prototype()));
    new QTreeWidgetItem(INDI_ccd, QStringList(setINDIFrameTypeFunc->prototype()));
    new QTreeWidgetItem(INDI_ccd, QStringList(startINDIExposureFunc->prototype()));

    // Focuser
    new QTreeWidgetItem(INDI_focuser, QStringList(setINDIFocusSpeedFunc->prototype()));
    new QTreeWidgetItem(INDI_focuser, QStringList(setINDIFocusTimeoutFunc->prototype()));
    new QTreeWidgetItem(INDI_focuser, QStringList(startINDIFocusFunc->prototype()));

    // Filter
    new QTreeWidgetItem(INDI_filter, QStringList(setINDIFilterNumFunc->prototype()));
	#endif

    //Add icons to Push Buttons
    sb->NewButton->setIcon( QIcon::fromTheme( "document-new" ) );
    sb->OpenButton->setIcon( QIcon::fromTheme( "document-open" ) );
    sb->SaveButton->setIcon( QIcon::fromTheme( "document-save" ) );
    sb->SaveAsButton->setIcon( QIcon::fromTheme( "document-save-as" ) );
    sb->RunButton->setIcon( QIcon::fromTheme( "system-run" ) );
    sb->CopyButton->setIcon( QIcon::fromTheme( "view-refresh" ) );
    sb->AddButton->setIcon( QIcon::fromTheme( "go-previous" ) );
    sb->RemoveButton->setIcon( QIcon::fromTheme( "go-next" ) );
    sb->UpButton->setIcon( QIcon::fromTheme( "go-up" ) );
    sb->DownButton->setIcon( QIcon::fromTheme( "go-down" ) );

    //Prepare the widget stack
    argBlank = new QWidget();
    argLookToward = new ArgLookToward( sb->ArgStack );
    argFindObject = new ArgFindObject( sb->ArgStack ); //shared by Add/RemoveLabel and Add/RemoveTrail
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

	#if 0
    argStartINDI = new ArgStartINDI ( sb->ArgStack );
    argSetDeviceINDI = new ArgSetDeviceINDI (sb->ArgStack);
    argShutdownINDI = new ArgShutdownINDI ( sb->ArgStack );
    argSwitchINDI   = new ArgSwitchINDI ( sb->ArgStack );
    argSetPortINDI  = new ArgSetPortINDI ( sb->ArgStack );
    argSetTargetCoordINDI = new ArgSetTargetCoordINDI ( sb->ArgStack );
    argSetTargetNameINDI  = new ArgSetTargetNameINDI ( sb->ArgStack );
    argSetActionINDI      = new ArgSetActionINDI ( sb->ArgStack );
    argWaitForActionINDI  = new ArgSetActionINDI ( sb->ArgStack );
    argSetFocusSpeedINDI  = new ArgSetFocusSpeedINDI ( sb->ArgStack );
    argStartFocusINDI     = new ArgStartFocusINDI( sb->ArgStack );
    argSetFocusTimeoutINDI = new ArgSetFocusTimeoutINDI( sb->ArgStack );
    argSetGeoLocationINDI  = new ArgSetGeoLocationINDI( sb->ArgStack );
    argStartExposureINDI   = new ArgStartExposureINDI( sb->ArgStack );
    argSetUTCINDI          = new ArgSetUTCINDI( sb->ArgStack );
    argSetScopeActionINDI  = new ArgSetScopeActionINDI( sb->ArgStack );
    argSetFrameTypeINDI    = new ArgSetFrameTypeINDI ( sb->ArgStack );
    argSetCCDTempINDI      = new ArgSetCCDTempINDI( sb->ArgStack );
    argSetFilterNumINDI    = new ArgSetFilterNumINDI( sb->ArgStack );

    argStartFocusINDI->directionCombo->addItem("IN");
    argStartFocusINDI->directionCombo->addItem("OUT");

    argSetScopeActionINDI->actionCombo->addItem("SLEW");
    argSetScopeActionINDI->actionCombo->addItem("TRACK");
    argSetScopeActionINDI->actionCombo->addItem("SYNC");
    argSetScopeActionINDI->actionCombo->addItem("PARK");
    argSetScopeActionINDI->actionCombo->addItem("ABORT");

    argSetFrameTypeINDI->typeCombo->addItem("FRAME_LIGHT");
    argSetFrameTypeINDI->typeCombo->addItem("FRAME_BIAS");
    argSetFrameTypeINDI->typeCombo->addItem("FRAME_DARK");
    argSetFrameTypeINDI->typeCombo->addItem("FRAME_FLAT");
	#endif

    sb->ArgStack->addWidget( argBlank );
    sb->ArgStack->addWidget( argLookToward );
    sb->ArgStack->addWidget( argFindObject );
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

	#if 0
    sb->ArgStack->addWidget( argStartINDI);
    sb->ArgStack->addWidget( argSetDeviceINDI);
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
	#endif

    sb->ArgStack->setCurrentIndex( 0 );

    snd = new ScriptNameDialog( ks );
    otv = new OptionsTreeView( ks );

    otv->resize( sb->width(), 0.5*sb->height() );

    initViewOptions();
    otv->resizeColumns();

    //connect widgets in ScriptBuilderUI
    connect( this, SIGNAL(user1Clicked()), this, SLOT(slotClose()));
    connect( sb->FunctionTree, SIGNAL( itemDoubleClicked(QTreeWidgetItem *, int )), this, SLOT( slotAddFunction() ) );
    connect( sb->FunctionTree, SIGNAL( itemClicked(QTreeWidgetItem*, int) ), this, SLOT( slotShowDoc() ) );
    connect( sb->UpButton, SIGNAL( clicked() ), this, SLOT( slotMoveFunctionUp() ) );
    connect( sb->ScriptListBox, SIGNAL( itemClicked(QListWidgetItem *) ), this, SLOT( slotArgWidget() ) );
    connect( sb->DownButton, SIGNAL( clicked() ), this, SLOT( slotMoveFunctionDown() ) );
    connect( sb->CopyButton, SIGNAL( clicked() ), this, SLOT( slotCopyFunction() ) );
    connect( sb->RemoveButton, SIGNAL( clicked() ), this, SLOT( slotRemoveFunction() ) );
    connect( sb->NewButton, SIGNAL( clicked() ), this, SLOT( slotNew() ) );
    connect( sb->OpenButton, SIGNAL( clicked() ), this, SLOT( slotOpen() ) );
    connect( sb->SaveButton, SIGNAL( clicked() ), this, SLOT( slotSave() ) );
    connect( sb->SaveAsButton, SIGNAL( clicked() ), this, SLOT( slotSaveAs() ) );
    connect( sb->AddButton, SIGNAL( clicked() ), this, SLOT( slotAddFunction() ) );
    connect( sb->RunButton, SIGNAL( clicked() ), this, SLOT( slotRunScript() ) );

    //Connections for Arg Widgets
    connect( argSetGeoLocation->FindCityButton, SIGNAL( clicked() ), this, SLOT( slotFindCity() ) );
    connect( argLookToward->FindButton, SIGNAL( clicked() ), this, SLOT( slotFindObject() ) );
    connect( argChangeViewOption->TreeButton, SIGNAL( clicked() ), this, SLOT( slotShowOptions() ) );

    connect( argLookToward->FocusEdit, SIGNAL( textChanged(const QString &) ), this, SLOT( slotLookToward() ) );
    connect( argFindObject->NameEdit, SIGNAL( textChanged(const QString &) ), this, SLOT( slotArgFindObject() ) );
    connect( argSetRaDec->RABox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotRa() ) );
    connect( argSetRaDec->DecBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotDec() ) );
    connect( argSetAltAz->AltBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotAlt() ) );
    connect( argSetAltAz->AzBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotAz() ) );
    connect( argSetLocalTime->DateWidget, SIGNAL( changed(const QDate&) ), this, SLOT( slotChangeDate() ) );
    connect( argSetLocalTime->TimeBox, SIGNAL( timeChanged(const QTime&) ), this, SLOT( slotChangeTime() ) );
    connect( argWaitFor->DelayBox, SIGNAL( valueChanged(int) ), this, SLOT( slotWaitFor() ) );
    connect( argWaitForKey->WaitKeyEdit, SIGNAL( textChanged(const QString &) ), this, SLOT( slotWaitForKey() ) );
    connect( argSetTracking->CheckTrack, SIGNAL( stateChanged(int) ), this, SLOT( slotTracking() ) );
    connect( argChangeViewOption->OptionName, SIGNAL( activated(const QString &) ), this, SLOT( slotViewOption() ) );
    connect( argChangeViewOption->OptionValue, SIGNAL( textChanged(const QString &) ), this, SLOT( slotViewOption() ) );
    connect( argSetGeoLocation->CityName, SIGNAL( textChanged(const QString &) ), this, SLOT( slotChangeCity() ) );
    connect( argSetGeoLocation->ProvinceName, SIGNAL( textChanged(const QString &) ), this, SLOT( slotChangeProvince() ) );
    connect( argSetGeoLocation->CountryName, SIGNAL( textChanged(const QString &) ), this, SLOT( slotChangeCountry() ) );
    connect( argTimeScale->TimeScale, SIGNAL( scaleChanged(float) ), this, SLOT( slotTimeScale() ) );
    connect( argZoom->ZoomBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotZoom() ) );
    connect( argExportImage->ExportFileName, SIGNAL( textChanged(const QString &) ), this, SLOT( slotExportImage() ) );
    connect( argExportImage->ExportWidth, SIGNAL( valueChanged(int) ), this, SLOT( slotExportImage() ) );
    connect( argExportImage->ExportHeight, SIGNAL( valueChanged(int) ), this, SLOT( slotExportImage() ) );
    connect( argPrintImage->UsePrintDialog, SIGNAL( toggled(bool) ), this, SLOT( slotPrintImage() ) );
    connect( argPrintImage->UseChartColors, SIGNAL( toggled(bool) ), this, SLOT( slotPrintImage() ) );
    connect( argSetColor->ColorName, SIGNAL( activated(const QString &) ), this, SLOT( slotChangeColorName() ) );
    connect( argSetColor->ColorValue, SIGNAL( changed(const QColor &) ), this, SLOT( slotChangeColor() ) );
    connect( argLoadColorScheme->SchemeList, SIGNAL( itemClicked ( QListWidgetItem * )), this, SLOT( slotLoadColorScheme() ) );

    //connect( sb->AppendINDIWait, SIGNAL ( toggled(bool) ), this, SLOT(slotINDIWaitCheck(bool)));

	#if 0
    // Connections for INDI's Arg widgets

    // INDI Start Device
    connect (argStartINDI->deviceName, SIGNAL( textChanged(const QString &) ), this, SLOT(slotINDIStartDeviceName()));
    connect (argStartINDI->LocalButton, SIGNAL ( toggled(bool)), this, SLOT (slotINDIStartDeviceMode()));

    // Set Device Name
    connect (argSetDeviceINDI->deviceName, SIGNAL( textChanged(const QString &) ), this, SLOT(slotINDISetDevice()));

    // INDI Shutdown Device
    connect (argShutdownINDI->deviceName, SIGNAL( textChanged(const QString &) ), this, SLOT(slotINDIShutdown()));

    // INDI Swtich Device
    connect (argSwitchINDI->OnButton, SIGNAL ( toggled( bool)), this, SLOT (slotINDISwitchDeviceConnection()));

    // INDI Set Device Port
    connect (argSetPortINDI->devicePort, SIGNAL( textChanged(const QString &) ), this, SLOT(slotINDISetPortDevicePort()));

    // INDI Set Target Coord
    connect( argSetTargetCoordINDI->RABox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotINDISetTargetCoordDeviceRA() ) );
    connect( argSetTargetCoordINDI->DecBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotINDISetTargetCoordDeviceDEC() ) );

    // INDI Set Target Name
    connect (argSetTargetNameINDI->targetName, SIGNAL( textChanged(const QString &) ), this, SLOT(slotINDISetTargetNameTargetName()));

    // INDI Set Action
    connect (argSetActionINDI->actionName, SIGNAL( textChanged(const QString &) ), this, SLOT(slotINDISetActionName()));

    // INDI Wait For Action
    connect (argWaitForActionINDI->actionName, SIGNAL( textChanged(const QString &) ), this, SLOT(slotINDIWaitForActionName()));

    // INDI Set Focus Speed
    connect (argSetFocusSpeedINDI->speedIN, SIGNAL( valueChanged(int) ), this, SLOT(slotINDISetFocusSpeed()));

    // INDI Start Focus
    connect (argStartFocusINDI->directionCombo, SIGNAL( activated(const QString &) ), this, SLOT(slotINDIStartFocusDirection()));

    // INDI Set Focus Timeout
    connect (argSetFocusTimeoutINDI->timeOut, SIGNAL( valueChanged(int) ), this, SLOT(slotINDISetFocusTimeout()));

    // INDI Set Geo Location
    connect( argSetGeoLocationINDI->longBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotINDISetGeoLocationDeviceLong() ) );
    connect( argSetGeoLocationINDI->latBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotINDISetGeoLocationDeviceLat() ) );

    // INDI Start Exposure
    connect (argStartExposureINDI->timeOut, SIGNAL( valueChanged(int) ), this, SLOT(slotINDIStartExposureTimeout()));

    // INDI Set UTC
    connect (argSetUTCINDI->UTC, SIGNAL( textChanged(const QString &) ), this, SLOT(slotINDISetUTC()));

    // INDI Set Scope Action
    connect (argSetScopeActionINDI->actionCombo, SIGNAL( activated(const QString &) ), this, SLOT(slotINDISetScopeAction()));

    // INDI Set Frame type
    connect (argSetFrameTypeINDI->typeCombo, SIGNAL( activated(const QString &) ), this, SLOT(slotINDISetFrameType()));

    // INDI Set CCD Temp
    connect (argSetCCDTempINDI->temp, SIGNAL( valueChanged(int) ), this, SLOT(slotINDISetCCDTemp()));

    // INDI Set Filter Num
    connect (argSetFilterNumINDI->filter_num, SIGNAL( valueChanged(int) ), this, SLOT(slotINDISetFilterNum()));
	#endif


    //disable some buttons
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
    while ( ! KStarsFunctionList.isEmpty() )
        delete KStarsFunctionList.takeFirst();

    //  while ( ! INDIFunctionList.isEmpty() )
    //    delete INDIFunctionList.takeFirst();

    while( !SimClockFunctionList.isEmpty() )
        delete SimClockFunctionList.takeFirst();

    while ( ! ScriptList.isEmpty() )
        delete ScriptList.takeFirst();
}

void ScriptBuilder::initViewOptions() {
    otv->optionsList()->setRootIsDecorated( true );
    QStringList fields;

    //InfoBoxes
    opsGUI = new QTreeWidgetItem( otv->optionsList(), QStringList(xi18n( "InfoBoxes" )) );
    fields << "ShowInfoBoxes" << xi18n( "Toggle display of all InfoBoxes" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsGUI, fields );
    fields.clear();
    fields << "ShowTimeBox" << xi18n( "Toggle display of Time InfoBox" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsGUI, fields );
    fields.clear();
    fields << "ShowGeoBox" << xi18n( "Toggle display of Geographic InfoBox" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsGUI, fields );
    fields.clear();
    fields << "ShowFocusBox" << xi18n( "Toggle display of Focus InfoBox" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsGUI, fields );
    fields.clear();
    fields << "ShadeTimeBox" << xi18n( "(un)Shade Time InfoBox" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsGUI, fields );
    fields.clear();
    fields << "ShadeGeoBox" << xi18n( "(un)Shade Geographic InfoBox" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsGUI, fields );
    fields.clear();
    fields << "ShadeFocusBox" << xi18n( "(un)Shade Focus InfoBox" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsGUI, fields );
    fields.clear();

    argChangeViewOption->OptionName->addItem( "ShowInfoBoxes" );
    argChangeViewOption->OptionName->addItem( "ShowTimeBox" );
    argChangeViewOption->OptionName->addItem( "ShowGeoBox" );
    argChangeViewOption->OptionName->addItem( "ShowFocusBox" );
    argChangeViewOption->OptionName->addItem( "ShadeTimeBox" );
    argChangeViewOption->OptionName->addItem( "ShadeGeoBox" );
    argChangeViewOption->OptionName->addItem( "ShadeFocusBox" );

    //Toolbars
    opsToolbar = new QTreeWidgetItem( otv->optionsList(), QStringList(xi18n( "Toolbars" )) );
    fields << "ShowMainToolBar" << xi18n( "Toggle display of main toolbar" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsToolbar, fields );
    fields.clear();
    fields << "ShowViewToolBar" << xi18n( "Toggle display of view toolbar" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsToolbar, fields );
    fields.clear();

    argChangeViewOption->OptionName->addItem( "ShowMainToolBar" );
    argChangeViewOption->OptionName->addItem( "ShowViewToolBar" );

    //Show Objects
    opsShowObj = new QTreeWidgetItem( otv->optionsList(), QStringList(xi18n( "Show Objects" )) );
    fields << "ShowStars" << xi18n( "Toggle display of Stars" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowDeepSky" << xi18n( "Toggle display of all deep-sky objects" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowMessier" << xi18n( "Toggle display of Messier object symbols" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowMessierImages" << xi18n( "Toggle display of Messier object images" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowNGC" << xi18n( "Toggle display of NGC objects" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowIC" << xi18n( "Toggle display of IC objects" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowSolarSystem" << xi18n( "Toggle display of all solar system bodies" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowSun" << xi18n( "Toggle display of Sun" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowMoon" << xi18n( "Toggle display of Moon" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowMercury" << xi18n( "Toggle display of Mercury" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowVenus" << xi18n( "Toggle display of Venus" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowMars" << xi18n( "Toggle display of Mars" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowJupiter" << xi18n( "Toggle display of Jupiter" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowSaturn" << xi18n( "Toggle display of Saturn" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowUranus" << xi18n( "Toggle display of Uranus" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowNeptune" << xi18n( "Toggle display of Neptune" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowPluto" << xi18n( "Toggle display of Pluto" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowAsteroids" << xi18n( "Toggle display of Asteroids" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowComets" << xi18n( "Toggle display of Comets" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();

    argChangeViewOption->OptionName->addItem( "ShowStars" );
    argChangeViewOption->OptionName->addItem( "ShowDeepSky" );
    argChangeViewOption->OptionName->addItem( "ShowMessier" );
    argChangeViewOption->OptionName->addItem( "ShowMessierImages" );
    argChangeViewOption->OptionName->addItem( "ShowNGC" );
    argChangeViewOption->OptionName->addItem( "ShowIC" );
    argChangeViewOption->OptionName->addItem( "ShowSolarSystem" );
    argChangeViewOption->OptionName->addItem( "ShowSun" );
    argChangeViewOption->OptionName->addItem( "ShowMoon" );
    argChangeViewOption->OptionName->addItem( "ShowMercury" );
    argChangeViewOption->OptionName->addItem( "ShowVenus" );
    argChangeViewOption->OptionName->addItem( "ShowMars" );
    argChangeViewOption->OptionName->addItem( "ShowJupiter" );
    argChangeViewOption->OptionName->addItem( "ShowSaturn" );
    argChangeViewOption->OptionName->addItem( "ShowUranus" );
    argChangeViewOption->OptionName->addItem( "ShowNeptune" );
    argChangeViewOption->OptionName->addItem( "ShowPluto" );
    argChangeViewOption->OptionName->addItem( "ShowAsteroids" );
    argChangeViewOption->OptionName->addItem( "ShowComets" );

    opsShowOther = new QTreeWidgetItem( otv->optionsList(), QStringList(xi18n( "Show Other" )) );
    fields << "ShowCLines" << xi18n( "Toggle display of constellation lines" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowOther, fields );
    fields.clear();
    fields << "ShowCBounds" << xi18n( "Toggle display of constellation boundaries" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowOther, fields );
    fields.clear();
    fields << "ShowCNames" << xi18n( "Toggle display of constellation names" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowOther, fields );
    fields.clear();
    fields << "ShowMilkyWay" << xi18n( "Toggle display of Milky Way" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowOther, fields );
    fields.clear();
    fields << "ShowGrid" << xi18n( "Toggle display of the coordinate grid" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowOther, fields );
    fields.clear();
    fields << "ShowEquator" << xi18n( "Toggle display of the celestial equator" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowOther, fields );
    fields.clear();
    fields << "ShowEcliptic" << xi18n( "Toggle display of the ecliptic" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowOther, fields );
    fields.clear();
    fields << "ShowHorizon" << xi18n( "Toggle display of the horizon line" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowOther, fields );
    fields.clear();
    fields << "ShowGround" << xi18n( "Toggle display of the opaque ground" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowOther, fields );
    fields.clear();
    fields << "ShowStarNames" << xi18n( "Toggle display of star name labels" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowOther, fields );
    fields.clear();
    fields << "ShowStarMagnitudes" << xi18n( "Toggle display of star magnitude labels" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowOther, fields );
    fields.clear();
    fields << "ShowAsteroidNames" << xi18n( "Toggle display of asteroid name labels" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowOther, fields );
    fields.clear();
    fields << "ShowCometNames" << xi18n( "Toggle display of comet name labels" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowOther, fields );
    fields.clear();
    fields << "ShowPlanetNames" << xi18n( "Toggle display of planet name labels" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowOther, fields );
    fields.clear();
    fields << "ShowPlanetImages" << xi18n( "Toggle display of planet images" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsShowOther, fields );
    fields.clear();

    argChangeViewOption->OptionName->addItem( "ShowCLines" );
    argChangeViewOption->OptionName->addItem( "ShowCBounds" );
    argChangeViewOption->OptionName->addItem( "ShowCNames" );
    argChangeViewOption->OptionName->addItem( "ShowMilkyWay" );
    argChangeViewOption->OptionName->addItem( "ShowGrid" );
    argChangeViewOption->OptionName->addItem( "ShowEquator" );
    argChangeViewOption->OptionName->addItem( "ShowEcliptic" );
    argChangeViewOption->OptionName->addItem( "ShowHorizon" );
    argChangeViewOption->OptionName->addItem( "ShowGround" );
    argChangeViewOption->OptionName->addItem( "ShowStarNames" );
    argChangeViewOption->OptionName->addItem( "ShowStarMagnitudes" );
    argChangeViewOption->OptionName->addItem( "ShowAsteroidNames" );
    argChangeViewOption->OptionName->addItem( "ShowCometNames" );
    argChangeViewOption->OptionName->addItem( "ShowPlanetNames" );
    argChangeViewOption->OptionName->addItem( "ShowPlanetImages" );

    opsCName = new QTreeWidgetItem( otv->optionsList(), QStringList(xi18n( "Constellation Names" )) );
    fields << "UseLatinConstellNames" << xi18n( "Show Latin constellation names" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsCName, fields );
    fields.clear();
    fields << "UseLocalConstellNames" <<xi18n( "Show constellation names in local language" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsCName, fields );
    fields.clear();
    fields << "UseAbbrevConstellNames" <<xi18n( "Show IAU-standard constellation abbreviations" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsCName, fields );
    fields.clear();

    argChangeViewOption->OptionName->addItem( "UseLatinConstellNames" );
    argChangeViewOption->OptionName->addItem( "UseLocalConstellNames" );
    argChangeViewOption->OptionName->addItem( "UseAbbrevConstellNames" );

    opsHide = new QTreeWidgetItem( otv->optionsList(), QStringList(xi18n( "Hide Items" )) );
    fields << "HideOnSlew" << xi18n( "Toggle whether objects hidden while slewing display" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsHide, fields );
    fields.clear();
    fields << "SlewTimeScale" << xi18n( "Timestep threshold (in seconds) for hiding objects")  << xi18n( "double" );
    new QTreeWidgetItem( opsHide, fields );
    fields.clear();
    fields << "HideStars" << xi18n( "Hide faint stars while slewing?" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsHide, fields );
    fields.clear();
    fields << "HidePlanets" << xi18n( "Hide solar system bodies while slewing?" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsHide, fields );
    fields.clear();
    fields << "HideMessier" << xi18n( "Hide Messier objects while slewing?" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsHide, fields );
    fields.clear();
    fields << "HideNGC" << xi18n( "Hide NGC objects while slewing?" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsHide, fields );
    fields.clear();
    fields << "HideIC" << xi18n( "Hide IC objects while slewing?" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsHide, fields );
    fields.clear();
    fields << "HideMilkyWay" << xi18n( "Hide Milky Way while slewing?" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsHide, fields );
    fields.clear();
    fields << "HideCNames" << xi18n( "Hide constellation names while slewing?" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsHide, fields );
    fields.clear();
    fields << "HideCLines" << xi18n( "Hide constellation lines while slewing?" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsHide, fields );
    fields.clear();
    fields << "HideCBounds" << xi18n( "Hide constellation boundaries while slewing?" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsHide, fields );
    fields.clear();
    fields << "HideGrid" << xi18n( "Hide coordinate grid while slewing?" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsHide, fields );
    fields.clear();

    argChangeViewOption->OptionName->addItem( "HideOnSlew" );
    argChangeViewOption->OptionName->addItem( "SlewTimeScale" );
    argChangeViewOption->OptionName->addItem( "HideStars" );
    argChangeViewOption->OptionName->addItem( "HidePlanets" );
    argChangeViewOption->OptionName->addItem( "HideMessier" );
    argChangeViewOption->OptionName->addItem( "HideNGC" );
    argChangeViewOption->OptionName->addItem( "HideIC" );
    argChangeViewOption->OptionName->addItem( "HideMilkyWay" );
    argChangeViewOption->OptionName->addItem( "HideCNames" );
    argChangeViewOption->OptionName->addItem( "HideCLines" );
    argChangeViewOption->OptionName->addItem( "HideCBounds" );
    argChangeViewOption->OptionName->addItem( "HideGrid" );

    opsSkymap = new QTreeWidgetItem( otv->optionsList(), QStringList(xi18n( "Skymap Options" )) );
    fields << "UseAltAz" << xi18n( "Use Horizontal coordinates? (otherwise, use Equatorial)")  << xi18n( "bool" );
    new QTreeWidgetItem( opsSkymap, fields );
    fields.clear();
    fields << "ZoomFactor" << xi18n( "Set the Zoom Factor" ) << xi18n( "double" );
    new QTreeWidgetItem( opsSkymap, fields );
    fields.clear();
    fields << "FOVName" << xi18n( "Select angular size for the FOV symbol (in arcmin)")  << xi18n( "double" );
    new QTreeWidgetItem( opsSkymap, fields );
    fields.clear();
    fields << "FOVShape" << xi18n( "Select shape for the FOV symbol (0=Square, 1=Circle, 2=Crosshairs, 4=Bullseye)" ) << xi18n( "int" );
    new QTreeWidgetItem( opsSkymap, fields );
    fields.clear();
    fields << "FOVColor" << xi18n( "Select color for the FOV symbol" ) << xi18n( "string" );
    new QTreeWidgetItem( opsSkymap, fields );
    fields.clear();
    fields << "UseAnimatedSlewing" << xi18n( "Use animated slewing? (otherwise, \"snap\" to new focus)" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsSkymap, fields );
    fields.clear();
    fields << "UseRefraction" << xi18n( "Correct for atmospheric refraction?" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsSkymap, fields );
    fields.clear();
    fields << "UseAutoLabel" << xi18n( "Automatically attach name label to centered object?" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsSkymap, fields );
    fields.clear();
    fields << "UseHoverLabel" << xi18n( "Attach temporary name label when hovering mouse over an object?" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsSkymap, fields );
    fields.clear();
    fields << "UseAutoTrail" << xi18n( "Automatically add trail to centered solar system body?" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsSkymap, fields );
    fields.clear();
    fields << "FadePlanetTrails" << xi18n( "Planet trails fade to sky color? (otherwise color is constant)" ) << xi18n( "bool" );
    new QTreeWidgetItem( opsSkymap, fields );
    fields.clear();

    argChangeViewOption->OptionName->addItem( "UseAltAz" );
    argChangeViewOption->OptionName->addItem( "ZoomFactor" );
    argChangeViewOption->OptionName->addItem( "FOVName" );
    argChangeViewOption->OptionName->addItem( "FOVSize" );
    argChangeViewOption->OptionName->addItem( "FOVShape" );
    argChangeViewOption->OptionName->addItem( "FOVColor" );
    argChangeViewOption->OptionName->addItem( "UseRefraction" );
    argChangeViewOption->OptionName->addItem( "UseAutoLabel" );
    argChangeViewOption->OptionName->addItem( "UseHoverLabel" );
    argChangeViewOption->OptionName->addItem( "UseAutoTrail" );
    argChangeViewOption->OptionName->addItem( "AnimateSlewing" );
    argChangeViewOption->OptionName->addItem( "FadePlanetTrails" );


    opsLimit = new QTreeWidgetItem( otv->optionsList(), QStringList(xi18n( "Limits" )) );
    /*
    fields << "magLimitDrawStar" << xi18n( "magnitude of faintest star drawn on map when zoomed in" ) << xi18n( "double" );
    new QTreeWidgetItem( opsLimit, fields );
    fields.clear();
    fields << "magLimitDrawStarZoomOut" << xi18n( "magnitude of faintest star drawn on map when zoomed out" ) << xi18n( "double" );
    new QTreeWidgetItem( opsLimit, fields );
    fields.clear();
    */

    // TODO: We have disabled the following two features. Enable them when feasible...
    /*
    fields << "magLimitDrawDeepSky" << xi18n( "magnitude of faintest nonstellar object drawn on map when zoomed in" ) << xi18n( "double" );
    new QTreeWidgetItem( opsLimit, fields );
    fields.clear();
    fields << "magLimitDrawDeepSkyZoomOut" << xi18n( "magnitude of faintest nonstellar object drawn on map when zoomed out" ) << xi18n( "double" );
    new QTreeWidgetItem( opsLimit, fields );
    fields.clear();
    */

    //FIXME: This description is incorrect! Fix after strings freeze
    fields << "starLabelDensity" << xi18n( "magnitude of faintest star labeled on map" ) << xi18n( "double" );
    new QTreeWidgetItem( opsLimit, fields );
    fields.clear();
    fields << "magLimitHideStar" << xi18n( "magnitude of brightest star hidden while slewing" ) << xi18n( "double" );
    new QTreeWidgetItem( opsLimit, fields );
    fields.clear();
    fields << "magLimitAsteroid" << xi18n( "magnitude of faintest asteroid drawn on map" ) << xi18n( "double" );
    new QTreeWidgetItem( opsLimit, fields );
    fields.clear();
    //FIXME: This description is incorrect! Fix after strings freeze
    fields << "asteroidLabelDensity" << xi18n( "magnitude of faintest asteroid labeled on map" ) << xi18n( "double" );
    new QTreeWidgetItem( opsLimit, fields );
    fields.clear();
    fields << "maxRadCometName" << xi18n( "comets nearer to the Sun than this (in AU) are labeled on map" ) << xi18n( "double" );
    new QTreeWidgetItem( opsLimit, fields );
    fields.clear();

    //    argChangeViewOption->OptionName->addItem( "magLimitDrawStar" );
    //    argChangeViewOption->OptionName->addItem( "magLimitDrawStarZoomOut" );
    argChangeViewOption->OptionName->addItem( "magLimitDrawDeepSky" );
    argChangeViewOption->OptionName->addItem( "magLimitDrawDeepSkyZoomOut" );
    argChangeViewOption->OptionName->addItem( "starLabelDensity" );
    argChangeViewOption->OptionName->addItem( "magLimitHideStar" );
    argChangeViewOption->OptionName->addItem( "magLimitAsteroid" );
    argChangeViewOption->OptionName->addItem( "asteroidLabelDensity" );
    argChangeViewOption->OptionName->addItem( "maxRadCometName" );

    //init the list of color names and values
    for ( unsigned int i=0; i < ks->data()->colorScheme()->numberOfColors(); ++i ) {
        argSetColor->ColorName->addItem( ks->data()->colorScheme()->nameAt(i) );
    }

    //init list of color scheme names
    argLoadColorScheme->SchemeList->addItem( xi18nc( "use default color scheme", "Default Colors" ) );
    argLoadColorScheme->SchemeList->addItem( xi18nc( "use 'star chart' color scheme", "Star Chart" ) );
    argLoadColorScheme->SchemeList->addItem( xi18nc( "use 'night vision' color scheme", "Night Vision" ) );
    argLoadColorScheme->SchemeList->addItem( xi18nc( "use 'moonless night' color scheme", "Moonless Night" ) );

    QFile file;
    QString line;
    file.setFileName( QStandardPaths::locate(QStandardPaths::DataLocation, "colors.dat" ) ); //determine filename in local user KDE directory tree.
    if ( file.open( QIODevice::ReadOnly ) ) {
        QTextStream stream( &file );

        while ( !stream.atEnd() ) {
            line = stream.readLine();
            argLoadColorScheme->SchemeList->addItem( line.left( line.indexOf( ':' ) ) );
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
        sb->ArgStack->setCurrentWidget( argBlank );

        sb->CopyButton->setEnabled( false );
        sb->RemoveButton->setEnabled( false );
        sb->RunButton->setEnabled( false );
        sb->SaveAsButton->setEnabled( false );

        currentFileURL.clear();
        currentScriptName.clear();
    }
}

void ScriptBuilder::slotOpen() {
    saveWarning();

    QString fname;
    QTemporaryFile tmpfile;
    tmpfile.open();

    /*
     * FIXME Needs porting to KF5
     *
    if ( !UnsavedChanges ) {
        currentFileURL = KFileDialog::getOpenUrl( currentDir, "*.kstars|" + xi18nc("Filter by file type: KStars Scripts.", "KStars Scripts (*.kstars)") );

        if ( currentFileURL.isValid() ) {
            currentDir = currentFileURL.directory();

            ScriptList.clear();
            sb->ScriptListBox->clear();
            sb->ArgStack->setCurrentWidget( argBlank );

            if ( currentFileURL.isLocalFile() ) {
                fname = currentFileURL.toLocalFile();
            } else {
                fname = tmpfile.fileName();
                if ( ! KIO::NetAccess::download( currentFileURL, fname, (QWidget*) 0 ) )
                    KMessageBox::sorry( 0, xi18n( "Could not download remote file." ), xi18n( "Download Error" ) );
            }

            QFile f( fname );
            if ( !f.open( QIODevice::ReadOnly) ) {
                QString message = xi18n( "Could not open file %1.", f.fileName() );
                KMessageBox::sorry( 0, message, xi18n( "Could Not Open File" ) );
                currentFileURL.clear();
                return;
            }

            QTextStream istream(&f);
            readScript( istream );

            f.close();
        } else if ( ! currentFileURL.url().isEmpty() ) {
            QString message = xi18n( "Invalid URL: %1", currentFileURL.url() );
            KMessageBox::sorry( 0, message, xi18n( "Invalid URL" ) );
            currentFileURL.clear();
        }
    }
    */
}

void ScriptBuilder::slotSave()
{
    QString fname;
    QTemporaryFile tmpfile;
    tmpfile.open();

    /*
     * FIXME Needs porting to KF5
     *
    if ( currentScriptName.isEmpty() ) {
        //Get Script Name and Author info
        if ( snd->exec() == QDialog::Accepted ) {
            currentScriptName = snd->scriptName();
            currentAuthor = snd->authorName();
        } else {
            return;
        }
    }

    bool newFilename = false;
    if ( currentFileURL.isEmpty() ) {
        currentFileURL = KFileDialog::getSaveUrl( currentDir, "*.kstars|" + xi18nc("Filter by file type: KStars Scripts.", "KStars Scripts (*.kstars)") );
        newFilename = true;
    }

    if ( currentFileURL.isValid() ) {
        currentDir = currentFileURL.directory();

        if ( currentFileURL.isLocalFile() ) {
            fname = currentFileURL.toLocalFile();

            //Warn user if file exists
            if (newFilename == true && QFile::exists(currentFileURL.path())) {
                int r=KMessageBox::warningContinueCancel(static_cast<QWidget *>(parent()),
                        xi18n( "A file named \"%1\" already exists. "
                              "Overwrite it?" , currentFileURL.fileName()),
                        xi18n( "Overwrite File?" ),
                        KStandardGuiItem::overwrite() );

                if(r==KMessageBox::Cancel) return;
            }
        } else {
            fname = tmpfile.fileName();
        }

        if ( fname.right( 7 ).toLower() != ".kstars" ) fname += ".kstars";

        QFile f( fname );
        if ( !f.open( QIODevice::WriteOnly) ) {
            QString message = xi18n( "Could not open file %1.", f.fileName() );
            KMessageBox::sorry( 0, message, xi18n( "Could Not Open File" ) );
            currentFileURL.clear();
            return;
        }

        QTextStream ostream(&f);
        writeScript( ostream );
        f.close();

        //set rwx for owner, rx for group, rx for other
        chmod( fname.toLatin1(), S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH );

        if ( tmpfile.fileName() == fname ) { //need to upload to remote location
            if ( ! KIO::NetAccess::upload( tmpfile.fileName(), currentFileURL, (QWidget*) 0 ) ) {
                QString message = xi18n( "Could not upload image to remote location: %1", currentFileURL.prettyUrl() );
                KMessageBox::sorry( 0, message, xi18n( "Could not upload file" ) );
            }
        }

        setUnsavedChanges( false );

    } else {
        QString message = xi18n( "Invalid URL: %1", currentFileURL.url() );
        KMessageBox::sorry( 0, message, xi18n( "Invalid URL" ) );
        currentFileURL.clear();
    }
    */
}

void ScriptBuilder::slotSaveAs() {
    currentFileURL.clear();
    currentScriptName.clear();
    slotSave();
}

void ScriptBuilder::saveWarning() {
    if ( UnsavedChanges ) {
        QString caption = xi18n( "Save Changes to Script?" );
        QString message = xi18n( "The current script has unsaved changes.  Would you like to save before closing it?" );
        int ans = KMessageBox::warningYesNoCancel( 0, message, caption, KStandardGuiItem::save(), KStandardGuiItem::discard() );
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
    //QString fname = tmpfile.name();
    QString fname = QDir::tempPath() + QLatin1Char('/') +  "kstars-tempscript";

    QFile f( fname );
    if ( f.exists() ) f.remove();
    if ( !f.open( QIODevice::WriteOnly) ) {
        QString message = xi18n( "Could not open file %1.", f.fileName() );
        KMessageBox::sorry( 0, message, xi18n( "Could Not Open File" ) );
        currentFileURL.clear();
        return;
    }

    QTextStream ostream(&f);
    writeScript( ostream );
    f.close();

    //set rwx for owner, rx for group, rx for other
    chmod( QFile::encodeName( f.fileName() ), S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH );

    KProcess p;
    p << f.fileName();
    p.start();

    if( !p.waitForStarted() )
        qDebug() << "Process did not start.";

    while ( !p.waitForFinished(10) ) {
        qApp->processEvents(); //otherwise tempfile may get deleted before script completes.
        if( p.state() != QProcess::Running )
            break;
    }
    //delete temp file
    if ( f.exists() ) f.remove();

    //uncomment if 'hide()' is uncommented...
    //	show();
}

/*
  This can't work anymore and is also not protable in any way :(
*/
void ScriptBuilder::writeScript( QTextStream &ostream )
{
    // FIXME Without --print-reply, the dbus-send doesn't do anything, why??
    QString dbus_call  = "dbus-send --dest=org.kde.kstars --print-reply ";
    QString main_method = "/KStars org.kde.kstars.";
    QString clock_method = "/KStars/SimClock org.kde.kstars.SimClock.";

    //Write script header
    ostream << "#!/bin/bash" << endl;
    ostream << "#KStars DBus script: " << currentScriptName << endl;
    ostream << "#by " << currentAuthor << endl;
    ostream << "#last modified: " << KStarsDateTime::currentDateTime().toString( Qt::ISODate ) << endl;
    ostream << "#" << endl;

    foreach ( ScriptFunction *sf, ScriptList )
    {
        if (!sf->valid()) continue;

        if ( sf->isClockFunction() )
        {
            ostream << dbus_call << clock_method << sf->scriptLine() << endl;
        } else
        {
            ostream << dbus_call << main_method << sf->scriptLine() << endl;

            // TODO INDI scripting to be supported under KDE 4.1
			#if 0

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

                if ( sf->argVal(0).contains(" "))
                    ostream << mainpre << "waitForINDIAction " << "\"" << sf->argVal(0) << "\" " << sf->INDIProperty() << endl;
                else
                    ostream << mainpre << "waitForINDIAction " << sf->argVal(0) << " " << sf->INDIProperty() << endl;
            }
			#endif
        }
    }

    //Write script footer
    ostream << "##" << endl;
}

void ScriptBuilder::readScript( QTextStream &istream )
{
    QString line;
    QString service_name = "org.kde.kstars.";
    QString fn_name;

    while ( ! istream.atEnd() ) {
        line = istream.readLine();

        //look for name of script
        if ( line.contains( "#KStars DBus script: " ) )
            currentScriptName = line.mid( 21 ).trimmed();

        //look for author of scriptbuilder
        if ( line.contains( "#by " ) )
            currentAuthor = line.mid( 4 ).trimmed();

        //Actual script functions
        if ( line.left(4) == "dbus" ) {

            //is ClockFunction?
            if ( line.contains( "SimClock" ) ) {
                service_name += "SimClock.";
            }

            //remove leading dbus prefix
            line = line.mid( line.lastIndexOf(service_name) + service_name.count() );


            fn_name = line.left(line.indexOf(" "));

            line = line.mid(line.indexOf(" ") + 1);

            //construct a stringlist that is fcn name and its arg name/value pairs
            QStringList fn;

            // If the function lacks any arguments, do not attempt to split
            if (fn_name != line)
                fn = line.split(' ');

            if ( parseFunction( fn_name, fn ) )
            {
                sb->ScriptListBox->addItem( ScriptList.last()->name() );
                // Initially, any read script is valid!
                ScriptList.last()->setValid(true);
            }
            else qWarning() << xi18n( "Could not parse script.  Line was: %1", line ) ;

        } // end if left(4) == "dcop"
    } // end while !atEnd()

    //Select first item in sb->ScriptListBox
    if ( sb->ScriptListBox->count() ) {
        sb->ScriptListBox->setCurrentItem( 0 );
        slotArgWidget();
    }
}

bool ScriptBuilder::parseFunction( QString fn_name, QStringList &fn )
{
    // clean up the string list first if needed
    // We need to perform this in case we havea quoted string "NGC 3000" because this will counted
    // as two arguments, and it should be counted as one.
    bool foundQuote(false), quoteProcessed(false);
    QString cur, arg;
    QStringList::iterator it;

    for (it = fn.begin(); it != fn.end(); ++it)
    {
        cur = (*it);

        cur = cur.mid(cur.indexOf(":") + 1).remove('\'');

        (*it) = cur;

        if ( cur.startsWith('\"'))
        {
            arg += cur.right(cur.length() - 1);
            arg += ' ';
            foundQuote = true;
            quoteProcessed = true;
        }
        else if (cur.endsWith('\"'))
        {
            arg += cur.left(cur.length() -1);
            arg += '\'';
            foundQuote = false;
        }
        else if (foundQuote)
        {
            arg += cur;
            arg += ' ';
        }
        else
        {
            arg += cur;
            arg += '\'';
        }
    }

    if (quoteProcessed)
        fn = arg.split( '\'', QString::SkipEmptyParts );

    //loop over known functions to find a name match
    foreach ( ScriptFunction *sf, KStarsFunctionList )
    {
        if ( fn_name == sf->name() )
        {

            if ( fn_name == "setGeoLocation" )
            {
                QString city( fn[0] ), prov, cntry( fn[1] );
                prov.clear();
                if ( fn.count() == 4 ) { prov = fn[1]; cntry = fn[2]; }
                if ( fn.count() == 3 || fn.count() == 4 )
                {
                    ScriptList.append( new ScriptFunction( sf ) );
                    ScriptList.last()->setArg( 0, city );
                    ScriptList.last()->setArg( 1, prov );
                    ScriptList.last()->setArg( 2, cntry );
                } else return false;

            } else if ( fn.count() != sf->numArgs()) return false;

            ScriptList.append( new ScriptFunction( sf ) );

            for ( int i=0; i<sf->numArgs(); ++i )
                ScriptList.last()->setArg( i, fn[i] );

            return true;
        }

        foreach ( ScriptFunction *sf, SimClockFunctionList )
        {
            if ( fn_name == sf->name() )
            {

                if ( fn.count() != sf->numArgs()) return false;

                ScriptList.append( new ScriptFunction( sf ) );

                for ( int i=0; i<sf->numArgs(); ++i )
                    ScriptList.last()->setArg( i, fn[i] );

                return true;
            }

        }

        #if 0
        foreach ( ScriptFunction *sf, INDIFunctionList )
        {
            if ( fn[0] == sf->name() )
            {

                if ( fn.count() != sf->numArgs() + 1 ) return false;

                ScriptList.append( new ScriptFunction( sf ) );

                for ( int i=0; i<sf->numArgs(); ++i )
                    ScriptList.last()->setArg( i, fn[i+1] );

                return true;
            }
        }
        #endif
    }

    //if we get here, no function-name match was found
    return false;
}

void ScriptBuilder::setUnsavedChanges( bool b ) {
    if ( checkForChanges ) {
        UnsavedChanges = b;
        sb->SaveButton->setEnabled( b );
    }
}

void ScriptBuilder::slotCopyFunction() {
    if ( ! UnsavedChanges ) setUnsavedChanges( true );

    int Pos = sb->ScriptListBox->currentRow() + 1;
    ScriptList.insert( Pos, new ScriptFunction( ScriptList[ Pos-1 ] ) );
    //copy ArgVals
    for ( int i=0; i < ScriptList[ Pos-1 ]->numArgs(); ++i )
        ScriptList[Pos]->setArg(i, ScriptList[ Pos-1 ]->argVal(i) );

    sb->ScriptListBox->insertItem( Pos, ScriptList[Pos]->name());
    //sb->ScriptListBox->setSelected( Pos, true );
    sb->ScriptListBox->setCurrentRow(Pos);
    slotArgWidget();
}

void ScriptBuilder::slotRemoveFunction() {
    setUnsavedChanges( true );

    int Pos = sb->ScriptListBox->currentRow();
    ScriptList.removeAt( Pos );
    sb->ScriptListBox->takeItem( Pos );
    if ( sb->ScriptListBox->count() == 0 ) {
        sb->ArgStack->setCurrentWidget( argBlank );
        sb->CopyButton->setEnabled( false );
        sb->RemoveButton->setEnabled( false );
        sb->RunButton->setEnabled( false );
        sb->SaveAsButton->setEnabled( false );
    } else {
        //sb->ScriptListBox->setSelected( Pos, true );
        if ( Pos == sb->ScriptListBox->count() ) {
            Pos = Pos - 1;
        }
        sb->ScriptListBox->setCurrentRow(Pos);
    }
    slotArgWidget();
}

void ScriptBuilder::slotAddFunction() {

    ScriptFunction *sc = NULL, *found = NULL;
    QTreeWidgetItem *currentItem = sb->FunctionTree->currentItem();

    if ( currentItem == NULL || currentItem->parent() == 0)
        return;

    foreach ( sc, KStarsFunctionList )
    if (sc->prototype() == currentItem->text(0))
    {
        found = sc;
        break;
    }

    foreach ( sc, SimClockFunctionList )
    if (sc->prototype() == currentItem->text(0))
    {
        found = sc;
        break;
    }

	 #if 0
    if (found == NULL)
    {
        foreach ( sc, INDIFunctionList )
        if (sc->prototype() == currentItem->text(0))
        {
            found = sc;
            break;
        }
    }
	#endif

    if (found == NULL) return;

    setUnsavedChanges( true );

    int Pos = sb->ScriptListBox->currentRow() + 1;

    ScriptList.insert( Pos, new ScriptFunction(found) );
    sb->ScriptListBox->insertItem(Pos,  ScriptList[Pos]->name());
    sb->ScriptListBox->setCurrentRow(Pos);
    slotArgWidget();
}

void ScriptBuilder::slotMoveFunctionUp() {
    if ( sb->ScriptListBox->currentRow() > 0 ) {
        setUnsavedChanges( true );

        //QString t = sb->ScriptListBox->currentItem()->text();
        QString t = sb->ScriptListBox->currentItem()->text();
        unsigned int n = sb->ScriptListBox->currentRow();

        ScriptFunction *tmp = ScriptList.takeAt( n );
        ScriptList.insert( n-1, tmp );

        sb->ScriptListBox->takeItem( n );
        sb->ScriptListBox->insertItem( n-1, t);
        sb->ScriptListBox->setCurrentRow(n-1);
        slotArgWidget();
    }
}

void ScriptBuilder::slotMoveFunctionDown() {
    if ( sb->ScriptListBox->currentRow() > -1 &&
            sb->ScriptListBox->currentRow() < ((int) sb->ScriptListBox->count())-1 ) {
        setUnsavedChanges( true );

        QString t = sb->ScriptListBox->currentItem()->text();
        unsigned int n = sb->ScriptListBox->currentRow();

        ScriptFunction *tmp = ScriptList.takeAt( n );
        ScriptList.insert( n+1, tmp );

        sb->ScriptListBox->takeItem( n );
        sb->ScriptListBox->insertItem( n+1 , t);
        sb->ScriptListBox->setCurrentRow( n+1);
        slotArgWidget();
    }
}

void ScriptBuilder::slotArgWidget() {
    //First, setEnabled on buttons that act on the selected script function
    if ( sb->ScriptListBox->currentRow() == -1 ) { //no selection
        sb->CopyButton->setEnabled( false );
        sb->RemoveButton->setEnabled( false );
        sb->UpButton->setEnabled( false );
        sb->DownButton->setEnabled( false );
    } else if ( sb->ScriptListBox->count() == 1 ) { //only one item, so disable up/down buttons
        sb->CopyButton->setEnabled( true );
        sb->RemoveButton->setEnabled( true );
        sb->UpButton->setEnabled( false );
        sb->DownButton->setEnabled( false );
    } else if ( sb->ScriptListBox->currentRow() == 0 ) { //first item selected
        sb->CopyButton->setEnabled( true );
        sb->RemoveButton->setEnabled( true );
        sb->UpButton->setEnabled( false );
        sb->DownButton->setEnabled( true );
    } else if ( sb->ScriptListBox->currentRow() == ((int) sb->ScriptListBox->count())-1 ) { //last item selected
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

    //RunButton and SaveAs button enabled when script not empty.
    if ( sb->ScriptListBox->count() ) {
        sb->RunButton->setEnabled( true );
        sb->SaveAsButton->setEnabled( true );
    } else {
        sb->RunButton->setEnabled( false );
        sb->SaveAsButton->setEnabled( false );
        setUnsavedChanges( false );
    }

    //Display the function's arguments widget
    if ( sb->ScriptListBox->currentRow() > -1 &&
            sb->ScriptListBox->currentRow() < ((int) sb->ScriptListBox->count()) ) {
        QString t = sb->ScriptListBox->currentItem()->text();
        unsigned int n = sb->ScriptListBox->currentRow();
        ScriptFunction *sf = ScriptList.at( n );

        checkForChanges = false; //Don't signal unsaved changes

        if ( sf->name() == "lookTowards" ) {
            sb->ArgStack->setCurrentWidget( argLookToward );
            QString s = sf->argVal(0);
            argLookToward->FocusEdit->setEditText( s );

        } else if ( sf->name() == "addLabel" || sf->name() == "removeLabel" || sf->name() == "addTrail" || sf->name() == "removeTrail" ) {
            sb->ArgStack->setCurrentWidget( argFindObject );
            QString s = sf->argVal(0);
            argFindObject->NameEdit->setText( s );

        } else if ( sf->name() == "setRaDec" ) {
            bool ok(false);
            double r(0.0),d(0.0);
            dms ra(0.0);

            sb->ArgStack->setCurrentWidget( argSetRaDec );

            ok = !sf->argVal(0).isEmpty();
            if (ok) r = sf->argVal(0).toDouble(&ok);
            else argSetRaDec->RABox->clear();
        if (ok) { ra.setH(r); argSetRaDec->RABox->showInHours( ra ); }

            ok = !sf->argVal(1).isEmpty();
            if (ok) d = sf->argVal(1).toDouble(&ok);
            else argSetRaDec->DecBox->clear();
            if (ok) argSetRaDec->DecBox->showInDegrees( dms(d) );

        } else if ( sf->name() == "setAltAz" ) {
            bool ok(false);
            double x(0.0),y(0.0);

            sb->ArgStack->setCurrentWidget( argSetAltAz );

            ok = !sf->argVal(0).isEmpty();
            if (ok) y = sf->argVal(0).toDouble(&ok);
            else argSetAltAz->AzBox->clear();
            if (ok) argSetAltAz->AltBox->showInDegrees( dms(y) );
            else argSetAltAz->AltBox->clear();

            ok = !sf->argVal(1).isEmpty();
            x = sf->argVal(1).toDouble(&ok);
            if (ok) argSetAltAz->AzBox->showInDegrees( dms(x) );

        } else if ( sf->name() == "zoomIn" ) {
            sb->ArgStack->setCurrentWidget( argBlank );
            //no Args

        } else if ( sf->name() == "zoomOut" ) {
            sb->ArgStack->setCurrentWidget( argBlank );
            //no Args

        } else if ( sf->name() == "defaultZoom" ) {
            sb->ArgStack->setCurrentWidget( argBlank );
            //no Args

        } else if ( sf->name() == "zoom" ) {
            sb->ArgStack->setCurrentWidget( argZoom );
            bool ok(false);
            /*double z = */sf->argVal(0).toDouble(&ok);
            if (ok) argZoom->ZoomBox->setText( sf->argVal(0) );
            else argZoom->ZoomBox->setText( "2000." );

        } else if ( sf->name() == "exportImage" ) {
            sb->ArgStack->setCurrentWidget( argExportImage );
            argExportImage->ExportFileName->setUrl( sf->argVal(0) );
            bool ok(false);
            int w=0, h=0;
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
            if ( sf->argVal(0) == xi18n( "true" ) ) argPrintImage->UsePrintDialog->setChecked( true );
            else argPrintImage->UsePrintDialog->setChecked( false );
            if ( sf->argVal(1) == xi18n( "true" ) ) argPrintImage->UseChartColors->setChecked( true );
            else argPrintImage->UseChartColors->setChecked( false );

        } else if ( sf->name() == "setLocalTime" ) {
            sb->ArgStack->setCurrentWidget( argSetLocalTime );
            bool ok(false);
            int year=0, month=0, day=0, hour=0, min=0, sec=0;

            year = sf->argVal(0).toInt(&ok);
            if (ok) month = sf->argVal(1).toInt(&ok);
            if (ok) day   = sf->argVal(2).toInt(&ok);
            if (ok) argSetLocalTime->DateWidget->setDate( QDate( year, month, day ) );
            else argSetLocalTime->DateWidget->setDate( QDate::currentDate() );

            hour = sf->argVal(3).toInt(&ok);
            if ( sf->argVal(3).isEmpty() ) ok = false;
            if (ok) min = sf->argVal(4).toInt(&ok);
            if (ok) sec = sf->argVal(5).toInt(&ok);
            if (ok) argSetLocalTime->TimeBox->setTime( QTime( hour, min, sec ) );
            else argSetLocalTime->TimeBox->setTime( QTime( QTime::currentTime() ) );

        } else if ( sf->name() == "waitFor" ) {
            sb->ArgStack->setCurrentWidget( argWaitFor );
            bool ok(false);
            int sec = sf->argVal(0).toInt(&ok);
            if (ok) argWaitFor->DelayBox->setValue( sec );
            else argWaitFor->DelayBox->setValue( 0 );

        } else if ( sf->name() == "waitForKey" ) {
            sb->ArgStack->setCurrentWidget( argWaitForKey );
            if ( sf->argVal(0).length()==1 || sf->argVal(0).toLower() == "space" )
                argWaitForKey->WaitKeyEdit->setText( sf->argVal(0) );
            else argWaitForKey->WaitKeyEdit->setText( QString() );

        } else if ( sf->name() == "setTracking" ) {
            sb->ArgStack->setCurrentWidget( argSetTracking );
            if ( sf->argVal(0) == xi18n( "true" ) ) argSetTracking->CheckTrack->setChecked( true  );
            else argSetTracking->CheckTrack->setChecked( false );

        } else if ( sf->name() == "changeViewOption" ) {
            sb->ArgStack->setCurrentWidget( argChangeViewOption );
            argChangeViewOption->OptionName->setCurrentIndex(
                argChangeViewOption->OptionName->findText( sf->argVal(0) ) );
            argChangeViewOption->OptionValue->setText( sf->argVal(1) );

        } else if ( sf->name() == "setGeoLocation" ) {
            sb->ArgStack->setCurrentWidget( argSetGeoLocation );
            argSetGeoLocation->CityName->setText( sf->argVal(0) );
            argSetGeoLocation->ProvinceName->setText( sf->argVal(1) );
            argSetGeoLocation->CountryName->setText( sf->argVal(2) );

        } else if ( sf->name() == "setColor" ) {
            sb->ArgStack->setCurrentWidget( argSetColor );
            if ( sf->argVal(0).isEmpty() ) sf->setArg( 0, "SkyColor" );  //initialize default value
            argSetColor->ColorName->setCurrentIndex(
                argSetColor->ColorName->findText(
                    ks->data()->colorScheme()->nameFromKey( sf->argVal(0) ) )
            );
            argSetColor->ColorValue->setColor( QColor( sf->argVal(1).remove('\\') ) );

        } else if ( sf->name() == "loadColorScheme" ) {
            sb->ArgStack->setCurrentWidget( argLoadColorScheme );
            argLoadColorScheme->SchemeList->setCurrentItem( argLoadColorScheme->SchemeList->findItems( sf->argVal(0).remove('\"'), Qt::MatchExactly ).at(0) );

        } else if ( sf->name() == "stop" ) {
            sb->ArgStack->setCurrentWidget( argBlank );
            //no Args

        } else if ( sf->name() == "start" ) {
            sb->ArgStack->setCurrentWidget( argBlank );
            //no Args

        } else if ( sf->name() == "setClockScale" ) {
            sb->ArgStack->setCurrentWidget( argTimeScale );
            bool ok(false);
            double ts = sf->argVal(0).toDouble(&ok);
            if (ok) argTimeScale->TimeScale->tsbox()->changeScale( float(ts) );
            else argTimeScale->TimeScale->tsbox()->changeScale( 0.0 );

        }

        //TODO JM: INDI Scripting to be included in KDE 4.1
		#if 0
        else if (sf->name() == "startINDI") {
            sb->ArgStack->setCurrentWidget( argStartINDI);

            argStartINDI->deviceName->setText(sf->argVal(0));
            if (sf->argVal(1) == "true")
                argStartINDI->LocalButton->setChecked(true);
            else if (! sf->argVal(1).isEmpty())
                argStartINDI->LocalButton->setChecked(false);
        }
        else if (sf->name() == "setINDIDevice")
        {
            sb->ArgStack->setCurrentWidget( argSetDeviceINDI);
            argSetDeviceINDI->deviceName->setText(sf->argVal(0));
        }
        else if (sf->name() == "shutdownINDI") {
            sb->ArgStack->setCurrentWidget( argShutdownINDI);
        }
        else if (sf->name() == "switchINDI") {
            sb->ArgStack->setCurrentWidget( argSwitchINDI);

            if (sf->argVal(0) == "true" || sf->argVal(0).isEmpty())
                argSwitchINDI->OnButton->setChecked(true);
            else
                argSwitchINDI->OffButton->setChecked(true);
        }
        else if (sf->name() == "setINDIPort") {
            sb->ArgStack->setCurrentWidget( argSetPortINDI);

            argSetPortINDI->devicePort->setText(sf->argVal(0));


        }
        else if (sf->name() == "setINDITargetCoord") {
            bool ok(false);
            double r(0.0),d(0.0);
            dms ra(0.0);

            sb->ArgStack->setCurrentWidget( argSetTargetCoordINDI);

            ok = !sf->argVal(0).isEmpty();
            if (ok) r = sf->argVal(0).toDouble(&ok);
            else argSetTargetCoordINDI->RABox->clear();
        if (ok) { ra.setH(r); argSetTargetCoordINDI->RABox->showInHours( ra ); }

            ok = !sf->argVal(1).isEmpty();
            if (ok) d = sf->argVal(1).toDouble(&ok);
            else argSetTargetCoordINDI->DecBox->clear();
            if (ok) argSetTargetCoordINDI->DecBox->showInDegrees( dms(d) );


        }
        else if (sf->name() == "setINDITargetName") {
            sb->ArgStack->setCurrentWidget( argSetTargetNameINDI);

            argSetTargetNameINDI->targetName->setText(sf->argVal(0));


        }
        else if (sf->name() == "setINDIAction") {
            sb->ArgStack->setCurrentWidget( argSetActionINDI);

            argSetActionINDI->actionName->setText(sf->argVal(0));


        }
        else if (sf->name() == "waitForINDIAction") {
            sb->ArgStack->setCurrentWidget( argWaitForActionINDI);

            argWaitForActionINDI->actionName->setText(sf->argVal(0));


        }
        else if (sf->name() == "setINDIFocusSpeed") {
            int t(0);
            bool ok(false);

            sb->ArgStack->setCurrentWidget( argSetFocusSpeedINDI);

            t = sf->argVal(0).toInt(&ok);
            if (ok) argSetFocusSpeedINDI->speedIN->setValue(t);
            else argSetFocusSpeedINDI->speedIN->setValue(0);


        }
        else if (sf->name() == "startINDIFocus") {
            sb->ArgStack->setCurrentWidget( argStartFocusINDI);
            bool itemSet(false);

            for (int i=0; i < argStartFocusINDI->directionCombo->count(); i++)
            {
                if (argStartFocusINDI->directionCombo->itemText(i) == sf->argVal(0))
                {
                    argStartFocusINDI->directionCombo->setCurrentIndex(i);
                    itemSet = true;
                    break;
                }
            }

            if (!itemSet) argStartFocusINDI->directionCombo->setCurrentIndex(0);


        }
        else if (sf->name() == "setINDIFocusTimeout") {
            int t(0);
            bool ok(false);

            sb->ArgStack->setCurrentWidget( argSetFocusTimeoutINDI);

            t = sf->argVal(0).toInt(&ok);
            if (ok) argSetFocusTimeoutINDI->timeOut->setValue(t);
            else argSetFocusTimeoutINDI->timeOut->setValue(0);


        }
        else if (sf->name() == "setINDIGeoLocation") {
            bool ok(false);
            double lo(0.0),la(0.0);

            sb->ArgStack->setCurrentWidget( argSetGeoLocationINDI);

            ok = !sf->argVal(0).isEmpty();
            if (ok) lo = sf->argVal(0).toDouble(&ok);
            else argSetGeoLocationINDI->longBox->clear();
        if (ok) { argSetGeoLocationINDI->longBox->showInDegrees( dms(lo) ); }

            ok = !sf->argVal(1).isEmpty();
            if (ok) la = sf->argVal(1).toDouble(&ok);
            else argSetGeoLocationINDI->latBox->clear();
            if (ok) argSetGeoLocationINDI->latBox->showInDegrees( dms(la) );

        }
        else if (sf->name() == "startINDIExposure") {
            int t(0);
            bool ok(false);

            sb->ArgStack->setCurrentWidget( argStartExposureINDI);

            t = sf->argVal(0).toInt(&ok);
            if (ok) argStartExposureINDI->timeOut->setValue(t);
            else argStartExposureINDI->timeOut->setValue(0);

        }
        else if (sf->name() == "setINDIUTC") {
            sb->ArgStack->setCurrentWidget( argSetUTCINDI);

            argSetUTCINDI->UTC->setText(sf->argVal(0));

        }
        else if (sf->name() == "setINDIScopeAction") {
            sb->ArgStack->setCurrentWidget( argSetScopeActionINDI);
            bool itemSet(false);

            for (int i=0; i < argSetScopeActionINDI->actionCombo->count(); i++)
            {
                if (argSetScopeActionINDI->actionCombo->itemText(i) == sf->argVal(0))
                {
                    argSetScopeActionINDI->actionCombo->setCurrentIndex(i);
                    itemSet = true;
                    break;
                }
            }

            if (!itemSet) argSetScopeActionINDI->actionCombo->setCurrentIndex(0);

        }
        else if (sf->name() == "setINDIFrameType") {
            sb->ArgStack->setCurrentWidget( argSetFrameTypeINDI);
            bool itemSet(false);

            for (int i=0; i < argSetFrameTypeINDI->typeCombo->count(); i++)
            {
                if (argSetFrameTypeINDI->typeCombo->itemText(i) == sf->argVal(0))
                {
                    argSetFrameTypeINDI->typeCombo->setCurrentIndex(i);
                    itemSet = true;
                    break;
                }
            }

            if (!itemSet) argSetFrameTypeINDI->typeCombo->setCurrentIndex(0);

        }
        else if (sf->name() == "setINDICCDTemp") {
            int t(0);
            bool ok(false);

            sb->ArgStack->setCurrentWidget( argSetCCDTempINDI);

            t = sf->argVal(0).toInt(&ok);
            if (ok) argSetCCDTempINDI->temp->setValue(t);
            else argSetCCDTempINDI->temp->setValue(0);

        }
        else if (sf->name() == "setINDIFilterNum") {
            int t(0);
            bool ok(false);

            sb->ArgStack->setCurrentWidget( argSetFilterNumINDI);

            t = sf->argVal(0).toInt(&ok);
            if (ok) argSetFilterNumINDI->filter_num->setValue(t);
            else argSetFilterNumINDI->filter_num->setValue(0);

        }
		 #endif

        checkForChanges = true; //signal unsaved changes if the argument widgets are changed
    }
}

void ScriptBuilder::slotShowDoc() {
    ScriptFunction *sc = NULL, *found= NULL;
    QTreeWidgetItem *currentItem = sb->FunctionTree->currentItem();

    if ( currentItem == NULL || currentItem->parent() == 0)
        return;

    foreach ( sc, KStarsFunctionList )
    if (sc->prototype() == currentItem->text(0))
    {
        found = sc;
        break;
    }

    foreach ( sc, SimClockFunctionList )
    if (sc->prototype() == currentItem->text(0))
    {
        found = sc;
        break;
    }

#if 0
    if (found == NULL)
    {
        foreach (sc, INDIFunctionList )
        if (sc->prototype() == currentItem->text(0))
        {
            found = sc;
            break;
        }
    }
#endif

    if (found == NULL)
    {
        sb->AddButton->setEnabled( false );
        qWarning() << xi18n( "Function index out of bounds." ) ;
        return;
    }

    sb->AddButton->setEnabled( true );
    sb->FuncDoc->setHtml( found->description() );
}

//Slots for Arg Widgets
void ScriptBuilder::slotFindCity() {
    QPointer<LocationDialog> ld = new LocationDialog( this );

    if ( ld->exec() == QDialog::Accepted ) {
        if ( ld->selectedCity() ) {
            // set new location names
            argSetGeoLocation->CityName->setText( ld->selectedCityName() );
            if ( ! ld->selectedProvinceName().isEmpty() ) {
                argSetGeoLocation->ProvinceName->setText( ld->selectedProvinceName() );
            } else {
                argSetGeoLocation->ProvinceName->clear();
            }
            argSetGeoLocation->CountryName->setText( ld->selectedCountryName() );

            ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];
            if ( sf->name() == "setGeoLocation" ) {
                setUnsavedChanges( true );

                sf->setArg( 0, ld->selectedCityName() );
                sf->setArg( 1, ld->selectedProvinceName() );
                sf->setArg( 2, ld->selectedCountryName() );
            } else {
                warningMismatch( "setGeoLocation" );
            }
        }
    }
    delete ld;
}

void ScriptBuilder::slotFindObject() {
    QPointer<FindDialog> fd = new FindDialog( ks );

    if ( fd->exec() == QDialog::Accepted && fd->selectedObject() ) {
        setUnsavedChanges( true );

        argLookToward->FocusEdit->setEditText( fd->selectedObject()->name() );
    }
    delete fd;
}

//TODO JM: INDI Scripting to be included in KDE 4.1

#if 0
void ScriptBuilder::slotINDIFindObject() {
    FindDialog fd( ks );

    if ( fd.exec() == QDialog::Accepted && fd.selectedObject() ) {
        setUnsavedChanges( true );

        argSetTargetNameINDI->targetName->setText( fd.selectedObject()->name() );
    }
}

void ScriptBuilder::slotINDIWaitCheck(bool /*toggleState*/)
{

    setUnsavedChanges(true);

}
#endif

void ScriptBuilder::slotShowOptions() {
    //Show tree-view of view options
    if ( otv->exec() == QDialog::Accepted ) {
        argChangeViewOption->OptionName->setCurrentIndex(
            argChangeViewOption->OptionName->findText(
                otv->optionsList()->currentItem()->text(0) )
        );
    }
}

void ScriptBuilder::slotLookToward() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "lookTowards" ) {
        setUnsavedChanges( true );

        sf->setArg( 0, argLookToward->FocusEdit->currentText() );
        sf->setValid(true);
    } else {
        warningMismatch( "lookTowards" );
    }
}

void ScriptBuilder::slotArgFindObject() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "addLabel" || sf->name() == "removeLabel" || sf->name() == "addTrail" || sf->name() == "removeTrail" ) {
        setUnsavedChanges( true );

        sf->setArg( 0, argFindObject->NameEdit->text() );
        sf->setValid(true);
    } else {
        warningMismatch( sf->name() );
    }
}

void ScriptBuilder::slotRa() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setRaDec" ) {
        //do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
        if ( argSetRaDec->RABox->text().isEmpty() ) return;

        bool ok(false);
        dms ra = argSetRaDec->RABox->createDms(false, &ok);
        if ( ok ) {
            setUnsavedChanges( true );

            sf->setArg( 0, QString( "%1" ).arg( ra.Hours() ) );
            if ( ! sf->argVal(1).isEmpty() ) sf->setValid( true );

        } else {
            sf->setArg( 0, QString() );
            sf->setValid( false );
        }
    } else {
        warningMismatch( "setRaDec" );
    }
}

void ScriptBuilder::slotDec() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setRaDec" ) {
        //do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
        if ( argSetRaDec->DecBox->text().isEmpty() ) return;

        bool ok(false);
        dms dec = argSetRaDec->DecBox->createDms(true, &ok);
        if ( ok ) {
            setUnsavedChanges( true );

            sf->setArg( 1, QString( "%1" ).arg( dec.Degrees() ) );
            if ( ! sf->argVal(0).isEmpty() ) sf->setValid( true );

        } else {
            sf->setArg( 1, QString() );
            sf->setValid( false );
        }
    } else {
        warningMismatch( "setRaDec" );
    }
}

void ScriptBuilder::slotAz() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setAltAz" ) {
        //do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
        if ( argSetAltAz->AzBox->text().isEmpty() ) return;

        bool ok(false);
        dms az = argSetAltAz->AzBox->createDms(true, &ok);
        if ( ok ) {
            setUnsavedChanges( true );
            sf->setArg( 1, QString( "%1" ).arg( az.Degrees() ) );
            if ( ! sf->argVal(0).isEmpty() ) sf->setValid( true );
        } else {
            sf->setArg( 1, QString() );
            sf->setValid( false );
        }
    } else {
        warningMismatch( "setAltAz" );
    }
}

void ScriptBuilder::slotAlt() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setAltAz" ) {
        //do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
        if ( argSetAltAz->AltBox->text().isEmpty() ) return;

        bool ok(false);
        dms alt = argSetAltAz->AltBox->createDms(true, &ok);
        if ( ok ) {
            setUnsavedChanges( true );

            sf->setArg( 0, QString( "%1" ).arg( alt.Degrees() ) );
            if ( ! sf->argVal(1).isEmpty() ) sf->setValid( true );
        } else {
            sf->setArg( 0, QString() );
            sf->setValid( false );
        }
    } else {
        warningMismatch( "setAltAz" );
    }
}

void ScriptBuilder::slotChangeDate() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setLocalTime" ) {
        setUnsavedChanges( true );

        QDate date = argSetLocalTime->DateWidget->date();

        sf->setArg( 0, QString( "%1" ).arg( date.year()   ) );
        sf->setArg( 1, QString( "%1" ).arg( date.month()  ) );
        sf->setArg( 2, QString( "%1" ).arg( date.day()    ) );
        if ( ! sf->argVal(3).isEmpty() ) sf->setValid( true );
    } else {
        warningMismatch( "setLocalTime" );
    }
}

void ScriptBuilder::slotChangeTime() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setLocalTime" ) {
        setUnsavedChanges( true );

        QTime time = argSetLocalTime->TimeBox->time();

        sf->setArg( 3, QString( "%1" ).arg( time.hour()   ) );
        sf->setArg( 4, QString( "%1" ).arg( time.minute() ) );
        sf->setArg( 5, QString( "%1" ).arg( time.second() ) );
        if ( ! sf->argVal(0).isEmpty() ) sf->setValid( true );
    } else {
        warningMismatch( "setLocalTime" );
    }
}

void ScriptBuilder::slotWaitFor() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "waitFor" ) {
        bool ok(false);
        int delay = argWaitFor->DelayBox->text().toInt( &ok );

        if ( ok ) {
            setUnsavedChanges( true );

            sf->setArg( 0, QString( "%1" ).arg( delay ) );
            sf->setValid( true );
        } else {
            sf->setValid( false );
        }
    } else {
        warningMismatch( "waitFor" );
    }
}

void ScriptBuilder::slotWaitForKey() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "waitForKey" ) {
        QString sKey = argWaitForKey->WaitKeyEdit->text().trimmed();

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
        warningMismatch( "waitForKey" );
    }
}

void ScriptBuilder::slotTracking() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setTracking" ) {
        setUnsavedChanges( true );

        sf->setArg( 0, ( argSetTracking->CheckTrack->isChecked() ? xi18n( "true" ) : xi18n( "false" ) ) );
        sf->setValid( true );
    } else {
        warningMismatch( "setTracking" );
    }
}

void ScriptBuilder::slotViewOption() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "changeViewOption" ) {
        if ( argChangeViewOption->OptionName->currentIndex() >= 0
                && argChangeViewOption->OptionValue->text().length() ) {
            setUnsavedChanges( true );

            sf->setArg( 0, argChangeViewOption->OptionName->currentText() );
            sf->setArg( 1, argChangeViewOption->OptionValue->text() );
            sf->setValid( true );
        } else {
            sf->setValid( false );
        }
    } else {
        warningMismatch( "changeViewOption" );
    }
}

void ScriptBuilder::slotChangeCity() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setGeoLocation" ) {
        QString city =     argSetGeoLocation->CityName->text();

        if ( city.length() ) {
            setUnsavedChanges( true );

            sf->setArg( 0, city );
            if ( sf->argVal(2).length() ) sf->setValid( true );
        } else {
            sf->setArg( 0, QString() );
            sf->setValid( false );
        }
    } else {
        warningMismatch( "setGeoLocation" );
    }
}

void ScriptBuilder::slotChangeProvince() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setGeoLocation" ) {
        QString province = argSetGeoLocation->ProvinceName->text();

        if ( province.length() ) {
            setUnsavedChanges( true );

            sf->setArg( 1, province );
            if ( sf->argVal(0).length() && sf->argVal(2).length() ) sf->setValid( true );
        } else {
            sf->setArg( 1, QString() );
            //might not be invalid
        }
    } else {
        warningMismatch( "setGeoLocation" );
    }
}

void ScriptBuilder::slotChangeCountry() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setGeoLocation" ) {
        QString country =  argSetGeoLocation->CountryName->text();

        if ( country.length() ) {
            setUnsavedChanges( true );

            sf->setArg( 2, country );
            if ( sf->argVal(0).length() ) sf->setValid( true );
        } else {
            sf->setArg( 2, QString() );
            sf->setValid( false );
        }
    } else {
        warningMismatch( "setGeoLocation" );
    }
}

void ScriptBuilder::slotTimeScale() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setClockScale" ) {
        setUnsavedChanges( true );

        sf->setArg( 0, QString( "%1" ).arg( argTimeScale->TimeScale->tsbox()->timeScale() ) );
        sf->setValid( true );
    } else {
        warningMismatch( "setClockScale" );
    }
}

void ScriptBuilder::slotZoom() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "zoom" ) {
        setUnsavedChanges( true );

        bool ok(false);
        argZoom->ZoomBox->text().toDouble(&ok);
        if ( ok ) {
            sf->setArg( 0, argZoom->ZoomBox->text() );
            sf->setValid( true );
        }
    } else {
        warningMismatch( "zoom" );
    }
}

void ScriptBuilder::slotExportImage() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "exportImage" ) {
        setUnsavedChanges( true );

        sf->setArg( 0, argExportImage->ExportFileName->url().url() );
        sf->setArg( 1, QString("%1").arg( argExportImage->ExportWidth->value() ) );
        sf->setArg( 2, QString("%1").arg( argExportImage->ExportHeight->value() ) );
        sf->setValid( true );
    } else {
        warningMismatch( "exportImage" );
    }
}

void ScriptBuilder::slotPrintImage() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "printImage" ) {
        setUnsavedChanges( true );

        sf->setArg( 0, ( argPrintImage->UsePrintDialog->isChecked() ? xi18n( "true" ) : xi18n( "false" ) ) );
        sf->setArg( 1, ( argPrintImage->UseChartColors->isChecked() ? xi18n( "true" ) : xi18n( "false" ) ) );
        sf->setValid( true );
    } else {
        warningMismatch( "exportImage" );
    }
}

void ScriptBuilder::slotChangeColorName() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setColor" ) {
        setUnsavedChanges( true );

        argSetColor->ColorValue->setColor( ks->data()->colorScheme()->colorAt( argSetColor->ColorName->currentIndex() ) );
        sf->setArg( 0, ks->data()->colorScheme()->keyAt( argSetColor->ColorName->currentIndex() ) );
        QString cname( argSetColor->ColorValue->color().name() );
        //if ( cname.at(0) == '#' ) cname = "\\" + cname; //prepend a "\" so bash doesn't think we have a comment
        sf->setArg( 1, cname );
        sf->setValid( true );
    } else {
        warningMismatch( "setColor" );
    }
}

void ScriptBuilder::slotChangeColor() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setColor" ) {
        setUnsavedChanges( true );

        sf->setArg( 0, ks->data()->colorScheme()->keyAt( argSetColor->ColorName->currentIndex() ) );
        QString cname( argSetColor->ColorValue->color().name() );
        //if ( cname.at(0) == '#' ) cname = "\\" + cname; //prepend a "\" so bash doesn't think we have a comment
        sf->setArg( 1, cname );
        sf->setValid( true );
    } else {
        warningMismatch( "setColor" );
    }
}

void ScriptBuilder::slotLoadColorScheme() {
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "loadColorScheme" ) {
        setUnsavedChanges( true );

        sf->setArg( 0, '\"' + argLoadColorScheme->SchemeList->currentItem()->text() + '\"' );
        sf->setValid( true );
    } else {
        warningMismatch( "loadColorScheme" );
    }
}

void ScriptBuilder::slotClose()
{
    saveWarning();

    if ( !UnsavedChanges ) {
        ScriptList.clear();
        sb->ScriptListBox->clear();
        sb->ArgStack->setCurrentWidget( argBlank );
        close();
    }
}

//TODO JM: INDI Scripting to be included in KDE 4.1

#if 0
void ScriptBuilder::slotINDIStartDeviceName()
{
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "startINDI" )
    {
        setUnsavedChanges( true );

        sf->setArg(0, argStartINDI->deviceName->text());
        sf->setArg(1, argStartINDI->LocalButton->isChecked() ? "true" : "false");
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "startINDI" );
    }

}

void ScriptBuilder::slotINDIStartDeviceMode()
{

    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "startINDI" )
    {
        setUnsavedChanges( true );

        sf->setArg(1, argStartINDI->LocalButton->isChecked() ? "true" : "false");
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "startINDI" );
    }

}

void ScriptBuilder::slotINDISetDevice()
{

    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIDevice" )
    {
        setUnsavedChanges( true );

        sf->setArg(0, argSetDeviceINDI->deviceName->text());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "startINDI" );
    }
}

void ScriptBuilder::slotINDIShutdown()
{

    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

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
        warningMismatch( "shutdownINDI" );
    }

}

void ScriptBuilder::slotINDISwitchDeviceConnection()
{

    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "switchINDI" )
    {

        if (sf->argVal(0) != (argSwitchINDI->OnButton->isChecked() ? "true" : "false"))
            setUnsavedChanges( true );

        sf->setArg(0, argSwitchINDI->OnButton->isChecked() ? "true" : "false");
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "switchINDI" );
    }

}

void ScriptBuilder::slotINDISetPortDevicePort()
{
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIPort" )
    {

        if (argSetPortINDI->devicePort->text().isEmpty())
        {
            sf->setValid(false);
            return;
        }

        if (sf->argVal(0) != argSetPortINDI->devicePort->text())
            setUnsavedChanges( true );

        sf->setArg(0, argSetPortINDI->devicePort->text());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDIPort" );
    }

}

void ScriptBuilder::slotINDISetTargetCoordDeviceRA()
{
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDITargetCoord" ) {
        //do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
        if ( argSetTargetCoordINDI->RABox->text().isEmpty() )
        {
            sf->setValid(false);
            return;
        }

        bool ok(false);
        dms ra = argSetTargetCoordINDI->RABox->createDms(false, &ok);
        if ( ok )
        {

            if (sf->argVal(0) != QString( "%1" ).arg( ra.Hours() ))
                setUnsavedChanges( true );

            sf->setArg( 0, QString( "%1" ).arg( ra.Hours() ) );
            if ( ( ! sf->argVal(1).isEmpty() ))
                sf->setValid( true );
            else
                sf->setValid(false);

        } else
        {
            sf->setArg( 0, QString() );
            sf->setValid( false );
        }
    } else {
        warningMismatch( "setINDITargetCoord" );
    }

}

void ScriptBuilder::slotINDISetTargetCoordDeviceDEC()
{
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDITargetCoord" ) {
        //do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
        if ( argSetTargetCoordINDI->DecBox->text().isEmpty() )
        {
            sf->setValid(false);
            return;
        }

        bool ok(false);
        dms dec = argSetTargetCoordINDI->DecBox->createDms(true, &ok);
        if ( ok )
        {

            if (sf->argVal(1) != QString( "%1" ).arg( dec.Degrees() ))
                setUnsavedChanges( true );

            sf->setArg( 1, QString( "%1" ).arg( dec.Degrees() ) );
            if ( ( ! sf->argVal(0).isEmpty() ))
                sf->setValid( true );
            else
                sf->setValid(false);

        } else
        {
            sf->setArg( 1, QString() );
            sf->setValid( false );
        }
    } else {
        warningMismatch( "setINDITargetCoord" );
    }

}

void ScriptBuilder::slotINDISetTargetNameTargetName()
{

    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDITargetName" )
    {
        if (argSetTargetNameINDI->targetName->text().isEmpty())
        {
            sf->setValid(false);
            return;
        }

        if (sf->argVal(0) != argSetTargetNameINDI->targetName->text())
            setUnsavedChanges( true );

        sf->setArg(0, argSetTargetNameINDI->targetName->text());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDITargetName" );
    }

}

void ScriptBuilder::slotINDISetActionName()
{
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIAction" )
    {
        if (argSetActionINDI->actionName->text().isEmpty())
        {
            sf->setValid(false);
            return;
        }

        if (sf->argVal(0) != argSetActionINDI->actionName->text())
            setUnsavedChanges( true );

        sf->setArg(0, argSetActionINDI->actionName->text());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDIAction" );
    }

}

void ScriptBuilder::slotINDIWaitForActionName()
{
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "waitForINDIAction" )
    {
        if (argWaitForActionINDI->actionName->text().isEmpty())
        {
            sf->setValid(false);
            return;
        }

        if (sf->argVal(0) != argWaitForActionINDI->actionName->text())
            setUnsavedChanges( true );

        sf->setArg(0, argWaitForActionINDI->actionName->text());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "waitForINDIAction" );
    }

}

void ScriptBuilder::slotINDISetFocusSpeed()
{
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIFocusSpeed" )
    {

        if (sf->argVal(0).toInt() != argSetFocusSpeedINDI->speedIN->value())
            setUnsavedChanges( true );

        sf->setArg(0, QString("%1").arg(argSetFocusSpeedINDI->speedIN->value()));
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDIFocusSpeed" );
    }

}

void ScriptBuilder::slotINDIStartFocusDirection()
{
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "startINDIFocus" )
    {
        if (sf->argVal(0) != argStartFocusINDI->directionCombo->currentText())
            setUnsavedChanges( true );

        sf->setArg(0, argStartFocusINDI->directionCombo->currentText());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "startINDIFocus" );
    }

}

void ScriptBuilder::slotINDISetFocusTimeout()
{
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIFocusTimeout" )
    {
        if (sf->argVal(0).toInt() != argSetFocusTimeoutINDI->timeOut->value())
            setUnsavedChanges( true );

        sf->setArg(0, QString("%1").arg(argSetFocusTimeoutINDI->timeOut->value()));
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDIFocusTimeout" );
    }

}

void ScriptBuilder::slotINDISetGeoLocationDeviceLong()
{
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

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

            if (sf->argVal(0) != QString( "%1" ).arg( longitude.Degrees()))
                setUnsavedChanges( true );

            sf->setArg( 0, QString( "%1" ).arg( longitude.Degrees() ) );
            if ( ! sf->argVal(1).isEmpty() )
                sf->setValid( true );
            else
                sf->setValid(false);

        } else
        {
            sf->setArg( 0, QString() );
            sf->setValid( false );
        }
    } else {
        warningMismatch( "setINDIGeoLocation" );
    }

}

void ScriptBuilder::slotINDISetGeoLocationDeviceLat()
{
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

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

            if (sf->argVal(1) != QString( "%1" ).arg( latitude.Degrees()))
                setUnsavedChanges( true );

            sf->setArg( 1, QString( "%1" ).arg( latitude.Degrees() ) );
            if ( ! sf->argVal(0).isEmpty() )
                sf->setValid( true );
            else
                sf->setValid(false);

        } else
        {
            sf->setArg( 1, QString() );
            sf->setValid( false );
        }
    } else {
        warningMismatch( "setINDIGeoLocation" );
    }

}

void ScriptBuilder::slotINDIStartExposureTimeout()
{
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "startINDIExposure" )
    {

        if (sf->argVal(0).toInt() != argStartExposureINDI->timeOut->value())
            setUnsavedChanges( true );

        sf->setArg(0, QString("%1").arg(argStartExposureINDI->timeOut->value()));
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "startINDIExposure" );
    }

}

void ScriptBuilder::slotINDISetUTC()
{
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIUTC" )
    {

        if (argSetUTCINDI->UTC->text().isEmpty())
        {
            sf->setValid(false);
            return;
        }

        if (sf->argVal(0) != argSetUTCINDI->UTC->text())
            setUnsavedChanges( true );

        sf->setArg(0, argSetUTCINDI->UTC->text());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDIUTC" );
    }

}

void ScriptBuilder::slotINDISetScopeAction()
{
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIScopeAction" )
    {

        if (sf->argVal(0) != argSetScopeActionINDI->actionCombo->currentText())
            setUnsavedChanges( true );

        sf->setArg(0, argSetScopeActionINDI->actionCombo->currentText());
        sf->setINDIProperty("CHECK");
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDIScopeAction" );
    }

}

void ScriptBuilder::slotINDISetFrameType()
{
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIFrameType" )
    {

        if (sf->argVal(0) != argSetFrameTypeINDI->typeCombo->currentText())
            setUnsavedChanges( true );

        sf->setArg(0, argSetFrameTypeINDI->typeCombo->currentText());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDIFrameType" );
    }

}

void ScriptBuilder::slotINDISetCCDTemp()
{
    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDICCDTemp" )
    {

        if (sf->argVal(0).toInt() != argSetCCDTempINDI->temp->value())
            setUnsavedChanges( true );

        sf->setArg(0, QString("%1").arg(argSetCCDTempINDI->temp->value()));
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDICCDTemp" );
    }

}

void ScriptBuilder::slotINDISetFilterNum()
{

    ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIFilterNum" )
    {

        if (sf->argVal(0).toInt() != argSetFilterNumINDI->filter_num->value())
            setUnsavedChanges( true );

        sf->setArg(0, QString("%1").arg(argSetFilterNumINDI->filter_num->value()));
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDIFilterNum" );
    }


}
#endif

void ScriptBuilder::warningMismatch (const QString &expected) const {
    qWarning() << xi18n( "Mismatch between function and Arg widget (expected %1.)", QString(expected) ) ;
}


