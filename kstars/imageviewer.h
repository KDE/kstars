/***************************************************************************
                          imageviewer.h  -  An ImageViewer for KStars
                             -------------------
    begin                : Mon Aug 27 2001
    copyright            : (C) 2001 by Thomas Kabelmann
    email                : tk78@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <qwidget.h>
#include <qstring.h>
#include <qimage.h>
#include <qpixmap.h>

#include <kpixmapio.h>
#include <kio/job.h>
#include <ktempfile.h>
#include <kdialog.h>
#include <kmainwindow.h>

/**
  *@author Thomas Kabelmann

  This image-viewer resizes automatically the picture. The output works with kio-slaves and not directly with the
  QImage save-routines because normally the image-files are in GIF-format and QT does not save these files. For other
  formats, like PNG, this is not so important because they can directly saved by QImage.

  The download-slave works asynchron so the parent-widget can be used at this time. The save-slave works synchron,
  but this is not important because the files are at this time local saved and this works not so long.
*/

class KURL;
class QFile;

class ImageViewer : public KMainWindow  {
	Q_OBJECT

	public:
		ImageViewer (const KURL *imageName, QWidget *parent, const char *name = 0);
		~ImageViewer();

	signals:
		void StartDownload();
		void DownloadComplete();
		
	protected:
		void paintEvent (QPaintEvent *ev);
		void resizeEvent (QResizeEvent *ev);
		void closeEvent (QCloseEvent *ev);
		void keyPressEvent (QKeyEvent *ev);
		void keyReleaseEvent (QKeyEvent *ev);

	private:
		void showImage();
		void loadImageFromURL();
		void saveFile (KURL &url);
		
		QImage image;
		QPixmap pix;
		KPixmapIO kpix;
		KTempFile tempfile;
		QFile *file;
				
		const KURL imageURL;
		bool downloadComplete, fileIsImage;
		QString filename;
		bool ctrl, key_s, key_q;	// the keys
		
	private slots:
		void downloadReady (KIO::Job *);
		void saveFileToDisc();
		void close();
};

#endif
