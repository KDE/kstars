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

#include <QFrame>
#include <QFile>
#include <QImage>
#include <QPixmap>

#include <kio/job.h>
#include <QDialog>

class QUrl;
class QLabel;

class ImageLabel : public QFrame {
    Q_OBJECT
public:
    explicit ImageLabel( QWidget *parent );
    ~ImageLabel();
    void setImage( const QImage &img );
    void invertPixels();

    QImage m_Image; // ImageViewer needs access to the image in order to modify it
protected:
    void paintEvent( QPaintEvent *e);
    void resizeEvent(QResizeEvent *);

private:
    QPixmap pix;

};

/** @class ImageViewer
 * @short Image viewer window for KStars
 * @author Thomas Kabelmann
 * @version 1.0
 *
 * This image-viewer automatically resizes the picture. The output
 * works with kio-slaves and not directly with the QImage save-routines
 * because normally the image-files are in GIF-format and QT does not
 * save these files. For other formats, like PNG, this is not so important
 * because they can directly saved by QImage.
 *
 * The download-slave works asynchron so the parent-widget can be used at
 * this time. The save-slave works synchronously, but this is not important
 * because the files are at this time local saved and this works not so long.
 */
class ImageViewer : public QDialog {
    Q_OBJECT

public:
    /** Create image viewer from URL with caption */
    ImageViewer (const QUrl &imageURL, const QString &capText, QWidget *parent = 0);

    explicit ImageViewer ( QString FileName, QWidget *parent = 0);

    /** Destructor. If there is a partially downloaded image file, delete it.*/
    ~ImageViewer();

private:
    /** Display the downloaded image.  Resize the window to fit the image,  If the image is
     * larger than the screen, make the image as large as possible while preserving the
     * original aspect ratio */
    void showImage( void );

    /** Download the image file pointed to by the URL string. */
    void loadImageFromURL( void );

    /** Save the downloaded image to a local file. */
    void saveFile (QUrl &url);

    QFile file;

    const QUrl m_ImageUrl;
    bool fileIsImage;
    QString filename;

    KIO::Job *downloadJob;  // download job of image -> 0 == no job is running

    ImageLabel *m_View;
    QLabel     *m_Caption;

private slots:
    /** Initialize (common part of onstructors) */
    void init(QString caption, QString capText);
    /** Make sure download has finished, then make sure file exists, then display the image */
    void downloadReady (KJob *);

    /** Saves file to disc. */
    void saveFileToDisc();

    /** Inverts colors **/
    void invertColors();
};

#endif
