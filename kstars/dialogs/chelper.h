/***************************************************************************
                          CHelper.h  -  description
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

#ifndef CHelper_H_
#define CHelper_H_

#include <QDialog>

#include "ui_chelper.h"

#include "skyobjects/skypoint.h"
class KStars;

class CHelperUI : public QFrame, public Ui::CHelper {
    Q_OBJECT
public:
    explicit CHelperUI( QWidget *parent=0 );
};


class CHelper : public QDialog {
    Q_OBJECT
public:
    /**Constructor. */
    explicit CHelper();

    /**Destructor (empty). */
    ~CHelper();



public slots:

    /**Attempt to interpret the text in the KLineEdits as Ra and Dec values.
    	*If the point is validated, close the window.
    	*/
    void validatePoint();

private:

    CHelperUI *fd;

};

#endif
