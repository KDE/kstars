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

#ifndef IMAGEVIEWER_H_
#define IMAGEVIEWER_H_

#include <QImage>
#include <QPixmap>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QCloseEvent>

#include <kio/job.h>
#include <ktempfile.h>
#include <kdialog.h>

#include "ui_imageviewer.h"

/**@class ImageViewer
	*@short Image viewer window for KStars
	*@author Thomas Kabelmann
	*@version 1.0
	*
	*This image-viewer automatically resizes the picture. The output 
	*works with kio-slaves and not directly with the QImage save-routines 
	*because normally the image-files are in GIF-format and QT does not 
	*save these files. For other formats, like PNG, this is not so important 
	*because they can directly saved by QImage.
	*
	*The download-slave works asynchron so the parent-widget can be used at 
	*this time. The save-slave works synchronously, but this is not important 
	*because the files are at this time local saved and this works not so long.
	*/

class KUrl;
class QFile;

class ImageViewerUI : public QFrame, public Ui::ImageViewer {
	Q_OBJECT
	public:
		ImageViewerUI( QWidget *parent );
};

class ImageViewer : public KDialog {
	Q_OBJECT

	public:
	/**Constructor. */
		ImageViewer (const KUrl &imageURL, const QString &capText, QWidget *parent );

	/**Destructor. If there is a partially downloaded image file, delete it.*/
		~ImageViewer();

	protected:
	/**Bitblt the image onto the viewer widget */
		void paintEvent (QPaintEvent *ev);

	/**The window is resized when a file finishes downloading, before it is displayed.
		*The resizeEvent converts the downloaded QImage to a QPixmap 
		*@note (JH: not sure why this conversion is not done in showImage)
		*/
		void resizeEvent (QResizeEvent *ev);

	/**Make sure all events have been processed before closing the dialog */
		void closeEvent (QCloseEvent *ev);

	private:
	/**Display the downloaded image.  Resize the window to fit the image,  If the image is
		*larger than the screen, make the image as large as possible while preserving the
		*original aspect ratio
		*/
		void showImage( void );

	/**Download the image file pointed to by the URL string.
		*/
		void loadImageFromURL( void );

	/**Save the downloaded image to a local file.
		*/
		void saveFile (KUrl &url);
		
	/**Kill running download jobs, if close of window is forced.
		*/
		void checkJob();

		QImage image;
		//KPixmapIO kpix;
		KTempFile tempfile;
		QFile *file;
		
		const KUrl m_ImageUrl;
		bool fileIsImage;
		QString filename;

		KIO::Job *downloadJob;  // download job of image -> 0 == no job is running
		
		ImageViewerUI *ui;

	private slots:
	/**Make sure download has finished, then make sure file exists, then display the image */
		void downloadReady (KJob *);

	/**Saves. File. To. Disc. */
		void saveFileToDisc( void );

	/**Close the window.*/
		void close( void );
};

#endif
