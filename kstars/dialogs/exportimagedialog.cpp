/***************************************************************************
                          exportimagedialog.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Jun 13 2011
    copyright            : (C) 2011 by Rafał Kułaga
    email                : rl.kulaga@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "exportimagedialog.h"
#include "kstars/kstars.h"
#include "kstars/skymap.h"
#include "kstars/legend.h"
#include "kstars/skyqpainter.h"

#include <kio/netaccess.h>
#include <kmessagebox.h>
#include <ktemporaryfile.h>
#include <kurl.h>

#include <QSvgGenerator>
#include <QDir>
#include <QDesktopWidget>


ExportImageDialogUI::ExportImageDialogUI(QWidget *parent)
    : QFrame(parent)
{
    setupUi(this);
}

ExportImageDialog::ExportImageDialog(const QString &url, const QSize &size)
    : KDialog((QWidget*) KStars::Instance()), m_KStars(KStars::Instance()), m_Url(url), m_Size(size)
{
    m_DialogUI = new ExportImageDialogUI(this);
    setMainWidget(m_DialogUI);

    setWindowTitle(i18n("Export sky image"));

    setupWidgets();
    setupConnections();
}

ExportImageDialog::~ExportImageDialog()
{}

void ExportImageDialog::exportImage()
{
    //If the filename string contains no "/" separators, assume the
    //user wanted to place a file in their home directory.
    KUrl fileURL;
    if(!m_Url.contains("/"))
    {
        fileURL = QDir::homePath() + '/' + m_Url;
    }

    else
    {
        fileURL = m_Url;
    }

    KTemporaryFile tmpfile;
    tmpfile.open();
    QString fname;

    if(fileURL.isValid())
    {
        if(fileURL.isLocalFile())
        {
            fname = fileURL.toLocalFile();
        }

        else
        {
            fname = tmpfile.fileName();
        }

        SkyMap *map = m_KStars->map();

        //Determine desired image format from filename extension
        QString ext = fname.mid(fname.lastIndexOf(".") + 1);
        if(ext.toLower() == "svg")
        {
            // export as SVG
            QSvgGenerator svgGenerator;
            svgGenerator.setFileName(fname);
            svgGenerator.setTitle(i18n("KStars Exported Sky Image"));
            svgGenerator.setDescription(i18n("KStars Exported Sky Image"));
            svgGenerator.setSize(QSize(map->width(), map->height()));
            svgGenerator.setResolution(qMax(map->logicalDpiX(), map->logicalDpiY()));
            svgGenerator.setViewBox(QRect(0, 0, map->width(), map->height()));

            SkyQPainter painter(m_KStars, &svgGenerator);
            painter.begin();

            map->exportSkyImage(&painter);

            if(m_DialogUI->addLegendCheckBox->isChecked())
            {
                addLegend(&painter);
            }

            painter.end();
            qApp->processEvents();
        }

        else
        {
            // export as raster graphics
            const char* format = "PNG";

            if(ext.toLower() == "png") {format = "PNG";}
            else if(ext.toLower() == "jpg" || ext.toLower() == "jpeg" ) {format = "JPG";}
            else if(ext.toLower() == "gif") {format = "GIF";}
            else if(ext.toLower() == "pnm") {format = "PNM";}
            else if(ext.toLower() == "bmp") {format = "BMP";}
            else
            {
                kWarning() << i18n("Could not parse image format of %1; assuming PNG.", fname);
            }

            int width = m_Size.width();
            int height = m_Size.height();

            QPixmap skyimage(map->width(), map->height());
            QPixmap outimage(width, height);
            outimage.fill();

            map->exportSkyImage(&skyimage);
            qApp->processEvents();

            //skyImage is the size of the sky map.  The requested image size is w x h.
            //If w x h is smaller than the skymap, then we simply crop the image.
            //If w x h is larger than the skymap, pad the skymap image with a white border.
            if(width == map->width() && height == map->height())
            {
                outimage = skyimage.copy();
            }

            else
            {
                int dx(0), dy(0), sx(0), sy(0);
                int sw(map->width()), sh(map->height());

                if(width > map->width())
                {
                    dx = (width - map->width())/2;
                }

                else
                {
                    sx = (map->width() - width)/2;
                    sw = width;
                }

                if(height > map->height())
                {
                    dy = (height - map->height())/2;
                }

                else
                {
                    sy = (map->height() - height)/2;
                    sh = height;
                }

                QPainter p;
                p.begin(&outimage);
                p.fillRect(outimage.rect(), QBrush( Qt::white));
                p.drawImage(dx, dy, skyimage.toImage(), sx, sy, sw, sh);
                p.end();
            }

            if(m_DialogUI->addLegendCheckBox->isChecked())
            {
                addLegend(&outimage);
            }

            if(!outimage.save(fname, format))
            {
                kDebug() << i18n("Error: Unable to save image: %1 ", fname);
            }

            else
            {
                kDebug() << i18n("Image saved to file: %1", fname);
            }
        }

        if(tmpfile.fileName() == fname)
        {
            //attempt to upload image to remote location
            if(!KIO::NetAccess::upload(tmpfile.fileName(), fileURL, this))
            {
                QString message = i18n( "Could not upload image to remote location: %1", fileURL.prettyUrl() );
                KMessageBox::sorry( 0, message, i18n( "Could not upload file" ) );
            }
        }
    }
}

void ExportImageDialog::switchLegendConfig(bool newState)
{
    m_DialogUI->legendOrientationLabel->setEnabled(newState);
    m_DialogUI->legendOrientationComboBox->setEnabled(newState);
    m_DialogUI->legendTypeLabel->setEnabled(newState);
    m_DialogUI->legendTypeComboBox->setEnabled(newState);
    m_DialogUI->legendPositionLabel->setEnabled(newState);
    m_DialogUI->legendPositionComboBox->setEnabled(newState);
}

void ExportImageDialog::setupWidgets()
{
    m_DialogUI->addLegendCheckBox->setChecked(true);

    m_DialogUI->legendOrientationComboBox->addItem(i18n("Horizontal"));
    m_DialogUI->legendOrientationComboBox->addItem(i18n("Vertical"));

    m_DialogUI->legendTypeComboBox->addItem(i18n("Full legend"));
    m_DialogUI->legendTypeComboBox->addItem(i18n("Scale-only"));

    QStringList positions;
    positions << i18n("Upper left corner") << i18n("Upper right corner") << i18n("Lower left corner")
            << i18n("Lower right corner");
    m_DialogUI->legendPositionComboBox->addItems(positions);
}

void ExportImageDialog::setupConnections()
{
    connect(this, SIGNAL(okClicked()), this, SLOT(exportImage()));
    connect(this, SIGNAL(cancelClicked()), this, SLOT(close()));

    connect(m_DialogUI->addLegendCheckBox, SIGNAL(toggled(bool)), this, SLOT(switchLegendConfig(bool)));
}

void ExportImageDialog::addLegend(SkyQPainter *painter)
{
    Legend legend(m_KStars);

    // set font for legend labels
    legend.setFont(QFont("Courier New", 8));

    if(m_DialogUI->legendOrientationComboBox->currentIndex() == 1) // orientation: vertical
    {
        legend.setOrientation(Legend::LO_VERTICAL);
    }

    bool scaleOnly = false;
    if(m_DialogUI->legendTypeComboBox->currentIndex() == 1) // scale only
    {
        scaleOnly = true;
    }

    QSize size = legend.calculateSize(scaleOnly);

    switch(m_DialogUI->legendPositionComboBox->currentIndex())
    {
    case 0: // position: upper left corner
        {
            legend.paintLegend(painter, QPoint(0, 0), scaleOnly);

            break;
        }

    case 1: // position: upper right corner
        {
            QPoint pos(painter->device()->width() - size.width(), 0);
            legend.paintLegend(painter, pos, scaleOnly);

            break;
        }

    case 2: // position: lower left corner
        {
            QPoint pos(0, painter->device()->height() - size.height());
            legend.paintLegend(painter, pos, scaleOnly);

            break;
        }

    case 3: // position: lower right corner
        {
            QPoint pos(painter->device()->width() - size.width(), painter->device()->height() - size.height());
            legend.paintLegend(painter, pos, scaleOnly);

            break;
        }

    default: // should never happen
        {
            break;
        }
    }
}

void ExportImageDialog::addLegend(QPaintDevice *pd)
{
    SkyQPainter painter(m_KStars, pd);
    painter.begin();

    addLegend(&painter);

    painter.end();
}
