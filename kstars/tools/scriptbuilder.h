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

#ifndef SCRIPTBUILDER_H_
#define SCRIPTBUILDER_H_

#include <QDialog>

#include "ui_scriptbuilder.h"
#include "ui_scriptnamedialog.h"
#include "ui_optionstreeview.h"
#include "scriptargwidgets.h"

class QTreeWidget;
class QTextStream;
class QVBoxLayout;
class QUrl;

class KStars;
class ScriptFunction;

class OptionsTreeViewWidget : public QFrame, public Ui::OptionsTreeView {
    Q_OBJECT
public:
    explicit OptionsTreeViewWidget( QWidget *p );
};

class OptionsTreeView : public QDialog {
    Q_OBJECT
public:
    explicit OptionsTreeView( QWidget *p );
    ~OptionsTreeView();
    QTreeWidget* optionsList() { return otvw->OptionsList; }
    void resizeColumns();

private:
    OptionsTreeViewWidget *otvw;
};

class ScriptNameWidget : public QFrame, public Ui::ScriptNameDialog {
    Q_OBJECT
public:
    explicit ScriptNameWidget( QWidget *p );
};

class ScriptNameDialog : public QDialog {
    Q_OBJECT
public:
    explicit ScriptNameDialog( QWidget *p );
    ~ScriptNameDialog();
    QString scriptName() const { return snw->ScriptName->text(); }
    QString authorName() const { return snw->AuthorName->text(); }

private slots:
    void slotEnableOkButton();

private:
    ScriptNameWidget *snw;
    QPushButton *okB;
};

class ScriptBuilderUI : public QFrame, public Ui::ScriptBuilder {
    Q_OBJECT
public:
    explicit ScriptBuilderUI( QWidget *p );
};

/**@class ScriptBuilder
	*A GUI tool for building behavioral DBus scripts for KStars.
	*@author Jason Harris
	*@version 1.0
	*/
class ScriptBuilder : public QDialog {
    Q_OBJECT
public:
    explicit ScriptBuilder( QWidget *parent );
    ~ScriptBuilder();

    bool unsavedChanges() const { return UnsavedChanges; }
    void setUnsavedChanges( bool b=true );
    void saveWarning();
    void readScript( QTextStream &istream );
    void writeScript( QTextStream &ostream );
    bool parseFunction( QString fn_name, QStringList &fn );

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
    void slotArgFindObject();
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
    void slotLoadColorScheme();

	#if 0
    void slotINDIWaitCheck(bool toggleState);
    void slotINDIFindObject();
    void slotINDIStartDeviceName();
    void slotINDIStartDeviceMode();
    void slotINDISetDevice();
    void slotINDIShutdown();
    void slotINDISwitchDeviceConnection();
    void slotINDISetPortDevicePort();
    void slotINDISetTargetCoordDeviceRA();
    void slotINDISetTargetCoordDeviceDEC();
    void slotINDISetTargetNameTargetName();
    void slotINDISetActionName();
    void slotINDIWaitForActionName();
    void slotINDISetFocusSpeed();
    void slotINDIStartFocusDirection();
    void slotINDISetFocusTimeout();
    void slotINDISetGeoLocationDeviceLong();
    void slotINDISetGeoLocationDeviceLat();
    void slotINDIStartExposureTimeout();
    void slotINDISetUTC();
    void slotINDISetScopeAction();
    void slotINDISetFrameType();
    void slotINDISetCCDTemp();
    void slotINDISetFilterNum();
	#endif

private:
    void initViewOptions();
    void warningMismatch (const QString &expected) const;

    ScriptBuilderUI *sb;

    KStars *ks; //parent needed for sub-dialogs
    QList<ScriptFunction*> KStarsFunctionList;
    QList<ScriptFunction*> SimClockFunctionList;

	#if 0
    QList<ScriptFunction*> INDIFunctionList;
	#endif

    QList<ScriptFunction*> ScriptList;

    QWidget *argBlank;
    ArgLookToward *argLookToward;
    ArgFindObject *argFindObject;
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

	#if 0
    ArgStartINDI *argStartINDI;
    ArgSetDeviceINDI *argSetDeviceINDI;
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
	#endif

    ScriptNameDialog *snd;
    OptionsTreeView *otv;

    QTreeWidgetItem *opsGUI, *opsToolbar, *opsShowObj, *opsShowOther, *opsCName, *opsHide, *opsSkymap, *opsLimit;

    bool UnsavedChanges;
    bool checkForChanges;
    QUrl currentFileURL;
    QString currentDir;
    QString currentScriptName, currentAuthor;
};

#endif
