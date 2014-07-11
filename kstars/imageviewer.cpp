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
#include "kstars.h"

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

#include <KJobUiDelegate>
#include <KLocalizedString>
#include <kmessagebox.h>

ImageLabel::ImageLabel( QWidget *parent ) : QFrame( parent )
{
    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Expanding );
    setFrameStyle( QFrame::StyledPanel | QFrame::Plain );
    setLineWidth( 2 );
}

ImageLabel::~ImageLabel()
{}

void ImageLabel::setImage( const QImage &img )
{
    m_Image = img;
    pix = QPixmap::fromImage(m_Image);
}

void ImageLabel::invertPixels()
{
    m_Image.invertPixels();
    pix = QPixmap::fromImage(m_Image.scaled(width(), height(), Qt::KeepAspectRatio));
}

void ImageLabel::paintEvent (QPaintEvent*)
{
    QPainter p;
    p.begin( this );
    int x = 0;
    if( pix.width() < width() )
        x = (width() - pix.width())/2;
    p.drawPixmap( x, 0, pix );
    p.end();
}

void ImageLabel::resizeEvent(QResizeEvent *event)
{
    int w=pix.width();
    int h=pix.height();

    if (event->size().width() == w && event->size().height() == h)
        return;

    pix = QPixmap::fromImage(m_Image.scaled(event->size(), Qt::KeepAspectRatio));
}

ImageViewer::ImageViewer (const QUrl &url, const QString &capText, QWidget *parent) :
    QDialog( parent ),
    m_ImageUrl(url),
    fileIsImage(false),
    downloadJob(0)
{
    init(url.fileName(), capText);
    // Add save button
    //FIXME Needs porting to KF5
    //setButtons( QDialog::User2 | QDialog::User1 | QDialog::Close );

    KGuiItem saveButton( xi18n("Save"), "document-save", xi18n("Save the image to disk") );
    //FIXME Needs porting to KF5
    //setButtonGuiItem( QDialog::User1, saveButton );

    // FIXME: Add more options, and do this more nicely
    KGuiItem invertButton( xi18n("Invert colors"), "", xi18n("Reverse colors of the image. This is useful to enhance contrast at times. This affects only the display and not the saving.") );
    //FIXME Needs porting to KF5
    //setButtonGuiItem( QDialog::User2, invertButton );

    connect( this, SIGNAL( user1Clicked() ), this, SLOT ( saveFileToDisc() ) );
    connect( this, SIGNAL( user2Clicked() ), this, SLOT ( invertColors() ) );
    // check URL
    if (!m_ImageUrl.isValid())
        qDebug() << "URL is malformed: " << m_ImageUrl;
    
    // FIXME: check the logic with temporary files. Races are possible
    {
        QTemporaryFile tempfile;
        tempfile.open();
        file.setFileName( tempfile.fileName() );
    }// we just need the name and delete the tempfile from disc; if we don't do it, a dialog will be show

    loadImageFromURL();
}

ImageViewer::ImageViewer ( QString FileName, QWidget *parent ) :
    QDialog( parent ),
    fileIsImage(true),
    downloadJob(0)
{
    init(FileName, QString());
    file.setFileName( FileName );
    showImage();
}

void ImageViewer::init(QString caption, QString capText) {
    setAttribute( Qt::WA_DeleteOnClose, true );
    setModal( false );
    setWindowTitle( xi18n( "KStars image viewer: %1", caption ) );

    //FIXME Needs porting to KF5
    //setButtons( QDialog::Close | QDialog::User1);

    // FIXME: Add more options, and do this more nicely
    KGuiItem invertButton( xi18n("Invert colors"), "", xi18n("Reverse colors of the image. This is useful to enhance contrast at times. This affects only the display and not the saving.") );
    //FIXME Needs porting to KF5
    //setButtonGuiItem( QDialog::User1, invertButton );
    connect( this, SIGNAL( user1Clicked() ), this, SLOT ( invertColors() ) );


    // Create widget
    QFrame* page = new QFrame( this );

    //setMainWidget( page );
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(page);
    setLayout(mainLayout);

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
}

ImageViewer::~ImageViewer() {
    if ( downloadJob ) {
        // close job quietly, without emitting a result
        downloadJob->kill( KJob::Quietly );
        delete downloadJob;
    }
}

void ImageViewer::loadImageFromURL()
{
    //FIXME Need to check this
    //QUrl saveURL = QUrl::fromPath( file.fileName() );
    QUrl saveURL = QUrl::fromLocalFile(file.fileName() );

    if (!saveURL.isValid())
        qDebug()<<"tempfile-URL is malformed\n";

    downloadJob = KIO::copy (m_ImageUrl, saveURL);	// starts the download asynchron
    connect (downloadJob, SIGNAL (result (KJob *)), SLOT (downloadReady (KJob *)));
}

void ImageViewer::downloadReady (KJob *job)
{
    // set downloadJob to 0, but don't delete it - the job will be deleted automatically !!!
    downloadJob = 0;

    if ( job->error() ) {
        job->uiDelegate()->showErrorMessage();
        close();        
        return;
    }

    file.close(); // to get the newest information from the file and not any information from opening of the file

    if ( file.exists() ) {
        showImage();
        return;
    }
    close();
}

void ImageViewer::showImage()
{
    QImage image;
    if( !image.load( file.fileName() )) {
        //TODO Check this
        //QString text = xi18n ("Loading of the image %1 failed.", m_ImageUrl.prettyUrl());
        QString text = xi18n ("Loading of the image %1 failed.", m_ImageUrl.url());
        KMessageBox::error (this, text);
        close();
        return;
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
}

void ImageViewer::saveFileToDisc()
{
    //QUrl newURL = KFileDialog::getSaveUrl(m_ImageUrl.fileName());  // save-dialog with default filename
    //TODO We need to put the default filename in there?
    QUrl newURL = QFileDialog::getSaveFileUrl(0, xi18n("Save Image"));//m_ImageUrl.fileName());  // save-dialog with default filename
    if (!newURL.isEmpty())
    {
        QFile f (newURL.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash).path() + '/' +  newURL.fileName());
        if (f.exists())
        {
            int r=KMessageBox::warningContinueCancel(static_cast<QWidget *>(parent()),
                    xi18n( "A file named \"%1\" already exists. "
                          "Overwrite it?" , newURL.fileName()),
                    xi18n( "Overwrite File?" ),
                    KStandardGuiItem::overwrite() );
            if(r==KMessageBox::Cancel) return;

            f.remove();
        }
        saveFile (newURL);
    }
}

void ImageViewer::saveFile (QUrl &url) {
    // synchronous access to prevent segfaults
    //FIXME Needs porting to KF5
    /*
    if (!KIO::NetAccess::file_copy (QUrl (file.fileName()), url, (QWidget*) 0))
    {
        QString text = xi18n ("Saving of the image %1 failed.", url.prettyUrl());
        KMessageBox::error (this, text);
    }
    */
}

void ImageViewer::invertColors() {
    // Invert colors
    m_View->invertPixels();
    m_View->update();
}

#include "imageviewer.moc"
