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

#include <qwidget.h>
#include <qptrlist.h>
#include <qlayout.h>
#include <kurl.h>

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

class KStars;
class QListViewItem;

/**
 *
 * Jason Harris
 **/
class ScriptBuilder : public KDialogBase
{
Q_OBJECT
public:
	ScriptBuilder( QWidget *parent, const char *name=0 );
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

	void slotEnableScriptNameOK();

private:
	void initViewOptions();

	ScriptBuilderUI *sb;

	KStars *ks; //parent needed for sub-dialogs
	QPtrList<ScriptFunction> FunctionList;
	QPtrList<ScriptFunction> ScriptList;
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

	ScriptNameDialog *snd;
	OptionsTreeView *otv;

	QListViewItem *opsGUI, *opsToolbar, *opsShowObj, *opsShowOther, *opsCName, *opsHide, *opsSkymap, *opsLimit;

	bool UnsavedChanges;
	KURL currentFileURL;
	QString currentDir;
	QString currentScriptName, currentAuthor;
};

#endif
