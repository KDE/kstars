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

#include <qimage.h>
#include <qpixmap.h>

#include <kpixmapio.h>
#include <kio/job.h>
#include <ktempfile.h>
#include <kmainwindow.h>

/**@class ImageViewer
	*@short Image viewer widget for KStars
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

class KURL;
class QFile;

class ImageViewer : public KMainWindow  {
	Q_OBJECT

	public:
	/**Constructor. */
		ImageViewer (const KURL *imageName, const QString &capText, QWidget *parent, const char *name = 0);

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

	/**Keyboard shortcuts for saving files and closing the window
		*@note (this should be deprecated; instead just assign KAccel 
		*to the close/save buttons)
		*/
		void keyPressEvent (QKeyEvent *ev);

	/**Unset the bool variables that indicate keys were pressed.
		*(this should be deprecated; see above)
		*/
		void keyReleaseEvent (QKeyEvent *ev);

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
		void saveFile (KURL &url);
		
	/**Kill running download jobs, if close of window is forced.
		*/
		void checkJob();

		QImage image;
		QPixmap pix;
		KPixmapIO kpix;
		KTempFile tempfile;
		QFile *file;
		
		const KURL imageURL;
		bool fileIsImage;
		QString filename;
		bool ctrl, key_s, key_q;	// the keys

		KIO::Job *downloadJob;  // download job of image -> 0 == no job is running
		
	private slots:
	/**Make sure download has finished, then make sure file exists, then display the image */
		void downloadReady (KIO::Job *);

	/**Saves. File. To. Disc. */
		void saveFileToDisc( void );

	/**Close the window.*/
		void close( void );
};

#endif
