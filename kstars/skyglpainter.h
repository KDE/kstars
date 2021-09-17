/*
    SPDX-FileCopyrightText: 2010 Henry de Valence <hdevalence@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SKYGLPAINTER_H
#define SKYGLPAINTER_H

#include <cstddef>
#include <Eigen/Core>

#include "skypainter.h"
#include "skyobjects/skyobject.h"
#include "projections/projector.h"

class QGLWidget;

class SkyGLPainter : public SkyPainter
{
    public:
        explicit SkyGLPainter(QGLWidget *widget);
        bool drawPlanet(KSPlanetBase *planet) override;
        bool drawPointSource(const SkyPoint *loc, float mag, char sp = 'A') override;
        void drawSkyPolygon(LineList *list, bool forceClip = true) override;
        void drawSkyPolyline(LineList *list, SkipHashList *skipList = nullptr,
                             LineListLabel *label = nullptr) override;
        void drawSkyLine(SkyPoint *a, SkyPoint *b) override;
        void drawSkyBackground() override;
        void drawObservingList(const QList<SkyObject *> &obs) override;
        void drawFlags() override;
        void end() override;
        void begin() override;
        void setBrush(const QBrush &brush) override;
        void setPen(const QPen &pen) override;
        void drawHorizon(bool filled, SkyPoint *labelPoint = nullptr,
                         bool *drawLabel = nullptr) override;
        bool drawSatellite(Satellite *sat) override;
        bool drawSupernova(Supernova *sup) override;
        void drawText(int x, int y, const QString text, QFont font, QColor color);
        bool drawConstellationArtImage(ConstellationsArt *obj) override;
        bool drawHips() override;

    private:
        bool addItem(SkyPoint *p, int type, float width, char sp = 'a');
        void drawBuffer(int type);
        void drawPolygon(const QVector<Eigen::Vector2f> &poly, bool convex = true,
                         bool flush_buffers = true);

        /** Render textured rectangle on screeen. Parameters are texture
             *  to be used, position, orientation and size of rectangle*/
        void drawTexturedRectangle(const QImage &img, const Eigen::Vector2f &pos, const float angle,
                                   const float sizeX, const float sizeY);

        const Projector *m_proj;

        Eigen::Vector4f m_pen;
        static const int BUFSIZE = 512;
        ///FIXME: what kind of TYPE_UNKNOWN objects are there?
        static const int NUMTYPES = (int)SkyObject::TYPE_UNKNOWN + 1;
        static Eigen::Vector2f m_vertex[NUMTYPES][6 * BUFSIZE];
        static Eigen::Vector2f m_texcoord[NUMTYPES][6 * BUFSIZE];
        static Eigen::Vector3f m_color[NUMTYPES][6 * BUFSIZE];
        static int m_idx[NUMTYPES];
        static bool m_init;  ///< keep track of whether we have filled the texcoord array
        QGLWidget *m_widget; // Pointer to (GL) widget on which we are painting
};

#endif // SKYGLPAINTER_H
