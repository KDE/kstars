/*
    SPDX-FileCopyrightText: 2003 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skypoint.h"

#include <QImage>
#include <QList>
#include <QString>
#include <QDBusArgument>

class QPainter;

/**
 * @class FOV
 * A simple class encapsulating a Field-of-View symbol
 *
 * The FOV size, shape, name, and color can be customized. The rotation is by default 0 degrees East Of North.
 * @author Jason Harris
 * @author Jasem Mutlaq
 * @version 1.1
 */
class FOV : public QObject
{
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.fov")

  Q_PROPERTY(QString name MEMBER m_name)
  Q_PROPERTY(FOV::Shape shape MEMBER m_shape)
  Q_PROPERTY(float sizeX MEMBER m_sizeX)
  Q_PROPERTY(float sizeY MEMBER m_sizeY)
  Q_PROPERTY(float offsetX MEMBER m_offsetX)
  Q_PROPERTY(float offsetY MEMBER m_offsetY)
  Q_PROPERTY(float PA MEMBER m_PA)
  Q_PROPERTY(QString color MEMBER m_color)
  Q_PROPERTY(bool cpLock MEMBER m_lockCelestialPole)

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

    /** Default constructor */
    FOV();
    FOV(const QString &name, float a, float b = -1, float xoffset = 0, float yoffset = 0, float rot = 0,
        Shape shape = SQUARE, const QString &color = "#FFFFFF", bool useLockedCP = false);
    FOV(const FOV &other);

    void sync(const FOV &other);

    inline Q_SCRIPTABLE QString name() const { return m_name; }
    void setName(const QString &n) { m_name = n; }

    inline FOV::Shape shape() const { return m_shape; }
    void setShape(FOV::Shape s) { m_shape = s; }
    //void setShape(int s);

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

    // Position Angle
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

    bool lockCelestialPole() const;
    void setLockCelestialPole(bool lockCelestialPole);

private:
    QString m_name, m_color;
    FOV::Shape m_shape;
    float m_sizeX { 0 }, m_sizeY { 0 };
    float m_offsetX { 0 }, m_offsetY { 0 };
    float m_PA { 0 };
    float m_northPA { 0 };
    SkyPoint m_center;
    QImage m_image;
    bool m_imageDisplay { false };
    bool m_lockCelestialPole { false };

    static int getID() { return m_ID++; }
    static int m_ID;
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
    FOVManager() = default;
    ~FOVManager();

    /** @short Fill list with default FOVs*/
    static QList<FOV *> defaults();

    static QList<FOV *> m_FOVs;
};

// Shape
Q_DECLARE_METATYPE(FOV::Shape)
QDBusArgument &operator<<(QDBusArgument &argument, const FOV::Shape& source);
const QDBusArgument &operator>>(const QDBusArgument &argument, FOV::Shape &dest);
