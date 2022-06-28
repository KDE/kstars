/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mapcanvas.h"
#include <cstdlib>

#include <QPainter>
#include <QPixmap>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QStandardPaths>
#include "kspaths.h"

#include "dialogs/locationdialog.h"
#include "kstars.h"
#include "kstarsdata.h"

MapCanvas::MapCanvas(QWidget *parent) : QFrame(parent), ld(nullptr)
{
    setAutoFillBackground(false);

    QString bgFile = KSPaths::locate(QStandardPaths::AppLocalDataLocation, "geomap.jpg");
    bgImage        = new QPixmap(bgFile);
    xsize= width();
    ysize = height();
    ximage = bgImage->width();
    yimage = bgImage->height();
    ratio = ximage / yimage;
    xscale = xsize / 360;
    yscale = ysize / (360 / ratio);
    origin.setX(width() / 2);
    origin.setY(height() / 2);
}

MapCanvas::~MapCanvas()
{
    delete bgImage;
}

void MapCanvas::setGeometry(int x, int y, int w, int h)
{
    QWidget::setGeometry(x, y, w, h);
    origin.setX(w / 2);
    origin.setY(h / 2);
}

void MapCanvas::setGeometry(const QRect &r)
{
    QWidget::setGeometry(r);
    origin.setX(r.width() / 2);
    origin.setY(r.height() / 2);
}

void MapCanvas::mousePressEvent(QMouseEvent *e)
{
    //Determine Lat/Long corresponding to event press
    int lng = ((e->x() - origin.x()) / xscale);
    int lat = ((origin.y() - e->y()) / yscale);

    if (ld)
        ld->findCitiesNear(lng, lat);
}

void MapCanvas::paintEvent(QPaintEvent *)
{
    QPainter p;

    xsize= width();
    ysize = height();
    ximage = bgImage->width();
    yimage = bgImage->height();
    ratio = ximage / yimage;
    xscale = xsize / 360;
    yscale = ysize / (360 / ratio);
    origin.setX(width() / 2);
    origin.setY(height() / 2);

    //prepare the canvas
    p.begin(this);
    p.drawPixmap(0, 0, bgImage->scaled(size()));
    p.setPen(QPen(QColor("SlateGrey")));

    //Draw cities
    QPoint o;

    foreach (GeoLocation *g, KStarsData::Instance()->getGeoList())
    {
        convertAndScale(o, *g);

        if (o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height())
        {
            p.drawPoint(o.x(), o.y());
        }
    }

    // FIXME: there must be better way to this. Without bothering LocationDialog
    if (ld)
    {
        //redraw the cities that appear in the filtered list, with a white pen
        //If the list has not been filtered, skip the redraw.
        if (ld->filteredList().size())
        {
            p.setPen(Qt::white);
            foreach (GeoLocation *g, ld->filteredList())
            {
                convertAndScale(o, *g);

                if (o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height())
                {
                    p.drawPoint(o.x(), o.y());
                }
            }
        }

        GeoLocation *g = ld->selectedCity();
        if (g)
        {
            convertAndScale(o, *g);

            p.setPen(Qt::red);
            p.setBrush(Qt::red);
            p.drawEllipse(o.x() - 3, o.y() - 3, 6, 6);
            p.drawLine(o.x() - 16, o.y(), o.x() - 8, o.y());
            p.drawLine(o.x() + 8, o.y(), o.x() + 16, o.y());
            p.drawLine(o.x(), o.y() - 16, o.x(), o.y() - 8);
            p.drawLine(o.x(), o.y() + 8, o.x(), o.y() + 16);
            p.setPen(Qt::white);
            p.setBrush(Qt::white);
        }
    }
    p.end();
}

void MapCanvas::convertAndScale(QPoint &o, GeoLocation &g)
{
    int xpos = g.lng()->Degrees();
    int ypos = g.lat()->Degrees();
    o.setX((xpos * xscale) + origin.x());
    o.setY(height() - ((ypos * yscale) + origin.y()));
}
