/*
    SPDX-FileCopyrightText: 2013 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/* Project Includes */
#include "imageexporter.h"
#include "kstars.h"
#include "skyqpainter.h"
#include "skymap.h"

#include <KJob>
#include <KIO/StoredTransferJob>

/* Qt Includes */
#include <QTemporaryFile>
#include <QStatusBar>
#include <QSvgGenerator>

ImageExporter::ImageExporter(QObject *parent) : QObject(parent), m_includeLegend(false), m_Size(nullptr)
{
    m_Legend = new Legend;

    // set font for legend labels
    m_Legend->setFont(QFont("Courier New", 8));

    // set up the default alpha
    setLegendAlpha(160);
}

void ImageExporter::exportSvg(const QString &fileName)
{
    SkyMap *map = SkyMap::Instance();

    // export as SVG
    QSvgGenerator svgGenerator;
    svgGenerator.setFileName(fileName);
    svgGenerator.setTitle(i18n("KStars Exported Sky Image"));
    svgGenerator.setDescription(i18n("KStars Exported Sky Image"));
    svgGenerator.setSize(QSize(map->width(), map->height()));
    svgGenerator.setResolution(qMax(map->logicalDpiX(), map->logicalDpiY()));
    svgGenerator.setViewBox(QRect(0, 0, map->width(), map->height()));

    SkyQPainter painter(KStars::Instance(), &svgGenerator);
    painter.begin();

    map->exportSkyImage(&painter);

    if (m_includeLegend)
    {
        addLegend(&painter);
    }

    painter.end();
}

bool ImageExporter::exportRasterGraphics(const QString &fileName)
{
    //Determine desired image format from filename extension
    QString ext = fileName.mid(fileName.lastIndexOf(".") + 1);

    // export as raster graphics
    const char *format = "PNG";

    if (ext.toLower() == "png")
    {
        format = "PNG";
    }
    else if (ext.toLower() == "jpg" || ext.toLower() == "jpeg")
    {
        format = "JPG";
    }
    else if (ext.toLower() == "gif")
    {
        format = "GIF";
    }
    else if (ext.toLower() == "pnm")
    {
        format = "PNM";
    }
    else if (ext.toLower() == "bmp")
    {
        format = "BMP";
    }
    else
    {
        qWarning() << "Could not parse image format of" << fileName << "assuming PNG";
    }

    SkyMap *map = SkyMap::Instance();

    int width, height;
    if (m_Size)
    {
        width  = m_Size->width();
        height = m_Size->height();
    }
    else
    {
        width  = map->width();
        height = map->height();
    }

    QPixmap skyimage(map->width(), map->height());
    QPixmap outimage(width, height);
    outimage.fill();

    map->exportSkyImage(&skyimage);
    qApp->processEvents();

    //skyImage is the size of the sky map.  The requested image size is w x h.
    //If w x h is smaller than the skymap, then we simply crop the image.
    //If w x h is larger than the skymap, pad the skymap image with a white border.
    if (width == map->width() && height == map->height())
    {
        outimage = skyimage.copy();
    }

    else
    {
        int dx(0), dy(0), sx(0), sy(0);
        int sw(map->width()), sh(map->height());

        if (width > map->width())
        {
            dx = (width - map->width()) / 2;
        }

        else
        {
            sx = (map->width() - width) / 2;
            sw = width;
        }

        if (height > map->height())
        {
            dy = (height - map->height()) / 2;
        }

        else
        {
            sy = (map->height() - height) / 2;
            sh = height;
        }

        QPainter p;
        p.begin(&outimage);
        p.fillRect(outimage.rect(), QBrush(Qt::white));
        p.drawImage(dx, dy, skyimage.toImage(), sx, sy, sw, sh);
        p.end();
    }

    if (m_includeLegend)
    {
        addLegend(&outimage);
    }

    if (!outimage.save(fileName, format))
    {
        m_lastErrorMessage = i18n("Error: Unable to save image: %1", fileName);
        qDebug() << m_lastErrorMessage;
        return false;
    }

    else
    {
        KStars::Instance()->statusBar()->showMessage(i18n("Saved image to %1", fileName));
        return true;
    }
}
void ImageExporter::addLegend(SkyQPainter *painter)
{
    m_Legend->paintLegend(painter);
}

void ImageExporter::addLegend(QPaintDevice *pd)
{
    SkyQPainter painter(KStars::Instance(), pd);
    painter.begin();

    addLegend(&painter);

    painter.end();
}

bool ImageExporter::exportImage(QString url)
{
    QUrl fileURL = QUrl::fromUserInput(url);

    m_lastErrorMessage = QString();
    if (fileURL.isValid())
    {
        QTemporaryFile tmpfile;
        QString fname;
        bool isLocalFile = fileURL.isLocalFile();

        if (isLocalFile)
        {
            fname = fileURL.toLocalFile();
        }

        else
        {
            tmpfile.open();
            fname = tmpfile.fileName();
        }

        //Determine desired image format from filename extension
        QString ext = fname.mid(fname.lastIndexOf(".") + 1);
        if (ext.toLower() == "svg")
        {
            exportSvg(fname);
        }

        else
        {
            return exportRasterGraphics(fname);
        }

        if (!isLocalFile)
        {
            //attempt to upload image to remote location
            KIO::StoredTransferJob *put_job = KIO::storedHttpPost(&tmpfile, fileURL, -1);
            //if(!KIO::NetAccess::upload(tmpfile.fileName(), fileURL, m_KStars))
            if (put_job->exec() == false)
            {
                m_lastErrorMessage = i18n("Could not upload image to remote location: %1", fileURL.url());
                qWarning() << m_lastErrorMessage;
                return false;
            }
        }
        return true;
    }
    m_lastErrorMessage = i18n("Could not export image: URL %1 invalid", fileURL.url());
    qWarning() << m_lastErrorMessage;
    return false;
}

void ImageExporter::setLegendProperties(Legend::LEGEND_TYPE type, Legend::LEGEND_ORIENTATION orientation,
                                        Legend::LEGEND_POSITION position, int alpha, bool include)
{
    // set background color (alpha)
    setLegendAlpha(alpha);
    // set legend orientation
    m_Legend->setOrientation(orientation);

    // set legend type
    m_Legend->setType(type);

    // set legend position
    m_Legend->setPosition(position);

    m_includeLegend = include;
}

ImageExporter::~ImageExporter()
{
    delete m_Legend;
}

void ImageExporter::setRasterOutputSize(const QSize *size)
{
    if (size)
        m_Size = new QSize(*size); // make a copy, so it's safe if the original gets deleted
    else
        m_Size = nullptr;
}

void ImageExporter::setLegendAlpha(int alpha)
{
    Q_ASSERT(alpha >= 0 && alpha <= 255);
    Q_ASSERT(m_Legend);
    QColor bgColor = m_Legend->getBgColor();
    bgColor.setAlpha(alpha);
    m_Legend->setBgColor(bgColor);
}
