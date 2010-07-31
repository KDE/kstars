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
	ThumbnailPicker( SkyObject *o, const TQPixmap &current, TQWidget *parent=0, const char *name=0 );
	~ThumbnailPicker();

	TQPixmap* image() { return Image; }
	TQPixmap* currentListImage() { return PixList.at( SelectedImageIndex ); }
	bool imageFound() const { return bImageFound; }
	TQRect* imageRect() const { return ImageRect; }

private slots:
	void slotEditImage();
	void slotUnsetImage();
	void slotSetFromList( int i );
	void slotSetFromURL();
	void slotFillList();

/**Make sure download has finished, then make sure file exists, then add image to list */
	void downloadReady (KIO::Job *);

private:
	TQPixmap shrinkImage( TQPixmap *original, int newSize, bool setImage=false );
	void parseGooglePage( TQStringList &ImList, TQString URL );

	int SelectedImageIndex;
	ThumbnailPickerUI *ui;
	TQPixmap *Image;
	DetailDialog *dd;
	SkyObject *Object;
	TQPtrList<KIO::Job> JobList;
	TQPtrList<TQPixmap> PixList;
	bool bImageFound;
	TQRect *ImageRect;
};

#endif
