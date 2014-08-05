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
#include <QStatusBar>

#include <KJobUiDelegate>
#include <KLocalizedString>
#include <KMessageBox>

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

    // check URL
    if (!m_ImageUrl.isValid())
        qDebug() << "URL is malformed: " << m_ImageUrl;
    
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

    // Create widget
    QFrame* page = new QFrame( this );

    //setMainWidget( page );
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(page);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QPushButton *invertB = new QPushButton(xi18n("Invert colors"));
    invertB->setToolTip(xi18n("Reverse colors of the image. This is useful to enhance contrast at times. This affects only the display and not the saving."));
    QPushButton *saveB   = new QPushButton(QIcon::fromTheme("document-save"), xi18n("Save"));
    saveB->setToolTip(xi18n("Save the image to disk"));

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
    QFileDialog dialog;
    dialog.selectFile(m_ImageUrl.fileName().remove(m_ImageUrl.path()));
    dialog.setFileMode(QFileDialog::AnyFile);
    QUrl newURL = dialog.getSaveFileUrl(0, xi18n("Save Image")); // save-dialog with default filename
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

    //if (!KIO::NetAccess::file_copy (QUrl (file.fileName()), url, (QWidget*) 0))
    QUrl tmpURL((file.fileName()));
    tmpURL.setScheme("file");
    if (KIO::file_copy(tmpURL, url)->exec() == false)
    {
        QString text = xi18n ("Saving of the image %1 failed.", url.toString());
        KMessageBox::error (this, text);
    }
    else
        KStars::Instance()->statusBar()->showMessage(xi18n ("Saved image to %1", url.toString()));
}

void ImageViewer::invertColors() {
    // Invert colors
    m_View->invertPixels();
    m_View->update();
}


