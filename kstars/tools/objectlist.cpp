/***************************************************************************
                          observinglist.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 29 Nov 2004
    copyright            : (C) 2004 by Jeff Woods, Jason Harris
    email                : jcwoods@bellsouth.net, jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "objectlist.h"

#include <stdio.h>

#include <QFile>
#include <QDir>
#include <QFrame>
#include <QTextStream>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QDirIterator>

#include <kpushbutton.h>
#include <kstatusbar.h>
#include <ktextedit.h>
#include <kinputdialog.h>
#include <kicon.h>
#include <kio/netaccess.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <ktemporaryfile.h>
#include <klineedit.h>
#include <kplotobject.h>
#include <kplotaxis.h>
#include <kplotwidget.h>
#include <kio/copyjob.h>
#include <kstandarddirs.h>

#include "ksalmanac.h"
#include "obslistwizard.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "dialogs/locationdialog.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/starobject.h"
#include "skycomponents/skymapcomposite.h"
#include "skymap.h"
#include "dialogs/detaildialog.h"
#include "dialogs/finddialog.h"
#include "tools/altvstime.h"
#include "tools/wutdialog.h"
#include "Options.h"
#include "imageviewer.h"
#include "thumbnailpicker.h"
#include "obslistpopupmenu.h"
#include "oal/log.h"
#include "oal/oal.h"
#include "oal/execute.h"

#include <config-kstars.h>

#ifdef HAVE_INDI_H
#include "indi/indimenu.h"
#include "indi/indielement.h"
#include "indi/indiproperty.h"
#include "indi/indidevice.h"
#include "indi/devicemanager.h"
#include "indi/indistd.h"
#endif

//
// ObjectListUI
// ---------------------------------
ObjectListUI::ObjectListUI( QWidget *p ) : QFrame( p ) {
    setupUi( this );
}

//
// ObservingList
// ---------------------------------
ObjectList::ObjectList( KStars *_ks )
        : KDialog( (QWidget*)_ks ),
        ks( _ks )
{
    ui = new ObjectListUI( this );
    setMainWidget( ui );
    setCaption( i18n( "Object List" ) );
    setButtons( KDialog::Close );
}

ObjectList::~ObjectList()
{
}

#include "objectlist.moc"
