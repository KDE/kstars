/***************************************************************************
                          thumbnailpicker.cpp  -  description
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

#include "thumbnailpicker.h"
#include "thumbnaileditor.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QTextStream>
#include <QPainter>
#include <kio/copyjob.h>

#include <kdeversion.h>
#include <QPushButton>
#include <klineedit.h>
#include <kmessagebox.h>
#include <QUrl>
#include <kurlrequester.h>
#include <klocale.h>


#include "ksutils.h"
#include "dialogs/detaildialog.h"
#include "skyobjects/skyobject.h"

ThumbnailPickerUI::ThumbnailPickerUI( QWidget *parent ) : QFrame( parent ) {
    setupUi( this );
}

ThumbnailPicker::ThumbnailPicker( SkyObject *o, const QPixmap &current, QWidget *parent, double _w, double _h, QString cap )
        : QDialog( parent ), SelectedImageIndex(-1), dd((DetailDialog*)parent), Object(o), bImageFound( false )
{
    wid = _w;
    ht = _h;
    Image = new QPixmap( current );
    ImageRect = new QRect( 0, 0, 200, 200 );

    ui = new ThumbnailPickerUI( this );
    setMainWidget( ui );
    setWindowTitle( cap );
    setButtons( QDialog::Ok|QDialog::Cancel );

    ui->CurrentImage->setPixmap( *Image );

    connect( ui->EditButton, SIGNAL( clicked() ), this, SLOT( slotEditImage() ) );
    connect( ui->UnsetButton, SIGNAL( clicked() ), this, SLOT( slotUnsetImage() ) );
    connect( ui->ImageList, SIGNAL( currentRowChanged( int ) ),
             this, SLOT( slotSetFromList( int ) ) );
    connect( ui->ImageURLBox, SIGNAL( urlSelected( const QUrl& ) ),
             this, SLOT( slotSetFromURL() ) );
    connect( ui->ImageURLBox, SIGNAL( returnPressed() ),
             this, SLOT( slotSetFromURL() ) );

    ui->ImageURLBox->lineEdit()->setTrapReturnKey( true );
    ui->EditButton->setEnabled( false );

    slotFillList();
}

ThumbnailPicker::~ThumbnailPicker() {
    while ( ! PixList.isEmpty() ) delete PixList.takeFirst();
}

//Query online sources for images of the object
void ThumbnailPicker::slotFillList() {
    //Preload ImageList with the URLs in the object's ImageList:
    QStringList ImageList( Object->ImageList() );

    //Query Google Image Search:
    QUrl gURL( "http://images.google.com/images" );
    //Search for the primary name, or longname and primary name
    QString sName = QString("%1 ").arg( Object->name() );
    if ( Object->longname() != Object->name() ) {
        sName = QString("%1 ").arg( Object->longname() ) + sName;
    }
    gURL.addQueryItem( "q", sName ); //add the Google-image query string

    //Download the google page and parse it for image URLs
    parseGooglePage( ImageList, gURL.prettyUrl() );

    //Total Number of images to be loaded:
    int nImages = ImageList.count();
    if ( nImages ) {
        ui->SearchProgress->setMinimum( 0 );
        ui->SearchProgress->setMaximum( nImages-1 );
        ui->SearchLabel->setText( xi18n( "Loading images..." ) );
    }

    //Add images from the ImageList
    for ( int i=0; i<ImageList.size(); ++i ) {
        QString s( ImageList[i] );
        QUrl u( ImageList[i] );

        if ( u.isValid() ) {
            KIO::StoredTransferJob *j = KIO::storedGet( u, KIO::NoReload, KIO::HideProgressInfo );
            j->setUiDelegate(0);
            connect( j, SIGNAL( result(KJob*) ), SLOT( slotJobResult(KJob*) ) );
        }
    }
}

void ThumbnailPicker::slotJobResult( KJob *job ) {
    KIO::StoredTransferJob *stjob = (KIO::StoredTransferJob*)job;

    //Update Progressbar
    if ( ! ui->SearchProgress->isHidden() ) {
        ui->SearchProgress->setValue(ui->SearchProgress->value()+1);
        if ( ui->SearchProgress->value() == ui->SearchProgress->maximum() ) {
            ui->SearchProgress->hide();
            ui->SearchLabel->setText( xi18n( "Search results:" ) );
        }
    }

    //If there was a problem, just return silently without adding image to list.
    if ( job->error() ) {
        qDebug() << " error=" << job->error();
        job->kill();
        return;
    }

    QPixmap *pm = new QPixmap();
    pm->loadFromData( stjob->data() );

    uint w = pm->width();
    uint h = pm->height();
    uint pad = 0; /*FIXME LATER 4* QDialogBase::marginHint() + 2*ui->SearchLabel->height() + QDialogBase::actionButton( QDialogBase::Ok )->height() + 25;*/
    uint hDesk = QApplication::desktop()->availableGeometry().height() - pad;

    if ( h > hDesk )
        *pm = pm->scaled( w*hDesk/h, hDesk, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );

    PixList.append( pm );

    //Add 50x50 image and URL to listbox
    //ui->ImageList->insertItem( shrinkImage( PixList.last(), 50 ),
    //		cjob->srcURLs().first().prettyUrl() );
    ui->ImageList->addItem( new QListWidgetItem ( QIcon(shrinkImage( PixList.last(), 50 )), stjob->url().prettyUrl() ));
}

