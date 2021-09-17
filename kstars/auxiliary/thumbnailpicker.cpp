/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "thumbnailpicker.h"

#include "kstarsdata.h"
#include "ksutils.h"
#include "ksnotification.h"
#include "skyobjectuserdata.h"
#include "thumbnaileditor.h"
#include "dialogs/detaildialog.h"
#include "skyobjects/skyobject.h"

#include <KIO/CopyJob>
#include <KMessageBox>
#include <KJobUiDelegate>

#include <QDebug>

#include <QLineEdit>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QUrlQuery>

ThumbnailPickerUI::ThumbnailPickerUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

ThumbnailPicker::ThumbnailPicker(SkyObject *o, const QPixmap &current, QWidget *parent, double _w, double _h,
                                 QString cap)
    : QDialog(parent), SelectedImageIndex(-1), Object(o), bImageFound(false)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    thumbWidth  = _w;
    thumbHeight = _h;
    Image       = new QPixmap(current.scaled(_w, _h, Qt::KeepAspectRatio, Qt::FastTransformation));
    ImageRect   = new QRect(0, 0, 200, 200);

    ui = new ThumbnailPickerUI(this);

    setWindowTitle(cap);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    ui->CurrentImage->setPixmap(*Image);

    connect(ui->EditButton, SIGNAL(clicked()), this, SLOT(slotEditImage()));
    connect(ui->UnsetButton, SIGNAL(clicked()), this, SLOT(slotUnsetImage()));
    connect(ui->ImageList, SIGNAL(currentRowChanged(int)), this, SLOT(slotSetFromList(int)));
    connect(ui->ImageURLBox, SIGNAL(urlSelected(QUrl)), this, SLOT(slotSetFromURL()));
    connect(ui->ImageURLBox, SIGNAL(returnPressed()), this, SLOT(slotSetFromURL()));

    //ui->ImageURLBox->lineEdit()->setTrapReturnKey( true );
    ui->EditButton->setEnabled(false);

    slotFillList();
}

ThumbnailPicker::~ThumbnailPicker()
{
    while (!PixList.isEmpty())
        delete PixList.takeFirst();
}

//Query online sources for images of the object
void ThumbnailPicker::slotFillList()
{
    //Query Google Image Search:

    //Search for the primary name, or longname and primary name
    QString sName = QString("%1 ").arg(Object->name());
    if (Object->longname() != Object->name())
    {
        sName = QString("%1 ").arg(Object->longname()) + sName;
    }
    QString query =
        QString("http://www.google.com/search?q=%1&tbs=itp:photo,isz:ex,iszw:200,iszh:200&tbm=isch&source=lnt")
        .arg(sName);
    QUrlQuery gURL(query);

    //gURL.addQueryItem( "q", sName ); //add the Google-image query string

    parseGooglePage(gURL.query());
}

void ThumbnailPicker::slotProcessGoogleResult(KJob *result)
{
    //Preload ImageList with the URLs in the object's ImageList:
    SkyObjectUserdata::LinkList ImageList{
        KStarsData::Instance()->getUserData(Object->name()).images()
    };

    if (result->error())
    {
        result->uiDelegate()->showErrorMessage();
        result->kill();
        return;
    }

    QString PageHTML(static_cast<KIO::StoredTransferJob *>(result)->data());

    int index = PageHTML.indexOf("src=\"http:", 0);
    while (index >= 0)
    {
        index += 5; //move to end of "src=\"http:" marker

        //Image URL is everything from index to next occurrence of "\""
        ImageList.push_back(SkyObjectUserdata::LinkData{
            "", QUrl{ PageHTML.mid(index, PageHTML.indexOf("\"", index) - index) },
            SkyObjectUserdata::Type::website });

        index = PageHTML.indexOf("src=\"http:", index);
    }

    //Total Number of images to be loaded:
    int nImages = ImageList.size();
    if (nImages)
    {
        ui->SearchProgress->setMinimum(0);
        ui->SearchProgress->setMaximum(nImages - 1);
        ui->SearchLabel->setText(i18n("Loading images..."));
    }
    else
    {
        close();
        return;
    }

    //Add images from the ImageList
    for (const auto &image : ImageList)
    {
        const QUrl &u{ image.url };

        if (u.isValid())
        {
            KIO::StoredTransferJob *j =
                KIO::storedGet(u, KIO::NoReload, KIO::HideProgressInfo);
            j->setUiDelegate(nullptr);
            connect(j, SIGNAL(result(KJob *)), SLOT(slotJobResult(KJob *)));
        }
    }
}

