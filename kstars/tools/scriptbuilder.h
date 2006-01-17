/***************************************************************************
                          scriptbuilder.h  -  description
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

#ifndef SCRIPTBUILDER_H
#define SCRIPTBUILDER_H

#include <kdialogbase.h>

#include "scriptbuilderui.h"
#include "scriptnamedialog.h"
#include "scriptargwidgets.h"
#include "optionstreeview.h"

class QTreeWidget;
class QTextStream;
class QVBoxLayout;
class KURL;

class KStars;
class ScriptFunction;

class OptionsTreeViewWidget : public QFrame, public Ui::OptionsTreeView {
Q_OBJECT
public:
	OptionsTreeViewWidget( QWidget *p );
};

class OptionsTreeView : public KDialogBase {
Q_OBJECT
public:
	OptionsTreeView( QWidget *p );
	~OptionsTreeView();
	QTreeWidget* optionsList() { return otvw->OptionsList; }
 
private:
	OptionsTreeViewWidget *otvw;
};

class ScriptNameWidget : public QFrame, public Ui::ScriptNameDialog {
Q_OBJECT
public:
	ScriptNameWidget( QWidget *p );
};

class ScriptNameDialog : public KDialogBase {
Q_OBJECT
public:
	ScriptNameDialog( QWidget *p );
	~ScriptNameDialog();
	QString scriptName() const { return snw->ScriptName->text(); }
	QString authorName() const { return snw->AuthorName->text(); }
 
private slots: 
	void slotEnableOkButton();

private:
	ScriptNameWidget *snw;
};

class ScriptBuilderUI : public QFrame, public Ui::ScriptBuilder {
Q_OBJECT
public:
	ScriptBuilderUI( QWidget *p );
};

/**@class ScriptBuilder
	*A GUI tool for building behavioral DCOP scripts for KStars.
	*@author Jason Harris
	*@version 1.0
	*/
class ScriptBuilder : public KDialogBase {
Q_OBJECT
public:
	ScriptBuilder( QWidget *parent );
	~ScriptBuilder();

	bool unsavedChanges() const { return UnsavedChanges; }
	void setUnsavedChanges( bool b=true );
	void saveWarning();
	void readScript( QTextStream &istream );
	void writeScript( QTextStream &ostream );
	bool parseFunction( QStringList &fn );

public slots:
	void slotAddFunction();
	void slotMoveFunctionUp();
	void slotMoveFunctionDown();
	void slotArgWidget();
	void slotShowDoc();

	void slotNew();
	void slotOpen();
	void slotSave();
	void slotSaveAs();
	void slotRunScript();
	void slotClose();

	void slotCopyFunction();
	void slotRemoveFunction();

	void slotFindCity();
	void slotFindObject();
	void slotShowOptions();
	void slotLookToward();
	void slotRa();
	void slotDec();
	void slotAz();
	void slotAlt();
	void slotChangeDate();
	void slotChangeTime();
	void slotWaitFor();
	void slotWaitForKey();
	void slotTracking();
	void slotViewOption();
	void slotChangeCity();
	void slotChangeProvince();
	void slotChangeCountry();
	void slotTimeScale();
	void slotZoom();
	void slotExportImage();
	void slotPrintImage();
	void slotChangeColor();
	void slotChangeColorName();
	void slotLoadColorScheme(QListWidgetItem*);
	
