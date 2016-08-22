/***************************************************************************
                          imageviewer.cpp  -  An ImageViewer for KStars
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

#include "imageviewer.h"
#ifndef KSTARS_LITE
#include "kstars.h"
#endif

#include <QFont>
#include <QPainter>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QVBoxLayout>
#include <QApplication>
#include <QLabel>
#include <QDebug>
#include <QFileDialog>
#include <QStatusBar>

#ifndef KSTARS_LITE
#include <KMessageBox>
#include <KJobUiDelegate>
#endif
//#include <KIO/CopyJob>
#include <KLocalizedString>

QUrl ImageViewer::lastURL = QUrl::fromLocalFile(QDir::homePath());

ImageLabel::ImageLabel( QWidget *parent ) : QFrame( parent )
{
    #ifndef KSTARS_LITE
    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Expanding );
    setFrameStyle( QFrame::StyledPanel | QFrame::Plain );
    setLineWidth( 2 );
    #endif
}

ImageLabel::~ImageLabel()
{}

void ImageLabel::setImage( const QImage &img )
{
    #ifndef KSTARS_LITE
    m_Image = img;
    pix = QPixmap::fromImage(m_Image);
    #endif
}

void ImageLabel::invertPixels()
{
    #ifndef KSTARS_LITE
    m_Image.invertPixels();
    pix = QPixmap::fromImage(m_Image.scaled(width(), height(), Qt::KeepAspectRatio));
    #endif
}

void ImageLabel::paintEvent (QPaintEvent*)
{
    #ifndef KSTARS_LITE
    QPainter p;
    p.begin( this );
    int x = 0;
    if( pix.width() < width() )
        x = (width() - pix.width())/2;
    p.drawPixmap( x, 0, pix );
    p.end();
    #endif
}

void ImageLabel::resizeEvent(QResizeEvent *event)
{
    #ifndef KSTARS_LITE
    int w=pix.width();
    int h=pix.height();

    if (event->size().width() == w && event->size().height() == h)
        return;

<<<<<<< HEAD
    pix = QPixmap::fromImage(m_Image.scaled(event->size(), Qt::KeepAspectRatio));
    #endif
=======
    pix = QPixmap::fromImage(m_Image.scaled(event->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
>>>>>>> master
}

ImageViewer::ImageViewer (const QString &caption, QWidget *parent):
    QDialog( parent ),
    fileIsImage(false),
    downloadJob(0)
{
    #ifndef KSTARS_LITE
    init(caption, QString());
    #endif
}

ImageViewer::ImageViewer (const QUrl &url, const QString &capText, QWidget *parent) :
    QDialog( parent ),
    m_ImageUrl(url),
    fileIsImage(false)
{
    #ifndef KSTARS_LITE
    init(url.fileName(), capText);        

    // check URL
    if (!m_ImageUrl.isValid())
        qDebug() << "URL is malformed: " << m_ImageUrl;

    if (m_ImageUrl.isLocalFile())
    {
        loadImage(m_ImageUrl.toLocalFile());
        return;
    }
    
    {
        QTemporaryFile tempfile;
        tempfile.open();
        file.setFileName( tempfile.fileName() );
    }// we just need the name and delete the tempfile from disc; if we don't do it, a dialog will be show

    loadImageFromURL();
    #endif
}

void ImageViewer::init(QString caption, QString capText)
{
    #ifndef KSTARS_LITE
    setAttribute( Qt::WA_DeleteOnClose, true );
    setModal( false );
    setWindowTitle( i18n( "KStars image viewer: %1", caption ) );

    // Create widget
    QFrame* page = new QFrame( this );

    //setMainWidget( page );
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(page);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QPushButton *invertB = new QPushButton(i18n("Invert colors"));
    invertB->setToolTip(i18n("Reverse colors of the image. This is useful to enhance contrast at times. This affects only the display and not the saving."));
    QPushButton *saveB   = new QPushButton(QIcon::fromTheme("document-save"), i18n("Save"));
    saveB->setToolTip(i18n("Save the image to disk"));

    buttonBox->addButton(invertB, QDialogButtonBox::ActionRole);
    buttonBox->addButton(saveB, QDialogButtonBox::ActionRole);

    connect(invertB, SIGNAL(clicked()), this, SLOT(invertColors()));
    connect(saveB, SIGNAL(clicked()), this, SLOT(saveFileToDisc()));

    m_View = new ImageLabel( page );
    m_View->setAutoFillBackground( true );
    m_Caption = new QLabel( page );
    m_Caption->setAutoFillBackground( true );
    m_Caption->setFrameShape( QFrame::StyledPanel );
    m_Caption->setText( capText );
    // Add layout
    QVBoxLayout* vlay = new QVBoxLayout( page );
    vlay->setSpacing( 0 );
    vlay->setMargin( 0 );
    vlay->addWidget( m_View );
    vlay->addWidget( m_Caption );

    //Reverse colors
    QPalette p = palette();
    p.setColor( QPalette::Window, palette().color( QPalette::WindowText ) );
    p.setColor( QPalette::WindowText, palette().color( QPalette::Window ) );
    m_Caption->setPalette( p );
    m_View->setPalette( p );
    
    //If the caption is wider than the image, try to shrink the font a bit
    QFont capFont = m_Caption->font();
    capFont.setPointSize( capFont.pointSize() - 2 );
    m_Caption->setFont( capFont );
    #endif
}

<<<<<<< HEAD
ImageViewer::~ImageViewer() {
#ifndef KSTARS_LITE
    /*if ( downloadJob ) {
        // close job quietly, without emitting a result
        downloadJob->kill( KJob::Quietly );
        delete downloadJob;
    }*/
