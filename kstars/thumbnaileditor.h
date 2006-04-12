/***************************************************************************
                          thumbnaileditor.h  -  description
                             -------------------
    begin                : Thu Mar 2 2005
    copyright            : (C) 2005 by Jason Harris
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

#ifndef THUMBNAILEDITOR_H
#define THUMBNAILEDITOR_H

#include <QMouseEvent>
#include <QPaintEvent>

#include <kdialogbase.h>

#include "ui_thumbnaileditor.h"

class ThumbnailPicker;
class QPoint;

class ThumbnailEditorUI : public QFrame, public Ui::ThumbnailEditor {
	Q_OBJECT
	public:
		ThumbnailEditorUI( QWidget *parent );
};

class ThumbnailEditor : public KDialogBase
{
Q_OBJECT
public:
	ThumbnailEditor( QWidget *parent, const char *name=0 );
	~ThumbnailEditor();
	QPixmap thumbnail();

private slots:
	void slotUpdateCropLabel();

private:
	ThumbnailEditorUI *ui;
	ThumbnailPicker *tp;

};

#endif
