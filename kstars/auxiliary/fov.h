/***************************************************************************
                          fov.h  -  description
                             -------------------
    begin                : Fri 05 Sept 2003
    copyright            : (C) 2003 by Jason Harris
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

#pragma once

#include "skypoint.h"

#include <QImage>
#include <QList>
#include <QString>

class QPainter;

/**
 * @class FOV
 * A simple class encapsulating a Field-of-View symbol
 * @author Jason Harris
 * @version 1.0
 */
class FOV
{
  public:
    enum Shape
    {
        SQUARE,
        CIRCLE,
        CROSSHAIRS,
        BULLSEYE,
        SOLIDCIRCLE,
        UNKNOWN
    };
    static FOV::Shape intToShape(int);

    /** Default constructor */
    FOV();
    FOV(const QString &name, float a, float b = -1, float xoffset = 0, float yoffset = 0, float rot = 0,
        Shape shape = SQUARE, const QString &color = "#FFFFFF");

    inline QString name() const { return m_name; }
    void setName(const QString &n) { m_name = n; }

    inline Shape shape() const { return m_shape; }
    void setShape(Shape s) { m_shape = s; }
    void setShape(int s);

    inline float sizeX() const { return m_sizeX; }
    inline float sizeY() const { return m_sizeY; }
    void setSize(float s) { m_sizeX = m_sizeY = s; }
    void setSize(float sx, float sy)
    {
        m_sizeX = sx;
        m_sizeY = sy;
    }

    void setOffset(float fx, float fy)
    {
        m_offsetX = fx;
        m_offsetY = fy;
    }
    inline float offsetX() const { return m_offsetX; }
    inline float offsetY() const { return m_offsetY; }

    void setPA(float rt) { m_PA = rt; }
    inline float PA() const { return m_PA; }

    inline QString color() const { return m_color; }
    void setColor(const QString &c) { m_color = c; }

    /**
     * @short draw the FOV symbol on a QPainter
     * @param p reference to the target QPainter. The painter should already be started.
     * @param zoomFactor is zoom factor as in SkyMap.
     */
    void draw(QPainter &p, float zoomFactor);

    /**
     * @short draw FOV symbol so it will be inside a rectangle
     * @param p reference to the target QPainter. The painter should already be started.
     * @param x is X size of rectangle
     * @param y is Y size of rectangle
     */
    void draw(QPainter &p, float x, float y);

    SkyPoint center() const;
    void setCenter(const SkyPoint &center);

    float northPA() const;
    void setNorthPA(float northPA);

    void setImage(const QImage &image);

    void setImageDisplay(bool value);

  private:
    QString m_name, m_color;
    Shape m_shape;
    float m_sizeX { 0 }, m_sizeY { 0 };
    float m_offsetX { 0 }, m_offsetY { 0 };
    float m_PA { 0 };
    float m_northPA { 0 };
    SkyPoint m_center;
    QImage m_image;
    bool m_imageDisplay { false };
};

/**
 * @class FOVManager
 * A simple class handling FOVs.
 * @note Should migrate this from file (fov.dat) to using the user sqlite database
 * @author Jasem Mutlaq
 * @version 1.0
 */
class FOVManager
{
  public:
    /** @short Read list of FOVs from "fov.dat" */
    static const QList<FOV *> &readFOVs();
    /** @short Release the FOV cache */
    static void releaseCache();
    static void addFOV(FOV *newFOV)
    {
        Q_ASSERT(newFOV);
        m_FOVs.append(newFOV);
    }
    static void removeFOV(FOV *fov)
    {
        Q_ASSERT(fov);
        m_FOVs.removeOne(fov);
    }
    static const QList<FOV *> &getFOVs() { return m_FOVs; }

    /** @short Write list of FOVs to "fov.dat" */
    static bool save();

  private:
    FOVManager();
    ~FOVManager();

    /** @short Fill list with default FOVs*/
    static QList<FOV *> defaults();

    static QList<FOV *> m_FOVs;
};
