/*
    SPDX-FileCopyrightText: 2001 Thomas Kabelmann <tk78@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "imageviewer.h"
#include "Options.h"

#ifndef KSTARS_LITE
#include "kstars.h"
#include <KMessageBox>
#include <KFormat>
#endif

#include "ksnotification.h"
#include <QFileDialog>
#include <QJsonObject>
#include <QPainter>
#include <QResizeEvent>
#include <QScreen>
#include <QStatusBar>
#include <QTemporaryFile>
#include <QVBoxLayout>
#include <QPushButton>
#include <QApplication>

QUrl ImageViewer::lastURL = QUrl::fromLocalFile(QDir::homePath());

ImageLabel::ImageLabel(QWidget *parent) : QFrame(parent)
{
#ifndef KSTARS_LITE
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
    setLineWidth(2);
#endif
}

void ImageLabel::setImage(const QImage &img)
{
#ifndef KSTARS_LITE
    m_Image = img;
    pix     = QPixmap::fromImage(m_Image);
#endif
}

void ImageLabel::invertPixels()
{
#ifndef KSTARS_LITE
    m_Image.invertPixels();
    pix = QPixmap::fromImage(m_Image.scaled(width(), height(), Qt::KeepAspectRatio));
#endif
}

void ImageLabel::paintEvent(QPaintEvent *)
{
#ifndef KSTARS_LITE
    QPainter p;
    p.begin(this);
    int x = 0;
    if (pix.width() < width())
        x = (width() - pix.width()) / 2;
    p.drawPixmap(x, 0, pix);
    p.end();
#endif
}

void ImageLabel::resizeEvent(QResizeEvent *event)
{
    int w = pix.width();
    int h = pix.height();

    if (event->size().width() == w && event->size().height() == h)
        return;

    pix = QPixmap::fromImage(m_Image.scaled(event->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

ImageViewer::ImageViewer(const QString &caption, QWidget *parent) : QDialog(parent), fileIsImage(false), downloadJob(nullptr)
{
#ifndef KSTARS_LITE
    init(caption, QString());
#endif
}

ImageViewer::ImageViewer(const QUrl &url, const QString &capText, QWidget *parent)
    : QDialog(parent), m_ImageUrl(url)
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
        file.setFileName(tempfile.fileName());
    } // we just need the name and delete the tempfile from disc; if we don't do it, a dialog will be show

    loadImageFromURL();
#endif
}

void ImageViewer::init(QString caption, QString capText)
{
#ifndef KSTARS_LITE
    setAttribute(Qt::WA_DeleteOnClose, true);
    setModal(false);
    setWindowTitle(i18nc("@title:window", "KStars image viewer: %1", caption));

    // Create widget
    QFrame *page = new QFrame(this);

    //setMainWidget( page );
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(page);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QPushButton *invertB = new QPushButton(i18n("Invert colors"));
    invertB->setToolTip(i18n("Reverse colors of the image. This is useful to enhance contrast at times. This affects "
                             "only the display and not the saving."));
    QPushButton *saveB = new QPushButton(QIcon::fromTheme("document-save"), i18n("Save"));
    saveB->setToolTip(i18n("Save the image to disk"));

    buttonBox->addButton(invertB, QDialogButtonBox::ActionRole);
    buttonBox->addButton(saveB, QDialogButtonBox::ActionRole);

    connect(invertB, SIGNAL(clicked()), this, SLOT(invertColors()));
    connect(saveB, SIGNAL(clicked()), this, SLOT(saveFileToDisc()));

    m_View = new ImageLabel(page);
    m_View->setAutoFillBackground(true);
    m_Caption = new QLabel(page);
    m_Caption->setAutoFillBackground(true);
    m_Caption->setFrameShape(QFrame::StyledPanel);
    m_Caption->setText(capText);
    // Add layout
    QVBoxLayout *vlay = new QVBoxLayout(page);
    vlay->setSpacing(0);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->addWidget(m_View);
    vlay->addWidget(m_Caption);

    //Reverse colors
    QPalette p = palette();
    p.setColor(QPalette::Window, palette().color(QPalette::WindowText));
    p.setColor(QPalette::WindowText, palette().color(QPalette::Window));
    m_Caption->setPalette(p);
    m_View->setPalette(p);

    //If the caption is wider than the image, try to shrink the font a bit
    QFont capFont = m_Caption->font();
    capFont.setPointSize(capFont.pointSize() - 2);
    m_Caption->setFont(capFont);
#endif
}

ImageViewer::~ImageViewer()
{
    QString filename = file.fileName();
    if (filename.startsWith(QLatin1String("/tmp/")) || filename.contains("/Temp"))
    {
        if (m_ImageUrl.isEmpty() == false ||
                KMessageBox::questionYesNo(nullptr, i18n("Remove temporary file %1 from disk?", filename),
                                           i18n("Confirm Removal"), KStandardGuiItem::yes(), KStandardGuiItem::no(),
                                           "imageviewer_temporary_file_removal") == KMessageBox::Yes)
            QFile::remove(filename);
    }

    QApplication::restoreOverrideCursor();
}

void ImageViewer::loadImageFromURL()
{
#ifndef KSTARS_LITE
    QUrl saveURL = QUrl::fromLocalFile(file.fileName());

    if (!saveURL.isValid())
        qWarning() << "tempfile-URL is malformed";

    QApplication::setOverrideCursor(Qt::WaitCursor);

    downloadJob.setProgressDialogEnabled(true, i18n("Download"),
                                         i18n("Please wait while image is being downloaded..."));
    connect(&downloadJob, SIGNAL(downloaded()), this, SLOT(downloadReady()));
    connect(&downloadJob, SIGNAL(canceled()), this, SLOT(close()));
    connect(&downloadJob, SIGNAL(error(QString)), this, SLOT(downloadError(QString)));

    downloadJob.get(m_ImageUrl);
#endif
}

void ImageViewer::downloadReady()
{
#ifndef KSTARS_LITE
    QApplication::restoreOverrideCursor();

    if (file.open(QFile::WriteOnly))
    {
        file.write(downloadJob.downloadedData());
        file.close(); // to get the newest information from the file and not any information from opening of the file

        if (file.exists())
        {
            showImage();
            return;
        }

        close();
    }
    else
        KSNotification::error(file.errorString(), i18n("Image Viewer"));
#endif
}

void ImageViewer::downloadError(const QString &errorString)
{
#ifndef KSTARS_LITE
    QApplication::restoreOverrideCursor();
    KSNotification::error(errorString);
#endif
}

bool ImageViewer::loadImage(const QString &filename)
{
#ifndef KSTARS_LITE
    // If current file is temporary, remove from disk.
    if (file.fileName().startsWith(QLatin1String("/tmp/")) || file.fileName().contains("/Temp"))
        QFile::remove(file.fileName());

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

    if (!image.load(file.fileName()))
    {
        KSNotification::error(i18n("Loading of the image %1 failed.", m_ImageUrl.url()));
        close();
        return false;
    }

    fileIsImage = true; // we loaded the file and know now, that it is an image

    //If the image is larger than screen width and/or screen height,
    //shrink it to fit the screen
    QRect deskRect = QGuiApplication::primaryScreen()->geometry();
    int w          = deskRect.width();  // screen width
    int h          = deskRect.height(); // screen height

    if (image.width() <= w && image.height() > h) //Window is taller than desktop
        image = image.scaled(int(image.width() * h / image.height()), h);
    else if (image.height() <= h && image.width() > w) //window is wider than desktop
        image = image.scaled(w, int(image.height() * w / image.width()));
    else if (image.width() > w && image.height() > h) //window is too tall and too wide
    {
        //which needs to be shrunk least, width or height?
        float fx = float(w) / float(image.width());
        float fy = float(h) / float(image.height());
        if (fx > fy) //width needs to be shrunk less, so shrink to fit in height
            image = image.scaled(int(image.width() * fy), h);
        else //vice versa
            image = image.scaled(w, int(image.height() * fx));
    }

    show(); // hide is default

    m_View->setImage(image);
    w = image.width();

    //If the caption is wider than the image, set the window size
    //to fit the caption
    if (m_Caption->width() > w)
        w = m_Caption->width();
    //setFixedSize( w, image.height() + m_Caption->height() );

    resize(w, image.height());
    update();
    show();

    return true;
#else
    return false;
#endif
}

void ImageViewer::saveFileToDisc()
{
#ifndef KSTARS_LITE
    QFileDialog dialog;

    QUrl newURL =
        dialog.getSaveFileUrl(KStars::Instance(), i18nc("@title:window", "Save Image"), lastURL); // save-dialog with default filename
    if (!newURL.isEmpty())
    {
        //QFile f (newURL.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash).toLocalFile() + '/' +  newURL.fileName());
        QFile f(newURL.toLocalFile());
        if (f.exists())
        {
            if ((KMessageBox::warningContinueCancel(static_cast<QWidget *>(parent()),
                                                    i18n("A file named \"%1\" already exists. "
                                                            "Overwrite it?",
                                                            newURL.fileName()),
                                                    i18n("Overwrite File?"), KStandardGuiItem::overwrite()) == KMessageBox::Cancel))
                return;

            f.remove();
        }

        lastURL = QUrl(newURL.toString(QUrl::RemoveFilename));

        saveFile(newURL);
    }
#endif
}

void ImageViewer::saveFile(QUrl &url)
{
    // synchronous access to prevent segfaults

    //if (!KIO::NetAccess::file_copy (QUrl (file.fileName()), url, (QWidget*) 0))
    //QUrl tmpURL((file.fileName()));
    //tmpURL.setScheme("file");

    if (file.copy(url.toLocalFile()) == false)
        //if (KIO::file_copy(tmpURL, url)->exec() == false)
    {
        QString text = i18n("Saving of the image %1 failed.", url.toString());
#ifndef KSTARS_LITE
        KSNotification::error(text);
#else
        qDebug() << text;
#endif
    }
#ifndef KSTARS_LITE
    else
        KStars::Instance()->statusBar()->showMessage(i18n("Saved image to %1", url.toString()));
#endif
}

void ImageViewer::invertColors()
{
#ifndef KSTARS_LITE
    // Invert colors
    m_View->invertPixels();
    m_View->update();
#endif
}

QJsonObject ImageViewer::metadata()
{
#ifndef KSTARS_LITE
    QJsonObject md;
    if (m_View && !m_View->m_Image.isNull())
    {
        md.insert("resolution", QString("%1x%2").arg(m_View->m_Image.width()).arg(m_View->m_Image.height()));
        // sizeInBytes is only available in 5.10+
        md.insert("size", KFormat().formatByteSize(m_View->m_Image.bytesPerLine() * m_View->m_Image.height()));
        md.insert("bin", "1x1");
        md.insert("bpp", QString::number(m_View->m_Image.bytesPerLine() / m_View->m_Image.width() * 8));
    }
    return md;
#endif
}
