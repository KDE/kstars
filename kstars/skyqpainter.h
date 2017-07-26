/*
    (C) 2010 Henry de Valence <hdevalence@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/

#pragma once

#include "skypainter.h"

#include <QColor>
#include <QMap>

class Projector;
class QWidget;
class QSize;
class QMessageBox;

/**
 * @short The QPainter-based painting backend.
 * This class implements the SkyPainter interface using a QPainter.
 * For documentation, @see SkyPainter.
 */
class SkyQPainter : public SkyPainter, public QPainter
{
  public:
    /** Release the image cache */
    static void releaseImageCache();

    /**
     * @short Creates a SkyQPainter with the given QPaintDevice and uses the dimensions of the paint device as canvas dimensions
     * @param pd the painting device. Cannot be 0
     * @param canvasSize the size of the canvas
     */
    SkyQPainter(QPaintDevice *pd, const QSize &canvasSize);

    /**
     * @short Creates a SkyQPainter with the given QPaintDevice and given canvas size
     * @param pd the painting device. Cannot be 0
     */
    explicit SkyQPainter(QPaintDevice *pd);

    /**
     * @short Creates a SkyQPainter given a QWidget and an optional QPaintDevice.
     * @param widget the QWidget that provides the canvas size, and also the paint device unless @p pd is specified
     * @param pd the painting device. If 0, then @p widget will be used.
     */
    explicit SkyQPainter(QWidget *widget, QPaintDevice *pd = nullptr);

    virtual ~SkyQPainter();

    void setPen(const QPen &pen) Q_DECL_OVERRIDE;
    void setBrush(const QBrush &brush) Q_DECL_OVERRIDE;

    /**
     * @param vectorStars Draw stars as vector graphics whenever possible.
     * @note Drawing stars as vectors is slower, but is better when saving .svg files. Set to true only when you are drawing on a canvas where speed doesn't matter. Definitely not when drawing on the SkyMap.
     */
    inline void setVectorStars(bool vectorStars) { m_vectorStars = vectorStars; }
    inline bool getVectorStars() const { return m_vectorStars; }

    void begin() Q_DECL_OVERRIDE;
    void end() Q_DECL_OVERRIDE;

    /** Recalculates the star pixmaps. */
    static void initStarImages();

    // Sky drawing functions
    void drawSkyBackground() Q_DECL_OVERRIDE;
    void drawSkyLine(SkyPoint *a, SkyPoint *b) Q_DECL_OVERRIDE;
    void drawSkyPolyline(LineList *list, SkipHashList *skipList = nullptr,
                         LineListLabel *label = nullptr) Q_DECL_OVERRIDE;
    void drawSkyPolygon(LineList *list, bool forceClip = true) Q_DECL_OVERRIDE;
    bool drawPointSource(SkyPoint *loc, float mag, char sp = 'A') Q_DECL_OVERRIDE;
    bool drawDeepSkyObject(DeepSkyObject *obj, bool drawImage = false) Q_DECL_OVERRIDE;
    bool drawPlanet(KSPlanetBase *planet) Q_DECL_OVERRIDE;
    void drawObservingList(const QList<SkyObject *> &obs) Q_DECL_OVERRIDE;
    void drawFlags() Q_DECL_OVERRIDE;
    void drawHorizon(bool filled, SkyPoint *labelPoint = nullptr, bool *drawLabel = nullptr) Q_DECL_OVERRIDE;
    bool drawSatellite(Satellite *sat) Q_DECL_OVERRIDE;
    virtual void drawDeepSkySymbol(const QPointF &pos, int type, float size, float e, float positionAngle);
    bool drawSupernova(Supernova *sup) Q_DECL_OVERRIDE;
    bool drawComet(KSComet *com) Q_DECL_OVERRIDE;
    /// This function exists so that we can draw other objects (e.g., planets) as point sources.
    virtual void drawPointSource(const QPointF &pos, float size, char sp = 'A');
    bool drawConstellationArtImage(ConstellationsArt *obj) Q_DECL_OVERRIDE;

  private:
    virtual bool drawDeepSkyImage(const QPointF &pos, DeepSkyObject *obj, float positionAngle);

    QPaintDevice *m_pd { nullptr };
    const Projector *m_proj { nullptr };
    bool m_vectorStars { false };
    QSize m_size;
    static int starColorMode;
    static QColor m_starColor;
    static QMap<char, QColor> ColorMap;
};
