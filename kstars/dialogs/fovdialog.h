/***************************************************************************
                          fovdialog.h  -  description
                             -------------------
    begin                : Fri 05 Sept 2003
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

#ifndef FOVDIALOG_H_
#define FOVDIALOG_H_

#include <QPaintEvent>
#include <kdialog.h>

#include "fov.h"
#include "ui_fovdialog.h"
#include "ui_newfov.h"

/**@class FOVDialog Dialog to select a Field-of-View indicator (or create a new one)
	*@author Jason Harris
	*@version 1.0
	*/

class KStars;
class FOVWidget;

class FOVDialogUI : public QFrame, public Ui::FOVDialog {
    Q_OBJECT
public:
    FOVDialogUI( QWidget *parent=0 );
};

class NewFOVUI : public QFrame, public Ui::NewFOV {
    Q_OBJECT
public:
    NewFOVUI( QWidget *parent=0 );
};

class FOVDialog : public KDialog
{
    Q_OBJECT
public:
    FOVDialog( QWidget *parent = 0 );
    virtual ~FOVDialog();
    /** Write list of FOVs to disk. */
    void writeFOVList();
private slots:
    void slotNewFOV();
    void slotEditFOV();
    void slotRemoveFOV();
    void slotSelect(int);
private:
    /** Add new widget to list box */
    QListWidgetItem* addListWidget(FOV* f);
    
    unsigned int currentItem() const;   
    FOVDialogUI *fov;
    static int fovID;
};

/**@class NewFOV Dialog for defining a new FOV symbol
	*@author Jason Harris
	*@version 1.0
	*/
class NewFOV : public KDialog
{
    Q_OBJECT
public:
    /** Create new dialog
     * @param parent parent widget
     * @fov widget to copy data from. If it's empty will create empty one.
     */
    NewFOV( QWidget *parent=0, const FOV* fov = 0);
    ~NewFOV() {}
    /** Return reference to FOV. */
    const FOV& getFOV() const { return f; }
public slots:
    void slotBinocularFOVDistanceChanged( int index );
    void slotUpdateFOV();
    void slotComputeFOV();

private:
    FOV f;
    NewFOVUI *ui;   
};

#endif
