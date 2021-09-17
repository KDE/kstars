/*
    SPDX-FileCopyrightText: 2003 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fov.h"

#include "geolocation.h"
#include "kspaths.h"
#ifndef KSTARS_LITE
#include "kstars.h"
#endif
#include "kstarsdata.h"
#include "Options.h"
#include "skymap.h"
#include "projections/projector.h"
#include "fovadaptor.h"

#include <QPainter>
#include <QTextStream>
#include <QFile>
#include <QDebug>
#include <QStandardPaths>

#include <algorithm>

QList<FOV *> FOVManager::m_FOVs;
int FOV::m_ID = 1;

FOVManager::~FOVManager()
{
    qDeleteAll(m_FOVs);
}

QList<FOV *> FOVManager::defaults()
{
    QList<FOV *> fovs;
    fovs << new FOV(i18nc("use field-of-view for binoculars", "7x35 Binoculars"), 558, 558, 0, 0, 0, FOV::CIRCLE,
                    "#AAAAAA")
         << new FOV(i18nc("use a Telrad field-of-view indicator", "Telrad"), 30, 30, 0, 0, 0, FOV::BULLSEYE, "#AA0000")
         << new FOV(i18nc("use 1-degree field-of-view indicator", "One Degree"), 60, 60, 0, 0, 0, FOV::CIRCLE,
                    "#AAAAAA")
         << new FOV(i18nc("use HST field-of-view indicator", "HST WFPC2"), 2.4, 2.4, 0, 0, 0, FOV::SQUARE, "#AAAAAA")
         << new FOV(i18nc("use Radiotelescope HPBW", "30m at 1.3cm"), 1.79, 1.79, 0, 0, 0, FOV::SQUARE, "#AAAAAA");
    return fovs;
}

bool FOVManager::save()
{
    QFile f;

    // TODO: Move FOVs to user database instead of file!!
    f.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("fov.dat"));

    if (!f.open(QIODevice::WriteOnly))
    {
        qDebug() << "Could not open fov.dat.";
        return false;
    }

    QTextStream ostream(&f);
    foreach (FOV *fov, m_FOVs)
    {
        ostream << fov->name() << ':' << fov->sizeX() << ':' << fov->sizeY() << ':' << fov->offsetX() << ':'
                << fov->offsetY() << ':' << fov->PA() << ':' << QString::number(fov->shape())
                << ':' << fov->color()
                << ':' << (fov->lockCelestialPole() ? 1 : 0)
                << '\n';
    }
    f.close();

    return true;
}

const QList<FOV *> &FOVManager::readFOVs()
{
    qDeleteAll(m_FOVs);
    m_FOVs.clear();

    QFile f;
    f.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("fov.dat"));

    if (!f.exists())
    {
        m_FOVs = defaults();
        save();
        return m_FOVs;
    }

    if (f.open(QIODevice::ReadOnly))
    {
        QTextStream istream(&f);
        while (!istream.atEnd())
        {
            QStringList fields = istream.readLine().split(':');
            bool ok;
            QString name, color;
            float sizeX, sizeY, xoffset, yoffset, rot;
            bool lockedCP = false;
            FOV::Shape shape;
            if (fields.count() >= 8)
            {
                name  = fields[0];
                sizeX = fields[1].toFloat(&ok);
                if (!ok)
                {
                    return m_FOVs;
                }
                sizeY = fields[2].toFloat(&ok);
                if (!ok)
                {
                    return m_FOVs;
                }
                xoffset = fields[3].toFloat(&ok);
                if (!ok)
                {
                    return m_FOVs;
                }

                yoffset = fields[4].toFloat(&ok);
                if (!ok)
                {
                    return m_FOVs;
                }

                rot = fields[5].toFloat(&ok);
                if (!ok)
                {
                    return m_FOVs;
                }

                shape = static_cast<FOV::Shape>(fields[6].toInt(&ok));
                if (!ok)
                {
                    return m_FOVs;
                }
                color = fields[7];

                if (fields.count() == 9)
                    lockedCP = (fields[8].toInt(&ok) == 1);
            }
            else
            {
                continue;
            }

            m_FOVs.append(new FOV(name, sizeX, sizeY, xoffset, yoffset, rot, shape, color, lockedCP));
        }
    }
    return m_FOVs;
}

void FOVManager::releaseCache()
{
    qDeleteAll(m_FOVs);
    m_FOVs.clear();
}

FOV::FOV(const QString &n, float a, float b, float xoffset, float yoffset, float rot, Shape sh, const QString &col,
         bool useLockedCP) : QObject()
{
    qRegisterMetaType<FOV::Shape>("FOV::Shape");
    qDBusRegisterMetaType<FOV::Shape>();

    new FovAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QString("/KStars/FOV/%1").arg(getID()), this);

    m_name  = n;
    m_sizeX = a;
    m_sizeY = (b < 0.0) ? a : b;

    m_offsetX  = xoffset;
    m_offsetY  = yoffset;
    m_PA = rot;
    m_shape    = sh;
    m_color    = col;
    m_northPA  = 0;
    m_center.setRA(0);
    m_center.setDec(0);
    m_imageDisplay = false;
    m_lockCelestialPole = useLockedCP;
}

FOV::FOV() : QObject()
{
    qRegisterMetaType<FOV::Shape>("FOV::Shape");
    qDBusRegisterMetaType<FOV::Shape>();

    new FovAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QString("/KStars/FOV/%1").arg(getID()), this);

    m_name  = i18n("No FOV");
    m_color = "#FFFFFF";

    m_sizeX = m_sizeY = 0;
    m_shape           = SQUARE;
    m_imageDisplay = false;
    m_lockCelestialPole = false;
}

FOV::FOV(const FOV &other) : QObject()
{
    m_name   = other.m_name;
    m_color  = other.m_color;
    m_sizeX  = other.m_sizeX;
    m_sizeY  = other.m_sizeY;
    m_shape  = other.m_shape;
    m_offsetX = other.m_offsetX;
    m_offsetY = other.m_offsetY;
    m_PA     = other.m_PA;
    m_imageDisplay = other.m_imageDisplay;
    m_lockCelestialPole = other.m_lockCelestialPole;
}

void FOV::sync(const FOV &other)
{
    m_name   = other.m_name;
    m_color  = other.m_color;
    m_sizeX  = other.m_sizeX;
    m_sizeY  = other.m_sizeY;
    m_shape  = other.m_shape;
    m_offsetX = other.m_offsetX;
    m_offsetY = other.m_offsetY;
    m_PA     = other.m_PA;
    m_imageDisplay = other.m_imageDisplay;
    m_lockCelestialPole = other.m_lockCelestialPole;
}

void FOV::draw(QPainter &p, float zoomFactor)
{
    // Do not draw empty FOVs
    if (m_sizeX == 0 || m_sizeY == 0)
        return;

    p.setPen(QColor(color()));
    p.setBrush(Qt::NoBrush);

    p.setRenderHint(QPainter::Antialiasing, Options::useAntialias());

    float pixelSizeX = sizeX() * zoomFactor / 57.3 / 60.0;
    float pixelSizeY = sizeY() * zoomFactor / 57.3 / 60.0;

    float offsetXPixelSize = offsetX() * zoomFactor / 57.3 / 60.0;
    float offsetYPixelSize = offsetY() * zoomFactor / 57.3 / 60.0;

    p.save();

    if (m_center.ra().Degrees() > 0)
    {
        m_center.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
        QPointF skypoint_center = KStars::Instance()->map()->projector()->toScreen(&m_center);
        p.translate(skypoint_center.toPoint());
    }
    else
        p.translate(p.viewport().center());

    p.translate(offsetXPixelSize, offsetYPixelSize);
    p.rotate( (m_PA - m_northPA) * -1);

    QPointF center(0, 0);

    switch (shape())
    {
        case SQUARE:
        {
            QRect targetRect(center.x() - pixelSizeX / 2, center.y() - pixelSizeY / 2, pixelSizeX, pixelSizeY);
            if (m_imageDisplay)
                p.drawImage(targetRect, m_image);

            p.drawRect(targetRect);
            p.drawRect(center.x(), center.y() - (3 * pixelSizeY / 5), pixelSizeX / 40, pixelSizeX / 10);
            p.drawLine(center.x() - pixelSizeX / 30, center.y() - (3 * pixelSizeY / 5), center.x() + pixelSizeX / 20,
                       center.y() - (3 * pixelSizeY / 5));
            p.drawLine(center.x() - pixelSizeX / 30, center.y() - (3 * pixelSizeY / 5), center.x() + pixelSizeX / 70,
                       center.y() - (0.7 * pixelSizeY));
            p.drawLine(center.x() + pixelSizeX / 20, center.y() - (3 * pixelSizeY / 5), center.x() + pixelSizeX / 70,
                       center.y() - (0.7 * pixelSizeY));

            if (name().count() > 0)
            {
                int fontSize = pixelSizeX / 15;
                fontSize *= 14.0 / name().count();
                if (fontSize <= 4)
                    break;

                QFont font = p.font();
                font.setPixelSize(fontSize);
                p.setFont(font);

                QRect nameRect(targetRect.topLeft().x(), targetRect.topLeft().y() - (pixelSizeY / 8), targetRect.width() / 2,
                               pixelSizeX / 10);
                p.drawText(nameRect, Qt::AlignCenter, name());

                QRect sizeRect(targetRect.center().x(), targetRect.topLeft().y() - (pixelSizeY / 8), targetRect.width() / 2,
                               pixelSizeX / 10);
                p.drawText(sizeRect, Qt::AlignCenter, QString("%1'x%2'").arg(QString::number(m_sizeX, 'f', 1), QString::number(m_sizeY, 'f',
                           1)));
            }
        }
        break;
        case CIRCLE:
            p.drawEllipse(center, pixelSizeX / 2, pixelSizeY / 2);
            break;
        case CROSSHAIRS:
            //Draw radial lines
            p.drawLine(center.x() + 0.5 * pixelSizeX, center.y(), center.x() + 1.5 * pixelSizeX, center.y());
            p.drawLine(center.x() - 0.5 * pixelSizeX, center.y(), center.x() - 1.5 * pixelSizeX, center.y());
            p.drawLine(center.x(), center.y() + 0.5 * pixelSizeY, center.x(), center.y() + 1.5 * pixelSizeY);
            p.drawLine(center.x(), center.y() - 0.5 * pixelSizeY, center.x(), center.y() - 1.5 * pixelSizeY);
            //Draw circles at 0.5 & 1 degrees
            p.drawEllipse(center, 0.5 * pixelSizeX, 0.5 * pixelSizeY);
            p.drawEllipse(center, pixelSizeX, pixelSizeY);
            break;
        case BULLSEYE:
            p.drawEllipse(center, 0.5 * pixelSizeX, 0.5 * pixelSizeY);
            p.drawEllipse(center, 2.0 * pixelSizeX, 2.0 * pixelSizeY);
            p.drawEllipse(center, 4.0 * pixelSizeX, 4.0 * pixelSizeY);
            break;
        case SOLIDCIRCLE:
        {
            QColor colorAlpha = color();
            colorAlpha.setAlpha(127);
            p.setBrush(QBrush(colorAlpha));
            p.drawEllipse(center, pixelSizeX / 2, pixelSizeY / 2);
            p.setBrush(Qt::NoBrush);
            break;
        }
        default:
            ;
    }

    p.restore();
}

void FOV::draw(QPainter &p, float x, float y)
{
    float xfactor    = x / sizeX() * 57.3 * 60.0;
    float yfactor    = y / sizeY() * 57.3 * 60.0;
    float zoomFactor = std::min(xfactor, yfactor);
    switch (shape())
    {
        case CROSSHAIRS:
            zoomFactor /= 3;
            break;
        case BULLSEYE:
            zoomFactor /= 8;
            break;
        default:
            ;
    }
    draw(p, zoomFactor);
}

SkyPoint FOV::center() const
{
    return m_center;
}

void FOV::setCenter(const SkyPoint &center)
{
    m_center = center;
}

float FOV::northPA() const
{
    return m_northPA;
}

void FOV::setNorthPA(float northPA)
{
    m_northPA = northPA;
}

void FOV::setImage(const QImage &image)
{
    m_image = image;
}

void FOV::setImageDisplay(bool value)
{
    m_imageDisplay = value;
}

bool FOV::lockCelestialPole() const
{
    return m_lockCelestialPole;
}

void FOV::setLockCelestialPole(bool lockCelestialPole)
{
    m_lockCelestialPole = lockCelestialPole;
}

QDBusArgument &operator<<(QDBusArgument &argument, const FOV::Shape &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, FOV::Shape &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<FOV::Shape>(a);
    return argument;
}
