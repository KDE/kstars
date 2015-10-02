/***************************************************************************
                          thumbnailpicker.h  -  description
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

#ifndef THUMBNAILPICKER_H_
#define THUMBNAILPICKER_H_

#include <QPixmap>
#include <QDialog>

#include <KJob>
#include <KLocalizedString>
#include <KIO/Job>

#include "ui_thumbnailpicker.h"

class QPixmap;
class QRect;
class DetailDialog;
class SkyObject;

class ThumbnailPickerUI : public QFrame, public Ui::ThumbnailPicker {
    Q_OBJECT
public:
    explicit ThumbnailPickerUI( QWidget *p );
};

/** @short Dialog for modifying an object's thumbnail image
	*@author Jason Harris
	*/
class ThumbnailPicker : public QDialog
{
    Q_OBJECT
public:
    ThumbnailPicker( SkyObject *o, const QPixmap &current, QWidget *parent=0, double w = 200, double h= 200, QString cap = i18n ( "Choose Thumbnail Image" ) );
    ~ThumbnailPicker();

    QPixmap* image() { return Image; }
    QPixmap* currentListImage() { return PixList.at( SelectedImageIndex ); }
    bool imageFound() const { return bImageFound; }
    QRect* imageRect() const { return ImageRect; }

private slots:
    void slotEditImage();
    void slotUnsetImage();
    void slotSetFromList( int i );
    void slotSetFromURL();
    void slotFillList();
    void slotProcessGoogleResult(KJob*);

    /**Make sure download has finished, then make sure file exists, then add image to list */
    void slotJobResult(KJob *);

private:
    QPixmap shrinkImage( QPixmap *original, int newSize, bool setImage=false );
    void parseGooglePage(const QString &URL );

    int SelectedImageIndex;
    double wid, ht;
    ThumbnailPickerUI *ui;
    QPixmap *Image;
    DetailDialog *dd;
    SkyObject *Object;
    QList<KIO::Job*> JobList;
    QList<QPixmap*> PixList;
    bool bImageFound;
    QRect *ImageRect;
};

#endif
