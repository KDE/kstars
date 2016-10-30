/***************************************************************************
                          focusdialog.h  -  description
                             -------------------
    begin                : Sat Mar 23 2002
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

#ifndef FOCUSDIALOG_H_
#define FOCUSDIALOG_H_

#include <QDialog>

#include "ui_focusdialog.h"

#include "skyobjects/skypoint.h"
class KStars;

class FocusDialogUI : public QFrame, public Ui::FocusDialog {
    Q_OBJECT
public:
    explicit FocusDialogUI( QWidget *parent=0 );
};


/** @class FocusDialog
	*@short A small dialog for setting the focus coordinates manually.
	*@author Jason Harris
	*@version 1.0
	*/
class FocusDialog : public QDialog {
    Q_OBJECT
public:
    /**Constructor. */
    explicit FocusDialog();

    /**Destructor (empty). */
    ~FocusDialog();

    /** @return pointer to the SkyPoint described by the entered RA, Dec */
    inline SkyPoint* point() { return Point; }

    /** @return suggested size of focus window. */
    QSize sizeHint() const;

    /** @return whether user set the AltAz coords */
    inline bool usedAltAz() const { return UsedAltAz; }

    /**
      *@short Show the Az/Alt page instead of the RA/Dec page.
    	*/
    void activateAzAltPage() const;

public slots:
    /**If text has been entered in both KLineEdits, enable the Ok button. */
    void checkLineEdits();

    /**Attempt to interpret the text in the KLineEdits as Ra and Dec values.
    	*If the point is validated, close the window.
    	*/
    void validatePoint();

private:
    SkyPoint *Point;
    FocusDialogUI *fd;
    bool UsedAltAz;
    QPushButton *okB;
};

#endif
