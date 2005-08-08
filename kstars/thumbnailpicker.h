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

#ifndef THUMBNAILPICKER_H
#define THUMBNAILPICKER_H

#include <kdialogbase.h>
#include <kio/netaccess.h>
#include <kio/job.h>

class ThumbnailPickerUI;
class ThumbnailEditor;
class QPixmap;
class QFile;
class QRect;
class DetailDialog;
class SkyObject;

/**@short Dialog for modifying an object's thumbnail image
	*@author Jason Harris
	*/

class ThumbnailPicker : public KDialogBase
{
Q_OBJECT
public:
	ThumbnailPicker( SkyObject *o, const QPixmap &current, QWidget *parent=0, const char *name=0 );
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

/**Make sure download has finished, then make sure file exists, then add image to list */
	void downloadReady (KIO::Job *);

private:
	QPixmap shrinkImage( QPixmap *original, int newSize, bool setImage=false );
	void parseGooglePage( QStringList &ImList, QString URL );

	int SelectedImageIndex;
	ThumbnailPickerUI *ui;
	QPixmap *Image;
	DetailDialog *dd;
	SkyObject *Object;
	QPtrList<KIO::Job> JobList;
	QPtrList<QPixmap> PixList;
	bool bImageFound;
	QRect *ImageRect;
};

#endif
