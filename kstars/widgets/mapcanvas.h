/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef MAPCANVAS_H
#define MAPCANVAS_H

#include <QFrame>
#include <QPixmap>
#include <QMouseEvent>
#include <QPaintEvent>

/** @class MapCanvas
	*Used in LocationDialog for displaying a map of the Earth.
	*In addition, cities in the database are drawn as grey or white dots.
	*Also, the widget processes mouse clicks, to trim the list of
	*cities to those near the mouse click.
	*@short Widget used in the LocationDialog for displaying the world map.
	*@author Jason Harris
	*@version 1.0
	*/

class LocationDialog;
class QPixmap;

class MapCanvas : public QFrame
{
    Q_OBJECT
  public:
    /**Default constructor.  Initialize the widget: create pixmaps, load the
         * world map image
         * @param parent pointer to the parent LocationDialog
         */
    explicit MapCanvas(QWidget *parent);

    /**Destructor (empty) */
    ~MapCanvas() override;

    /** Set location dialog */
    // FIXME: This is temporary plug
    void setLocationDialog(LocationDialog *loc) { ld = loc; }
  public slots:
    /**Set the geometry of the map widget (overloaded from QWidget).
         * Resizes the size of the map pixmap to match the widget, and resets
         * the Origin QPoint so it remains at the center of the widget.
         * @note this is called automatically by resize events.
         * @p x the x-position of the widget
         * @p y the y-position of the widget
         * @p w the width of the widget
         * @p h the height of the widget
         */
    virtual void setGeometry(int x, int y, int w, int h);

    /**Set the geometry of the map widget (overloaded from QWidget).
         * Resizes the size of the map pixmap to match the widget, and resets
         * the Origin QPoint so it remains at the center of the widget.
         * This function behaves just like the above function.  It differs
         * only in the data type of its argument.
         * @note this is called automatically by resize events.
         * @p r QRect describing geometry
         */
    virtual void setGeometry(const QRect &r);

  protected:
    /**Draw the map.  Draw grey dots on the locations of all cities,
         * and highlight the cities which match the current filters as
         * white dits.  Also draw a red crosshairs on the
         * currently-selected city.
         * @see LocationDialog
         */
    void paintEvent(QPaintEvent *e) override;

    /**Trim the list of cities so that only those within 2 degrees
         * of the mouse click are shown in the list.
         * @see LocationDialog
         */
    void mousePressEvent(QMouseEvent *e) override;

  private:
    LocationDialog *ld;
    QPixmap *bgImage;
    QString BGColor;
    QPoint origin;
};

#endif
