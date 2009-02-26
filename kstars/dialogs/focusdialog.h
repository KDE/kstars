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

#include <kdialog.h>

#include "ui_focusdialog.h"

#include "skyobjects/skypoint.h"
class KStars;

class FocusDialogUI : public QFrame, public Ui::FocusDialog {
    Q_OBJECT
public:
    FocusDialogUI( QWidget *parent=0 );
};


/**@class FocusDialog
	*@short A small dialog for setting the focus coordinates manually.
	*@author Jason Harris
	*@version 1.0
	*/
class FocusDialog : public KDialog {
    Q_OBJECT
public:
    /**Constructor. */
    FocusDialog( KStars *_ks );

    /**Destructor (empty). */
    ~FocusDialog();

    /**@return pointer to the SkyPoint described by the entered RA, Dec */
    inline SkyPoint& point() { return Point; }

    /**@return suggested size of focus window. */
    QSize sizeHint() const;

    /**@return whether user set the AltAz coords */
    inline bool usedAltAz() const { return UsedAltAz; }

    /**
      *@short Show the Az/Alt page instead of the RA/Dec page.
    	*/
    void activateAzAltPage() const;

    /**
      *@short Convenience function to convert an epoch number (e.g., 2000.0) 
    	*to the corresponding Julian Day number (e.g., 2451545.0).
    	*@param epoch the epoch value to be converted.
    	*FIXME: This should probably move to KStarsDateTime
    	*/
    long double epochToJd (double epoch);

    /**
      *@short Convert a string to an epoch number; essentially just 
    	*converts the string to a double.
    	*@param eName the tring representation of the epoch number.
    	*@return the epoch number described by the string argument.
    	*FIXME: This should probably move to KStarsDateTime
    	*/
    double getEpoch (const QString &eName);

public slots:
    /**If text has been entered in both KLineEdits, enable the Ok button. */
    void checkLineEdits();

    /**Attempt to interpret the text in the KLineEdits as Ra and Dec values.
    	*If the point is validated, close the window.
    	*/
    void validatePoint();

private:
    KStars *ks;
    SkyPoint Point;
    FocusDialogUI *fd;
    bool UsedAltAz;
};

#endif