void ThumbnailPicker::slotJobResult(KJob *job)
{
    KIO::StoredTransferJob *stjob = (KIO::StoredTransferJob *)job;

    //Update Progressbar
    if (!ui->SearchProgress->isHidden())
    {
        ui->SearchProgress->setValue(ui->SearchProgress->value() + 1);
        if (ui->SearchProgress->value() == ui->SearchProgress->maximum())
        {
            ui->SearchProgress->hide();
            ui->SearchLabel->setText(i18n("Search results:"));
        }
    }

    //If there was a problem, just return silently without adding image to list.
    if (job->error())
    {
        qDebug() << " error=" << job->error();
        job->kill();
        return;
    }

    QPixmap *pm = new QPixmap();
    pm->loadFromData(stjob->data());

    uint w = pm->width();
    uint h = pm->height();
    uint pad =
        0; /*FIXME LATER 4* QDialogBase::marginHint() + 2*ui->SearchLabel->height() + QDialogBase::actionButton( QDialogBase::Ok )->height() + 25;*/
    uint hDesk = QGuiApplication::primaryScreen()->geometry().height() - pad;

    if (h > hDesk)
        *pm = pm->scaled(w * hDesk / h, hDesk, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    PixList.append(pm);

    //Add 50x50 image and URL to listbox
    //ui->ImageList->insertItem( shrinkImage( PixList.last(), 50 ),
    //		cjob->srcURLs().first().prettyUrl() );
    ui->ImageList->addItem(new QListWidgetItem(QIcon(shrinkImage(PixList.last(), 200)), stjob->url().url()));
}

//void ThumbnailPicker::parseGooglePage( QStringList &ImList, const QString &URL )
void ThumbnailPicker::parseGooglePage(const QString &URL)
{
    QUrl googleURL(URL);
    KIO::StoredTransferJob *job = KIO::storedGet(googleURL);
    connect(job, SIGNAL(result(KJob*)), this, SLOT(slotProcessGoogleResult(KJob*)));

    job->start();
}

QPixmap ThumbnailPicker::shrinkImage(QPixmap *pm, int size, bool setImage)
{
    int w(pm->width()), h(pm->height());
    int bigSize(w);
    int rx(0), ry(0), sx(0), sy(0), bx(0), by(0);
    if (size == 0)
        return QPixmap();

    //Prepare variables for rescaling image (if it is larger than 'size')
    if (w > size && w >= h)
    {
        h = size;
        w = size * pm->width() / pm->height();
    }
    else if (h > size && h > w)
    {
        w = size;
        h = size * pm->height() / pm->width();
    }
    sx = (w - size) / 2;
    sy = (h - size) / 2;
    if (sx < 0)
    {
        rx = -sx;
        sx = 0;
    }
    if (sy < 0)
    {
        ry = -sy;
        sy = 0;
    }

    if (setImage)
        bigSize = int(200. * float(pm->width()) / float(w));

    QPixmap result(size, size);
    result.fill(Qt::black); //in case final image is smaller than 'size'

    if (pm->width() > size || pm->height() > size) //image larger than 'size'?
    {
        //convert to QImage so we can smoothscale it
        QImage im(pm->toImage());
        im = im.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        //bitBlt sizexsize square section of image
        QPainter p;
        p.begin(&result);
        p.drawImage(rx, ry, im, sx, sy, size, size);
        p.end();

        if (setImage)
        {
            bx = int(sx * float(pm->width()) / float(w));
            by = int(sy * float(pm->width()) / float(w));
            ImageRect->setRect(bx, by, bigSize, bigSize);
        }
    }
    else //image is smaller than size x size
    {
        QPainter p;
        p.begin(&result);
        p.drawImage(rx, ry, pm->toImage());
        p.end();

        if (setImage)
        {
            bx = int(rx * float(pm->width()) / float(w));
            by = int(ry * float(pm->width()) / float(w));
            ImageRect->setRect(bx, by, bigSize, bigSize);
        }
    }

    return result;
}

void ThumbnailPicker::slotEditImage()
{
    QPointer<ThumbnailEditor> te = new ThumbnailEditor(this, thumbWidth, thumbHeight);
    if (te->exec() == QDialog::Accepted)
    {
        QPixmap pm = te->thumbnail();
        *Image     = pm;
        ui->CurrentImage->setPixmap(pm);
        ui->CurrentImage->update();
    }
    delete te;
}

void ThumbnailPicker::slotUnsetImage()
{
    //  QFile file;
    //if ( KSUtils::openDataFile( file, "noimage.png" ) ) {
    //    file.close();
    //     Image->load( file.fileName(), "PNG" );
    // } else {
    //     *Image = Image->scaled( dd->thumbnail()->width(), dd->thumbnail()->height() );
    //      Image->fill( dd->palette().color( QPalette::Window ) );
    //  }

    QPixmap noImage;
    noImage.load(":/images/noimage.png");
    Image = new QPixmap(noImage.scaled(thumbWidth, thumbHeight, Qt::KeepAspectRatio, Qt::FastTransformation));

    ui->EditButton->setEnabled(false);
    ui->CurrentImage->setPixmap(*Image);
    ui->CurrentImage->update();

    bImageFound = false;
}

void ThumbnailPicker::slotSetFromList(int i)
{
    //Display image in preview pane
    QPixmap pm;
    pm                 = shrinkImage(PixList[i], 200, true); //scale image
    SelectedImageIndex = i;

    ui->CurrentImage->setPixmap(pm);
    ui->CurrentImage->update();
    ui->EditButton->setEnabled(true);

    *Image      = PixList[i]->scaled(thumbWidth, thumbHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    bImageFound = true;
}

void ThumbnailPicker::slotSetFromURL()
{
    //Attempt to load the specified URL
    QUrl u = ui->ImageURLBox->url();

    if (u.isValid())
    {
        if (u.isLocalFile())
        {
            QFile localFile(u.toLocalFile());

            //Add image to list
            //If image is taller than desktop, rescale it.
            QImage im(localFile.fileName());

            if (im.isNull())
            {
                KSNotification::sorry(i18n("Failed to load image at %1", localFile.fileName()), i18n("Failed to load image"));
                return;
            }

            uint w = im.width();
            uint h = im.height();
            uint pad =
                0; /* FIXME later 4*marginHint() + 2*ui->SearchLabel->height() + actionButton( Ok )->height() + 25; */
            uint hDesk = QGuiApplication::primaryScreen()->geometry().height() - pad;

            if (h > hDesk)
                im = im.scaled(w * hDesk / h, hDesk, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

            //Add Image to top of list and 50x50 thumbnail image and URL to top of listbox
            PixList.insert(0, new QPixmap(QPixmap::fromImage(im)));
            ui->ImageList->insertItem(0, new QListWidgetItem(QIcon(shrinkImage(PixList.last(), 50)), u.url()));

            //Select the new image
            ui->ImageList->setCurrentRow(0);
            slotSetFromList(0);
        }
        else
        {
            KIO::StoredTransferJob *j = KIO::storedGet(u, KIO::NoReload, KIO::HideProgressInfo);
            j->setUiDelegate(nullptr);
            connect(j, SIGNAL(result(KJob*)), SLOT(slotJobResult(KJob*)));
        }
    }
}
