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

#include <klocale.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kstatusbar.h>
#include <kio/netaccess.h>
#include <kio/copyjob.h>
#include <kio/jobuidelegate.h>
#include <kaction.h>
#include <ktemporaryfile.h>
#include <kdebug.h>
#include <ktoolbar.h>

ImageLabel::ImageLabel( QWidget *parent ) : QFrame( parent )
{
    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Expanding );
    setFrameStyle( QFrame::StyledPanel | QFrame::Plain );
    setLineWidth( 2 );
}

ImageLabel::~ImageLabel()
{}

void ImageLabel::paintEvent (QPaintEvent*) {
    QPainter p;
    p.begin( this );
    int x = 0;
    if( m_Image.width() < width() )
        x = (width() - m_Image.width())/2;
    p.drawImage( x, 0, m_Image );
    p.end();
}

ImageViewer::ImageViewer (const KUrl &url, const QString &capText, QWidget *parent) :
    KDialog( parent ),
    m_ImageUrl(url),
    fileIsImage(false),
    downloadJob(0)
{
    init(url.fileName(), capText);
    // Add save button
    KGuiItem saveButton( i18n("Save"), "document-save", i18n("Save the image to disk") );
    setButtonGuiItem( KDialog::User1, saveButton );
    connect( this, SIGNAL( user1Clicked() ), this, SLOT ( saveFileToDisc() ) );
    // check URL
    if (!m_ImageUrl.isValid())
        kDebug() << "URL is malformed: " << m_ImageUrl;
    
    // FIXME: check the logic with temporary files. Races are possible
    {
        KTemporaryFile tempfile;
        tempfile.open();
        file.setFileName( tempfile.fileName() );
    }// we just need the name and delete the tempfile from disc; if we don't do it, a dialog will be show

    loadImageFromURL();
}

ImageViewer::ImageViewer ( QString FileName, QWidget *parent ) :
    KDialog( parent ),
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
    setCaption( i18n( "KStars image viewer" ) + QString( " : " ) + caption );
    setButtons( KDialog::Close );

    // Create widget
    QFrame* page = new QFrame( this );
    setMainWidget( page );
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
    KUrl saveURL = KUrl::fromPath( file.fileName() );
    if (!saveURL.isValid())
        kDebug()<<"tempfile-URL is malformed\n";

    downloadJob = KIO::copy (m_ImageUrl, saveURL);	// starts the download asynchron
    connect (downloadJob, SIGNAL (result (KJob *)), SLOT (downloadReady (KJob *)));
}

void ImageViewer::downloadReady (KJob *job)
{
    // set downloadJob to 0, but don't delete it - the job will be deleted automatically !!!
    downloadJob = 0;

    if ( job->error() ) {
        static_cast<KIO::Job*>(job)->ui()->showErrorMessage();
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
        QString text = i18n ("Loading of the image %1 failed.", m_ImageUrl.prettyUrl());
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
    setFixedSize( w, image.height() + m_Caption->height() );

    update();
}

void ImageViewer::saveFileToDisc()
{
    KUrl newURL = KFileDialog::getSaveUrl(m_ImageUrl.fileName());  // save-dialog with default filename
    if (!newURL.isEmpty())
    {
        QFile f (newURL.directory() + '/' +  newURL.fileName());
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
        saveFile (newURL);
    }
}

void ImageViewer::saveFile (KUrl &url) {
    // synchronous access to prevent segfaults
    if (!KIO::NetAccess::file_copy (KUrl (file.fileName()), url, (QWidget*) 0))
    {
        QString text = i18n ("Saving of the image %1 failed.", url.prettyUrl());
        KMessageBox::error (this, text);
    }
}

#include "imageviewer.moc"
