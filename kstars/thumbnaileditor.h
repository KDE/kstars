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

#ifndef THUMBNAILEDITOR_H_
#define THUMBNAILEDITOR_H_

#include <QMouseEvent>
#include <QPaintEvent>

#include <kdialog.h>

#include "ui_thumbnaileditor.h"

class ThumbnailPicker;

class ThumbnailEditorUI : public QFrame, public Ui::ThumbnailEditor {
    Q_OBJECT
public:
    explicit ThumbnailEditorUI( QWidget *parent );
};

class ThumbnailEditor : public KDialog
{
    Q_OBJECT
public:
    ThumbnailEditor( ThumbnailPicker *_tp, double _w, double _h );
    ~ThumbnailEditor();
    QPixmap thumbnail();

private slots:
    void slotUpdateCropLabel();

private:
    ThumbnailEditorUI *ui;
    ThumbnailPicker *tp;
    double w, h;

};

#endif
