/***************************************************************************
                          fov.cpp  -  description
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

#include "fov.h"
#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "geolocation.h"
#include "skymap.h"
#include "projections/projector.h"

#include <algorithm>

#include <QPainter>
#include <QTextStream>
#include <QFile>
#include <QDebug>
#include <QStandardPaths>
#include "kspaths.h"


FOV::Shape FOV::intToShape(int s)
{ 
    return (s >= FOV::UNKNOWN || s < 0) ? FOV::UNKNOWN : static_cast<FOV::Shape>(s);
} 

FOV::FOV( const QString &n, float a, float b, float xoffset, float yoffset, float rot, Shape sh, const QString &col )
{ 
    m_name = n;
    m_sizeX = a;
    m_sizeY = (b < 0.0) ? a : b;

    m_offsetX     = xoffset;
    m_offsetY     = yoffset;
    m_rotation    = rot;
    m_shape       = sh;
    m_color       = col;
    m_northPA     = 0;    
    m_center.setRA(0);
    m_center.setDec(0);
    m_imageDisplay=false;
} 

FOV::FOV()
{
    m_name  = i18n( "No FOV" );
    m_color = "#FFFFFF";

    m_sizeX = m_sizeY = 0;
    m_shape = SQUARE;
    m_offsetX=m_offsetY=m_rotation=0,m_northPA=0;
    m_imageDisplay=false;
}

void FOV::draw( QPainter &p, float zoomFactor ) {
    p.setPen( QColor( color() ) );
    p.setBrush( Qt::NoBrush );
    
    p.setRenderHint( QPainter::Antialiasing, Options::useAntialias() );


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

    p.translate(offsetXPixelSize,  offsetYPixelSize);
    p.rotate(m_rotation+m_northPA);

    QPointF center(0,0);

    switch ( shape() )
    {
    case SQUARE: 
    {
        QRect targetRect(center.x() - pixelSizeX/2, center.y() - pixelSizeY/2, pixelSizeX, pixelSizeY);
        if (m_imageDisplay)
        {
            //QTransform imageT;
            //imageT.rotate(m_rotation+m_northPA);
            //p.drawImage(targetRect, m_image.transformed(imageT));
            p.drawImage(targetRect, m_image);
        }
        p.drawRect(targetRect);
        p.drawRect( center.x() , center.y() - (3 * pixelSizeY/5), pixelSizeX/40, pixelSizeX/10);
        p.drawLine( center.x() - pixelSizeX/30, center.y() - (3 * pixelSizeY/5), center.x() + pixelSizeX/20, center.y() - (3 * pixelSizeY/5));
        p.drawLine( center.x() - pixelSizeX/30, center.y() - (3 * pixelSizeY/5), center.x() + pixelSizeX/70, center.y() - (0.7 * pixelSizeY));
        p.drawLine( center.x() + pixelSizeX/20, center.y() - (3 * pixelSizeY/5), center.x() + pixelSizeX/70, center.y() - (0.7 * pixelSizeY));
    }
        break;
    case CIRCLE: 
        p.drawEllipse( center, pixelSizeX/2, pixelSizeY/2 );
        break;
    case CROSSHAIRS: 
        //Draw radial lines
        p.drawLine(center.x() + 0.5*pixelSizeX, center.y(),
                   center.x() + 1.5*pixelSizeX, center.y());
        p.drawLine(center.x() - 0.5*pixelSizeX, center.y(),
                   center.x() - 1.5*pixelSizeX, center.y());
        p.drawLine(center.x(), center.y() + 0.5*pixelSizeY,
                   center.x(), center.y() + 1.5*pixelSizeY);
        p.drawLine(center.x(), center.y() - 0.5*pixelSizeY,
                   center.x(), center.y() - 1.5*pixelSizeY);
        //Draw circles at 0.5 & 1 degrees
        p.drawEllipse( center, 0.5 * pixelSizeX, 0.5 * pixelSizeY);
        p.drawEllipse( center,       pixelSizeX,       pixelSizeY);
        break;
    case BULLSEYE: 
        p.drawEllipse(center, 0.5 * pixelSizeX, 0.5 * pixelSizeY);
        p.drawEllipse(center, 2.0 * pixelSizeX, 2.0 * pixelSizeY);
        p.drawEllipse(center, 4.0 * pixelSizeX, 4.0 * pixelSizeY);
        break;
    case SOLIDCIRCLE: {
        QColor colorAlpha = color();
        colorAlpha.setAlpha(127);
        p.setBrush( QBrush( colorAlpha ) );
        p.drawEllipse(center, pixelSizeX/2, pixelSizeY/2 );
        p.setBrush(Qt::NoBrush);
        break;
    }
    default: ; 
    }

    p.restore();
}

void FOV::draw(QPainter &p, float x, float y)
{
    float xfactor = x / sizeX() * 57.3 * 60.0;
    float yfactor = y / sizeY() * 57.3 * 60.0;
    float zoomFactor = std::min(xfactor, yfactor);
    switch( shape() ) {
    case CROSSHAIRS: zoomFactor /= 3; break;
    case BULLSEYE:   zoomFactor /= 8; break;
    default: ;
    }
    draw(p, zoomFactor);
}

void FOV::setShape( int s)
{
    m_shape = intToShape(s);
}


QList<FOV*> FOV::defaults()
{
    QList<FOV*> fovs;
    fovs << new FOV(i18nc("use field-of-view for binoculars", "7x35 Binoculars" ),
                    558,  558, 0,0,0, CIRCLE,"#AAAAAA")
         << new FOV(i18nc("use a Telrad field-of-view indicator", "Telrad" ),
                    30,   30, 0,0,0,   BULLSEYE,"#AA0000")
         << new FOV(i18nc("use 1-degree field-of-view indicator", "One Degree"),
                    60,   60, 0,0,0,  CIRCLE,"#AAAAAA")
         << new FOV(i18nc("use HST field-of-view indicator", "HST WFPC2"),
                    2.4,  2.4, 0,0,0, SQUARE,"#AAAAAA")
         << new FOV(i18nc("use Radiotelescope HPBW", "30m at 1.3cm" ),
                    1.79, 1.79, 0,0,0, SQUARE,"#AAAAAA");
    return fovs;
}

void FOV::writeFOVs(const QList<FOV*> fovs)
{
    QFile f;
    f.setFileName( KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QDir::separator() + "fov.dat" ) ;

    if ( ! f.open( QIODevice::WriteOnly ) ) {
        qDebug() << "Could not open fov.dat.";
        return;
    }
    QTextStream ostream(&f);
    foreach(FOV* fov, fovs) {
        ostream << fov->name()  << ':'
                << fov->sizeX() << ':'
                << fov->sizeY() << ':'
                << fov->offsetX() << ':'
                << fov->offsetY() << ':'
                << fov->rotation() << ':'
                << QString::number( fov->shape() ) << ':' //FIXME: is this needed???
                << fov->color() << endl;
    }
    f.close();
}

QList<FOV*> FOV::readFOVs()
{
    QFile f;
    QList<FOV*> fovs;
    f.setFileName( KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QDir::separator() + "fov.dat" ) ;

    if( !f.exists() ) {
        fovs = defaults();
        writeFOVs(fovs);
        return fovs;
    }

    if( f.open(QIODevice::ReadOnly) ) {
        fovs.clear();
        QTextStream istream(&f);
        while( !istream.atEnd() ) {
            QStringList fields = istream.readLine().split(':');
            bool ok;
            QString name, color;
            float   sizeX, sizeY, xoffset, yoffset, rot;
            Shape   shape;
            if( fields.count() == 8 )
            {
                name = fields[0];
                sizeX = fields[1].toFloat(&ok);
                if( !ok ) {
                    return QList<FOV*>();
                }
                sizeY = fields[2].toFloat(&ok);
                if( !ok ) {
                    return QList<FOV*>();
                }
                xoffset = fields[3].toFloat(&ok);
                if( !ok ) {
                    return QList<FOV*>();
                }

                yoffset = fields[4].toFloat(&ok);
                if( !ok ) {
                    return QList<FOV*>();
                }

                rot = fields[5].toFloat(&ok);
                if( !ok ) {
                    return QList<FOV*>();
                }

                shape = intToShape( fields[6].toInt(&ok) );
                if( !ok ) {
                    return QList<FOV*>();
                }
                color = fields[7];
            } else {
                continue;
            }
            fovs.append( new FOV(name, sizeX, sizeY, xoffset, yoffset, rot, shape, color) );
        }
    }
    return fovs;
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


