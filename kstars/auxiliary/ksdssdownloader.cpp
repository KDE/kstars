/*
    SPDX-FileCopyrightText: 2016 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ksdssdownloader.h"

#include "Options.h"
#include "auxiliary/filedownloader.h"
#include "catalogobject.h"

#include <QImageWriter>
#include <QMimeDatabase>


KSDssDownloader::KSDssDownloader(QObject *parent) : QObject(parent)
{
    connect(this, &KSDssDownloader::downloadCanceled, this, [&]() { deleteLater(); });
    m_VersionPreference << "poss2ukstu_blue"
                        << "poss2ukstu_red"
                        << "poss1_blue"
                        << "poss1_red"
                        << "quickv"
                        << "poss2ukstu_ir";
    m_TempFile.open();
}

KSDssDownloader::KSDssDownloader(const SkyPoint *const p, const QString &destFileName,
                                 const std::function<void(bool)> &slotDownloadReady, QObject *parent)
    : QObject(parent)
{
    // Initialize version preferences. FIXME: This must be made a
    // user-changeable option just in case someone likes red

    connect(this, &KSDssDownloader::downloadCanceled, this, [&]() { deleteLater(); });

    m_VersionPreference << "poss2ukstu_blue"
                        << "poss2ukstu_red"
                        << "poss1_blue"
                        << "poss1_red"
                        << "quickv"
                        << "poss2ukstu_ir";
    m_TempFile.open();
    connect(this, &KSDssDownloader::downloadComplete, slotDownloadReady);
    startDownload(p, destFileName);
}

QString KSDssDownloader::getDSSURL(const SkyPoint *const p, const QString &version, struct KSDssImage::Metadata *md)
{
    double height, width;

    double dss_default_size = Options::defaultDSSImageSize();
    double dss_padding      = Options::dSSPadding();

    Q_ASSERT(p);
    Q_ASSERT(dss_default_size > 0.0 && dss_padding >= 0.0);

    const auto * dso = dynamic_cast<const CatalogObject *>(p);

    // Decide what to do about the height and width
    if (dso)
    {
        // For deep-sky objects, use their height and width information
        double a, b, pa;
        a = dso->a();
        b = dso->a() *
            dso->e(); // Use a * e instead of b, since e() returns 1 whenever one of the dimensions is zero. This is important for circular objects
        Q_ASSERT(a >= b);
        pa = dso->pa() * dms::DegToRad;

        // We now want to convert a, b, and pa into an image
        // height and width -- i.e. a dRA and dDec.
        // DSS uses dDec for height and dRA for width. (i.e. "top" is north in the DSS images, AFAICT)
        // From some trigonometry, assuming we have a rectangular object (worst case), we need:
        width  = a * sin(pa) + b * fabs(cos(pa));
        height = a * fabs(cos(pa)) + b * sin(pa);
        // 'a' and 'b' are in arcminutes, so height and width are in arcminutes

        // Pad the RA and Dec, so that we show more of the sky than just the object.
        height += dss_padding;
        width += dss_padding;
    }
    else
    {
        // For a generic sky object, we don't know what to do. So
        // we just assume the default size.
        height = width = dss_default_size;
    }
    // There's no point in tiny DSS images that are smaller than dss_default_size
    if (height < dss_default_size)
        height = dss_default_size;
    if (width < dss_default_size)
        width = dss_default_size;

    return getDSSURL(p, width, height, version, md);
}

QString KSDssDownloader::getDSSURL(const SkyPoint *const p, float width, float height, const QString &version,
                                   struct KSDssImage::Metadata *md)
{
    Q_ASSERT(p);
    if (!p)
        return QString();
    if (width <= 0)
        return QString();
    if (height <= 0)
        height = width;
    QString URL = getDSSURL(p->ra0(), p->dec0(), width, height, "gif", version, md);
    if (md)
    {
        const SkyObject *obj = nullptr;
        obj                  = dynamic_cast<const SkyObject *>(p);
        if (obj && obj->hasName())
            md->object = obj->name();
    }
    return URL;
}

QString KSDssDownloader::getDSSURL(const dms &ra, const dms &dec, float width, float height, const QString &type_,
                                   const QString &version_, struct KSDssImage::Metadata *md)
{
    const double dss_default_size = Options::defaultDSSImageSize();

    QString version   = version_.toLower();
    QString type      = type_.toLower();
    QString URLprefix = QString("https://archive.stsci.edu/cgi-bin/dss_search?v=%1&").arg(version);
    QString URLsuffix = QString("&e=J2000&f=%1&c=none&fov=NONE").arg(type);

    char decsgn = (dec.Degrees() < 0.0) ? '-' : '+';
    int dd      = abs(dec.degree());
    int dm      = abs(dec.arcmin());
    int ds      = abs(dec.arcsec());

    // Infinite and NaN sizes are replaced by the default size and tiny DSS images are resized to default size
    if (!qIsFinite(height) || height <= 0.0)
        height = dss_default_size;
    if (!qIsFinite(width) || width <= 0.0)
        width = dss_default_size;

    // DSS accepts images that are no larger than 75 arcminutes
    if (height > 75.0)
        height = 75.0;
    if (width > 75.0)
        width = 75.0;

    QString RAString, DecString, SizeString;
    DecString  = QString::asprintf("&d=%c%02d+%02d+%02d", decsgn, dd, dm, ds);
    RAString   = QString::asprintf("r=%02d+%02d+%02d", ra.hour(), ra.minute(), ra.second());
    SizeString  = QString::asprintf("&h=%02.1f&w=%02.1f", height, width);

    if (md)
    {
        md->src     = KSDssImage::Metadata::DSS;
        md->width   = width;
        md->height  = height;
        md->ra0     = ra;
        md->dec0    = dec;
        md->version = version;
        md->format  = (type.contains("fit") ? KSDssImage::Metadata::FITS : KSDssImage::Metadata::GIF);
        md->height  = height;
        md->width   = width;

        if (version.contains("poss2"))
            md->gen = 2;
        else if (version.contains("poss1"))
            md->gen = 1;
        else if (version.contains("quickv"))
            md->gen = 4;
        else
            md->gen = -1;

        if (version.contains("red"))
            md->band = 'R';
        else if (version.contains("blue"))
            md->band = 'B';
        else if (version.contains("ir"))
            md->band = 'I';
        else if (version.contains("quickv"))
            md->band = 'V';
        else
            md->band = '?';

        md->object = QString();
    }

    return (URLprefix + RAString + DecString + SizeString + URLsuffix);
}

void KSDssDownloader::initiateSingleDownloadAttempt(QUrl srcUrl)
{
    qDebug() << "Temp file is at " << m_TempFile.fileName();
    QUrl fileUrl = QUrl::fromLocalFile(m_TempFile.fileName());
    qDebug() << "Attempt #" << m_attempt << "downloading DSS Image. URL: " << srcUrl << " to " << fileUrl;
    //m_DownloadJob = KIO::copy( srcUrl, fileUrl, KIO::Overwrite ) ; // FIXME: Can be done with pure Qt
    //connect ( m_DownloadJob, SIGNAL (result(KJob*)), SLOT (downloadAttemptFinished()) );

    downloadJob = new FileDownloader();

    downloadJob->setProgressDialogEnabled(true, i18n("DSS Download"),
                                          i18n("Please wait while DSS image is being downloaded..."));
    connect(downloadJob, SIGNAL(canceled()), this, SIGNAL(downloadCanceled()));
    connect(downloadJob, SIGNAL(downloaded()), this, SLOT(downloadAttemptFinished()));
    connect(downloadJob, SIGNAL(error(QString)), this, SLOT(downloadError(QString)));

    downloadJob->get(srcUrl);
}

void KSDssDownloader::startDownload(const SkyPoint *const p, const QString &destFileName)
{
    QUrl srcUrl;
    m_FileName = destFileName;
    m_attempt  = 0;
    srcUrl.setUrl(getDSSURL(p, m_VersionPreference[m_attempt], &m_AttemptData));
    initiateSingleDownloadAttempt(srcUrl);
}

void KSDssDownloader::startSingleDownload(const QUrl srcUrl, const QString &destFileName, KSDssImage::Metadata &md)
{
    m_FileName   = destFileName;
    QUrl fileUrl = QUrl::fromLocalFile(m_TempFile.fileName());
    qDebug() << "Downloading DSS Image from URL: " << srcUrl << " to " << fileUrl;
    //m_DownloadJob = KIO::copy( srcUrl, fileUrl, KIO::Overwrite ) ; // FIXME: Can be done with pure Qt
    //connect ( m_DownloadJob, SIGNAL (result(KJob*)), SLOT (singleDownloadFinished()) );

    downloadJob = new FileDownloader();

    downloadJob->setProgressDialogEnabled(true, i18n("DSS Download"),
                                          i18n("Please wait while DSS image is being downloaded..."));
    connect(downloadJob, SIGNAL(canceled()), this, SIGNAL(downloadCanceled()));
    connect(downloadJob, SIGNAL(downloaded()), this, SLOT(singleDownloadFinished()));
    connect(downloadJob, SIGNAL(error(QString)), this, SLOT(downloadError(QString)));

    m_AttemptData = md;

    downloadJob->get(srcUrl);
}

void KSDssDownloader::downloadError(const QString &errorString)
{
    qDebug() << "Error " << errorString << " downloading DSS images!";
    emit downloadComplete(false);
    downloadJob->deleteLater();
}

void KSDssDownloader::singleDownloadFinished()
{
    m_TempFile.open();
    m_TempFile.write(downloadJob->downloadedData());
    m_TempFile.close();
    downloadJob->deleteLater();

    // Check if we have a proper DSS image or the DSS server failed
    QMimeDatabase mdb;
    QMimeType mt = mdb.mimeTypeForFile(m_TempFile.fileName(), QMimeDatabase::MatchContent);
    if (mt.name().contains("image", Qt::CaseInsensitive))
    {
        qDebug() << "DSS download was successful";
        emit downloadComplete(writeImageWithMetadata(m_TempFile.fileName(), m_FileName, m_AttemptData));
        return;
    }
    else
        emit downloadComplete(false);
}

void KSDssDownloader::downloadAttemptFinished()
{
    if (m_AttemptData.src == KSDssImage::Metadata::SDSS)
    {
        // FIXME: do SDSS-y things
        emit downloadComplete(false);
        deleteLater();
        downloadJob->deleteLater();
        return;
    }
    else
    {
        m_TempFile.open();
        m_TempFile.write(downloadJob->downloadedData());
        m_TempFile.close();
        downloadJob->deleteLater();

        // Check if we have a proper DSS image or the DSS server failed
        QMimeDatabase mdb;
        QMimeType mt = mdb.mimeTypeForFile(m_TempFile.fileName(), QMimeDatabase::MatchContent);
        if (mt.name().contains("image", Qt::CaseInsensitive))
        {
            qDebug() << "DSS download was successful";
            emit downloadComplete(writeImageFile());
            deleteLater();
            return;
        }

        // We must have failed, try the next attempt
        QUrl srcUrl;
        m_attempt++;
        if (m_attempt == m_VersionPreference.count())
        {
            // Nothing downloaded... very strange. Fail.
            qDebug() << "Error downloading DSS images: All alternatives failed!";
            emit downloadComplete(false);
            deleteLater();
            return;
        }
        srcUrl.setUrl(getDSSURL(m_AttemptData.ra0, m_AttemptData.dec0, m_AttemptData.width, m_AttemptData.height,
                                ((m_AttemptData.format == KSDssImage::Metadata::FITS) ? "fits" : "gif"),
                                m_VersionPreference[m_attempt], &m_AttemptData));
        initiateSingleDownloadAttempt(srcUrl);
    }
}

bool KSDssDownloader::writeImageFile()
{
    return writeImageWithMetadata(m_TempFile.fileName(), m_FileName, m_AttemptData);
}

bool KSDssDownloader::writeImageWithMetadata(const QString &srcFile, const QString &destFile,
                                             const KSDssImage::Metadata &md)
{
    // Write the temporary file into an image file with metadata
    QImage img(srcFile);
    QImageWriter writer(destFile, "png");

    writer.setText("Calibrated",
                   "true");    // This means that the image has RA/Dec size and orientation that is calibrated
    writer.setText("PA", "0"); // Position Angle is zero degrees for DSS images
    writer.setText("Source", QString::number(KSDssImage::Metadata::DSS));
    writer.setText("Format", QString::number(KSDssImage::Metadata::PNG));
    writer.setText("Version", md.version);
    writer.setText("Object", md.object);
    writer.setText("RA", md.ra0.toHMSString(true));
    writer.setText("Dec", md.dec0.toDMSString(true, true));
    writer.setText("Width", QString::number(md.width));
    writer.setText("Height", QString::number(md.height));
    writer.setText("Band", QString() + md.band);
    writer.setText("Generation", QString::number(md.gen));
    writer.setText("Author", "KStars KSDssDownloader");
    return writer.write(img);
}
