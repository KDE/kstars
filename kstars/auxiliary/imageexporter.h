/*
    SPDX-FileCopyrightText: 2013 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef IMAGEEXPORTER_H
#define IMAGEEXPORTER_H

#include "../printing/legend.h"

#include <QObject>

class KStars;
class QSize;

/**
 * @class ImageExporter
 * @short Backends for exporting a sky image, either raster or vector, with a legend
 * @author Rafał Kułaga <rl.kulaga@gmail.com>
 * @author Akarsh Simha <akarsh.simha@kdemail.net>
 */
class ImageExporter : public QObject
{
    Q_OBJECT

  public:
    /**
         * @short Constructor
         */
    explicit ImageExporter(QObject *parent = nullptr);

    /**
         * @short Destructor
         */
    ~ImageExporter() override;

    /**
         * @return last error message
         */
    inline QString getLastErrorMessage() const { return m_lastErrorMessage; }

  public Q_SLOTS:
    /**
         * @short Exports an image with the defined settings.
         * @param url URL of the exported image
         * @return false if the URL was a network location and uploading to the network location failed
         * @note This method calls an SVG backend instead if the file extension is svg. Otherwise, it draws raster.
         */
    bool exportImage(QString url);

    /**
         * @short Set the legend properties
         * @param type Legend type. (See enum LEGEND_TYPE in legend.h)
         * @param orientation Legend orientation. (See LEGEND_ORIENTATION in legend.h)
         * @param position Legend position. (See LEGEND_POSITION in legend.h)
         * @param alpha Legend alpha (transparency). Default value is 160.
         * @param include Include the legend?
         */
    void setLegendProperties(Legend::LEGEND_TYPE type, Legend::LEGEND_ORIENTATION orientation,
                             Legend::LEGEND_POSITION position, int alpha = 160, bool include = true);

    /**
         * @short Include legend?
         * @param include The legend will be included if the flag is set to true
         */
    inline void includeLegend(bool include) { m_includeLegend = include; }

    /**
         * @short Set legend transparency
         * @param alpha Transparency level
         */
    void setLegendAlpha(int alpha);

    /**
         * @short Set the size of output raster images
         * @param size a pointer to a QSize containing the size of images. If a null pointer is supplied, the SkyMap size is used.
         * @note If size is larger than the skymap size, then the sky image is padded; if it is smaller, then it is cropped. No rescaling is done.
         */
    void setRasterOutputSize(const QSize *size);

    /**
         * @return a pointer to the legend used
         */
    inline Legend *getLegend() { return m_Legend; }

  private:
    void exportSvg(const QString &fileName);
    bool exportRasterGraphics(const QString &fileName);
    void addLegend(SkyQPainter *painter);
    void addLegend(QPaintDevice *pd);

    bool m_includeLegend;
    Legend *m_Legend;
    QSize *m_Size;
    QString m_lastErrorMessage;
};

#endif
