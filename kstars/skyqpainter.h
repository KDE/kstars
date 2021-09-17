/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skypainter.h"

#include <QColor>
#include <QMap>

class Projector;
class QWidget;
class QSize;
class QMessageBox;
class HIPSRenderer;
class TerrainRenderer;
class KSEarthShadow;

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

    ~SkyQPainter() override;

    void setPen(const QPen &pen) override;
    void setBrush(const QBrush &brush) override;

    /**
         * @param vectorStars Draw stars as vector graphics whenever possible.
         * @note Drawing stars as vectors is slower, but is better when saving .svg files. Set to true only when you are drawing on a canvas where speed doesn't matter. Definitely not when drawing on the SkyMap.
         */
    inline void setVectorStars(bool vectorStars) { m_vectorStars = vectorStars; }
    inline bool getVectorStars() const { return m_vectorStars; }

    void begin() override;
    void end() override;

    /** Recalculates the star pixmaps. */
    static void initStarImages();

    // Sky drawing functions
    void drawSkyBackground() override;
    void drawSkyLine(SkyPoint *a, SkyPoint *b) override;
    void drawSkyPolyline(LineList *list, SkipHashList *skipList = nullptr,
                         LineListLabel *label = nullptr) override;
    void drawSkyPolygon(LineList *list, bool forceClip = true) override;
    bool drawPointSource(const SkyPoint *loc, float mag, char sp = 'A') override;
    bool drawCatalogObject(const CatalogObject &obj) override;
    void drawCatalogObjectImage(const QPointF &pos, const CatalogObject &obj,
                                float positionAngle);
    bool drawPlanet(KSPlanetBase *planet) override;
    bool drawEarthShadow(KSEarthShadow *shadow) override;
    void drawObservingList(const QList<SkyObject *> &obs) override;
    void drawFlags() override;
    void drawHorizon(bool filled, SkyPoint *labelPoint = nullptr,
                     bool *drawLabel = nullptr) override;
    bool drawSatellite(Satellite *sat) override;
    virtual void drawDeepSkySymbol(const QPointF &pos, int type, float size, float e,
                                   float positionAngle);
    bool drawSupernova(Supernova *sup) override;
    bool drawComet(KSComet *com) override;
    /// This function exists so that we can draw other objects (e.g., planets) as point sources.
    virtual void drawPointSource(const QPointF &pos, float size, char sp = 'A');
    bool drawConstellationArtImage(ConstellationsArt *obj) override;
    bool drawHips() override;
    bool drawTerrain() override;

  private:
    QPaintDevice *m_pd{ nullptr };
    const Projector *m_proj{ nullptr };
    bool m_vectorStars{ false };
    HIPSRenderer *m_hipsRender{ nullptr };
    TerrainRenderer *m_terrainRender{ nullptr };
    QSize m_size;
    static int starColorMode;
    static QColor m_starColor;
    static QMap<char, QColor> ColorMap;
};
