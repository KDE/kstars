/***************************************************************************
                          kstarsmessagebox.h  -  description
                             -------------------
    begin                : Mon Mar 11 2002
    copyright            : (C) 2002 by Jason Harris
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

#ifndef KSTARSMESSAGEBOX_H
#define KSTARSMESSAGEBOX_H
#include <qstring.h>

class QWidget;
class QStringList;

/**This class contains only static member functions for creating customized
  *warning message boxes.  At this point, there is only one kind of box:
  *badCatalog.  This is nearly identical to
  *KMessageBox::warningContinueCancelList, except it only has one button,
  *instead of two.
	*@short slightly modified KMessageBox
  *@author Jason Harris
	*@version 0.9
  */

class KStarsMessageBox {
public: 

enum { Ok = 1, Cancel = 2, Yes = 3, No = 4, Continue = 5 };

static int badCatalog(QWidget *parent,
                         const QString &text,
                         const QStringList &strlist,
                         const QString &caption = QString::null,
                         const QString &dontAskAgainName=QString::null,
                         bool notify=true);
};

#endif