=======
ImageViewer::~ImageViewer()
{
    QString filename = file.fileName();
    if (filename.startsWith("/tmp/") || filename.contains("/Temp"))
    {
        if (m_ImageUrl.isEmpty() == false ||
                KMessageBox::questionYesNo(0, i18n("Remove temporary file %1 from disk?", filename), i18n("Confirm Removal"),
                                   KStandardGuiItem::yes(), KStandardGuiItem::no(), i18n("imageviewer_temporary_file_removal")) == KMessageBox::Yes)
            QFile::remove(filename);
    }
>>>>>>> master

    QApplication::restoreOverrideCursor();
#endif
}

void ImageViewer::loadImageFromURL()
{
#ifndef KSTARS_LITE
    QUrl saveURL = QUrl::fromLocalFile(file.fileName() );

    if (!saveURL.isValid())
        qDebug()<<"tempfile-URL is malformed\n";

    QApplication::setOverrideCursor(Qt::WaitCursor);

    connect(&downloadJob, SIGNAL(downloaded()), this, SLOT(downloadReady()));
    connect(&downloadJob, SIGNAL(error(QString)), this, SLOT(downloadError(QString)));

    downloadJob.get(m_ImageUrl);
#endif
}

void ImageViewer::downloadReady ()
{
#ifndef KSTARS_LITE
    QApplication::restoreOverrideCursor();

    if (file.open(QFile::WriteOnly))
    {
        file.write(downloadJob.downloadedData());
        file.close(); // to get the newest information from the file and not any information from opening of the file

        if ( file.exists() )
        {
            showImage();
            return;
        }

        close();
    }
    else
        KMessageBox::error(0, file.errorString(), i18n("Image Viewer"));
#endif
}

void ImageViewer::downloadError(const QString &errorString)
{
#ifndef KSTARS_LITE
    QApplication::restoreOverrideCursor();
    KMessageBox::error(this, errorString);
#endif
}

bool ImageViewer::loadImage(const QString &filename)
{
#ifndef KSTARS_LITE
    file.setFileName(filename);
    return showImage();
#else
    return false;
#endif
}

bool ImageViewer::showImage()
{
#ifndef KSTARS_LITE
    QImage image;

    if( !image.load( file.fileName() ))
    {
        QString text = i18n ("Loading of the image %1 failed.", m_ImageUrl.url());
        KMessageBox::error (this, text);
        close();
        return false;
    }

    fileIsImage = true;	// we loaded the file and know now, that it is an image

    //If the image is larger than screen width and/or screen height,
    //shrink it to fit the screen
    QRect deskRect = QApplication::desktop()->availableGeometry();
    int w = deskRect.width(); // screen width
    int h = deskRect.height(); // screen height

    if ( image.width() <= w && image.height() > h ) //Window is taller than desktop
        image = image.scaled( int( image.width()*h/image.height() ), h );
    else if ( image.height() <= h && image.width() > w ) //window is wider than desktop
        image = image.scaled( w, int( image.height()*w/image.width() ) );
    else if ( image.width() > w && image.height() > h ) { //window is too tall and too wide
        //which needs to be shrunk least, width or height?
        float fx = float(w)/float(image.width());
        float fy = float(h)/float(image.height());
        if (fx > fy) //width needs to be shrunk less, so shrink to fit in height
            image = image.scaled( int( image.width()*fy ), h );
        else //vice versa
            image = image.scaled( w, int( image.height()*fx ) );
    }

    show();	// hide is default

    m_View->setImage( image );
    w = image.width();

    //If the caption is wider than the image, set the window size
    //to fit the caption
    if ( m_Caption->width() > w )
        w = m_Caption->width();
    //setFixedSize( w, image.height() + m_Caption->height() );

    resize(w, image.height());
    update();

    return true;
#else
    return false;
#endif
}

void ImageViewer::saveFileToDisc()
{
#ifndef KSTARS_LITE
    QFileDialog dialog;

    QUrl newURL = dialog.getSaveFileUrl(KStars::Instance(), i18n("Save Image"), lastURL); // save-dialog with default filename
    if (!newURL.isEmpty())
    {
        //QFile f (newURL.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash).toLocalFile() + '/' +  newURL.fileName());
        QFile f(newURL.toLocalFile());
        if (f.exists())
        {
            int r=KMessageBox::warningContinueCancel(static_cast<QWidget *>(parent()),
                    i18n( "A file named \"%1\" already exists. "
                          "Overwrite it?" , newURL.fileName()),
                    i18n( "Overwrite File?" ),
                    KStandardGuiItem::overwrite() );
            if(r==KMessageBox::Cancel) return;

            f.remove();
        }

        lastURL = QUrl(newURL.toString(QUrl::RemoveFilename));

        saveFile (newURL);
    }
#endif
}

void ImageViewer::saveFile (QUrl &url)
{
    // synchronous access to prevent segfaults

    //if (!KIO::NetAccess::file_copy (QUrl (file.fileName()), url, (QWidget*) 0))
    //QUrl tmpURL((file.fileName()));
    //tmpURL.setScheme("file");

    if (file.copy(url.toLocalFile()) == false)
    //if (KIO::file_copy(tmpURL, url)->exec() == false)
    {
        QString text = i18n ("Saving of the image %1 failed.", url.toString());
        #ifndef KSTARS_LITE
            KMessageBox::error (this, text);
        #else
            qDebug() << text;
        #endif
    }
    #ifndef KSTARS_LITE
    else
        KStars::Instance()->statusBar()->showMessage(i18n ("Saved image to %1", url.toString()));
    #endif
}

void ImageViewer::invertColors() {
    #ifndef KSTARS_LITE
    // Invert colors
    m_View->invertPixels();
    m_View->update();
    #endif
}