void ThumbnailPicker::parseGooglePage( QStringList &ImList, const QString &URL ) {
    QString tmpFile;
    QString PageHTML;

    //Read the google image page's HTML into the PageHTML QString:
    if ( KIO::NetAccess::exists(URL, KIO::NetAccess::SourceSide, this) && KIO::NetAccess::download( URL, tmpFile, this ) ) {
        QFile file( tmpFile );
        if ( file.open( QIODevice::ReadOnly ) ) {
            QTextStream instream(&file);
            PageHTML = instream.readAll();
            file.close();
            KIO::NetAccess::removeTempFile( tmpFile );
        } else {
            qDebug() << "Could not read local copy of google image page";
            KIO::NetAccess::removeTempFile( tmpFile );
            return;
        }
    } else {
        qDebug() << KIO::NetAccess::lastErrorString();
        return;
    }

    int index = PageHTML.indexOf( "src=\"http:", 0 );
    while ( index >= 0 ) {
        index += 5; //move to end of "src=\"http:" marker

        //Image URL is everything from index to next occurrence of "\""
        ImList.append( PageHTML.mid( index, PageHTML.indexOf( "\"", index ) - index ) );

        index = PageHTML.indexOf( "src=\"http:", index );
    }
}

QPixmap ThumbnailPicker::shrinkImage( QPixmap *pm, int size, bool setImage ) {
    int w( pm->width() ), h( pm->height() );
    int bigSize( w );
    int rx(0), ry(0), sx(0), sy(0), bx(0), by(0);
    if ( size == 0 ) return QPixmap();

    //Prepare variables for rescaling image (if it is larger than 'size')
    if ( w > size && w >= h ) {
        h = size;
        w = size*pm->width()/pm->height();
    } else if ( h > size && h > w ) {
        w = size;
        h = size*pm->height()/pm->width();
    }
    sx = (w - size)/2;
    sy = (h - size)/2;
    if ( sx < 0 ) { rx = -sx; sx = 0; }
    if ( sy < 0 ) { ry = -sy; sy = 0; }

    if ( setImage ) bigSize = int( 200.*float(pm->width())/float(w) );

    QPixmap result( size, size );
    result.fill( QColor( "white" ) ); //in case final image is smaller than 'size'

    if ( pm->width() > size || pm->height() > size ) { //image larger than 'size'?
        //convert to QImage so we can smoothscale it
        QImage im( pm->toImage() );
        im = im.scaled( w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );

        //bitBlt sizexsize square section of image
        QPainter p;
        p.begin( &result );
        p.drawImage( rx, ry, im, sx, sy, size, size );
        p.end();

        if ( setImage ) {
            bx = int( sx*float(pm->width())/float(w) );
            by = int( sy*float(pm->width())/float(w) );
            ImageRect->setRect( bx, by, bigSize, bigSize );
        }

    } else { //image is smaller than size x size
        QPainter p;
        p.begin( &result );
        p.drawImage( rx, ry, pm->toImage() );
        p.end();

        if ( setImage ) {
            bx = int( rx*float(pm->width())/float(w) );
            by = int( ry*float(pm->width())/float(w) );
            ImageRect->setRect( bx, by, bigSize, bigSize );
        }
    }

    return result;
}