	void slotINDIWaitCheck(bool toggleState);
	void slotINDIFindObject();
	void slotINDIStartDeviceName();
	void slotINDIStartDeviceMode();
	void slotINDIShutdown();
	void slotINDISwitchDeviceName();
	void slotINDISwitchDeviceConnection();
	void slotINDISetPortDeviceName();
	void slotINDISetPortDevicePort();
	void slotINDISetTargetCoordDeviceName();
	void slotINDISetTargetCoordDeviceRA();
	void slotINDISetTargetCoordDeviceDEC();
	void slotINDISetTargetNameDeviceName();
	void slotINDISetTargetNameTargetName();
	void slotINDISetActionDeviceName();
	void slotINDISetActionName();
	void slotINDIWaitForActionDeviceName();
	void slotINDIWaitForActionName();
	void slotINDISetFocusSpeedDeviceName();
	void slotINDISetFocusSpeed();
	void slotINDIStartFocusDeviceName();
	void slotINDIStartFocusDirection();
	void slotINDISetFocusTimeoutDeviceName();
	void slotINDISetFocusTimeout();
	void slotINDISetGeoLocationDeviceName();
	void slotINDISetGeoLocationDeviceLong();
	void slotINDISetGeoLocationDeviceLat();
	void slotINDIStartExposureDeviceName();
	void slotINDIStartExposureTimeout();
	void slotINDISetUTCDeviceName();
	void slotINDISetUTC();
	void slotINDISetScopeActionDeviceName();
	void slotINDISetScopeAction();
	void slotINDISetFrameTypeDeviceName();
	void slotINDISetFrameType();
	void slotINDISetCCDTempDeviceName();
	void slotINDISetCCDTemp();
	void slotINDISetFilterNumDeviceName();
	void slotINDISetFilterNum();

private:
	void initViewOptions();

	ScriptBuilderUI *sb;

	KStars *ks; //parent needed for sub-dialogs
	QList<ScriptFunction*> KStarsFunctionList;
	QList<ScriptFunction*> INDIFunctionList;
	QList<ScriptFunction*> ScriptList;
	QVBoxLayout *vlay;

	QWidget *argBlank;
	ArgLookToward *argLookToward;
	ArgSetRaDec *argSetRaDec;
	ArgSetAltAz *argSetAltAz;
	ArgSetLocalTime *argSetLocalTime;
	ArgWaitFor *argWaitFor;
	ArgWaitForKey *argWaitForKey;
	ArgSetTrack *argSetTracking;
	ArgChangeViewOption *argChangeViewOption;
	ArgSetGeoLocation *argSetGeoLocation;
	ArgTimeScale *argTimeScale;
	ArgZoom *argZoom;
	ArgExportImage *argExportImage;
	ArgPrintImage *argPrintImage;
	ArgSetColor *argSetColor;
	ArgLoadColorScheme *argLoadColorScheme;
	ArgStartINDI *argStartINDI;
	ArgShutdownINDI *argShutdownINDI;
	ArgSwitchINDI *argSwitchINDI;
	ArgSetPortINDI *argSetPortINDI;
	ArgSetTargetCoordINDI *argSetTargetCoordINDI;
	ArgSetTargetNameINDI *argSetTargetNameINDI;
	ArgSetActionINDI *argSetActionINDI;
	ArgSetActionINDI *argWaitForActionINDI;
	ArgSetFocusSpeedINDI *argSetFocusSpeedINDI;
	ArgStartFocusINDI *argStartFocusINDI;
	ArgSetFocusTimeoutINDI *argSetFocusTimeoutINDI;
	ArgSetGeoLocationINDI *argSetGeoLocationINDI;
	ArgStartExposureINDI *argStartExposureINDI;
	ArgSetUTCINDI *argSetUTCINDI;
	ArgSetScopeActionINDI *argSetScopeActionINDI;
	ArgSetFrameTypeINDI *argSetFrameTypeINDI;
	ArgSetCCDTempINDI *argSetCCDTempINDI;
        ArgSetFilterNumINDI *argSetFilterNumINDI;
	
	ScriptNameDialog *snd;
	OptionsTreeView *otv;

	QTreeWidgetItem *opsGUI, *opsToolbar, *opsShowObj, *opsShowOther, *opsCName, *opsHide, *opsSkymap, *opsLimit;

	bool UnsavedChanges;
	KURL currentFileURL;
	QString currentDir;
	QString currentScriptName, currentAuthor;
	QString lastINDIDeviceName;
};

#endif