void ThumbnailPicker::slotEditImage() {
    QPointer<ThumbnailEditor> te = new ThumbnailEditor( this, wid, ht );
    if ( te->exec() == QDialog::Accepted ) {
        QPixmap pm = te->thumbnail();
        *Image = pm;
        ui->CurrentImage->setPixmap( pm );
        ui->CurrentImage->update();
    }
    delete te;
}

void ThumbnailPicker::slotUnsetImage() {
    QFile file;
    if ( KSUtils::openDataFile( file, "noimage.png" ) ) {
        file.close();
        Image->load( file.fileName(), "PNG" );
    } else {
        *Image = Image->scaled( dd->thumbnail()->width(), dd->thumbnail()->height() );
        Image->fill( dd->palette().color( QPalette::Window ) );
    }

    ui->EditButton->setEnabled( false );
    ui->CurrentImage->setPixmap( *Image );
    ui->CurrentImage->update();

    bImageFound = false;
}

void ThumbnailPicker::slotSetFromList( int i ) {
    //Display image in preview pane
    QPixmap pm;
    pm = shrinkImage( PixList[i], 200, true ); //scale image
    SelectedImageIndex = i;

    ui->CurrentImage->setPixmap( pm );
    ui->CurrentImage->update();
    ui->EditButton->setEnabled( true );

    *Image = PixList[i]->scaled( wid, ht, Qt::IgnoreAspectRatio, Qt::SmoothTransformation ); 
    bImageFound = true;
}

void ThumbnailPicker::slotSetFromURL() {
    //Attempt to load the specified URL
    QUrl u = ui->ImageURLBox->url();

    if ( u.isValid() ) {
        if ( u.isLocalFile() ) {
            QFile localFile( u.toLocalFile() );

            //Add image to list
            //If image is taller than desktop, rescale it.
            QImage im( localFile.fileName() );

            if ( im.isNull() ) {
                KMessageBox::sorry( 0,
                                    xi18n("Failed to load image at %1", localFile.fileName() ),
                                    xi18n("Failed to load image") );
                return;
            }

            uint w = im.width();
            uint h = im.height();
            uint pad = 0;/* FIXME later 4*marginHint() + 2*ui->SearchLabel->height() + actionButton( Ok )->height() + 25; */
            uint hDesk = QApplication::desktop()->availableGeometry().height() - pad;

            if ( h > hDesk )
                im = im.scaled( w*hDesk/h, hDesk, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );

            //Add Image to top of list and 50x50 thumbnail image and URL to top of listbox
            PixList.insert( 0, new QPixmap( QPixmap::fromImage( im ) ) );
            ui->ImageList->insertItem( 0, new QListWidgetItem ( QIcon(shrinkImage( PixList.last(), 50 )), u.prettyUrl() ));

            //Select the new image
            ui->ImageList->setCurrentRow( 0 );
            slotSetFromList(0);

        } else {
            KIO::StoredTransferJob *j = KIO::storedGet( u, KIO::NoReload, KIO::HideProgressInfo );
            j->setUiDelegate(0);
            connect( j, SIGNAL( result(KJob*) ), SLOT( slotJobResult(KJob*) ) );
        }
    }
}

#include "thumbnailpicker.moc"
