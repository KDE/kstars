/***************************************************************************
                          fovwidget.cpp  -  description
                             -------------------
    begin                : 20 Aug 2009
    copyright            : (C) 2009 by Khudyakov Alexey
    email                : alexey.skladnoy@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef INFOBOXWIDGET_H_
#define INFOBOXWIDGET_H_

#include <QWidget>
#include <QPoint>
#include <QList>
#include <QString>
#include <QStringList>

class SkyPoint;
class SkyObject;
class InfoBoxWidget;

class InfoBoxes : public QWidget
{
    Q_OBJECT
public:

    InfoBoxes(QWidget* parent = 0) : QWidget(parent) {}
    virtual ~InfoBoxes();

    void addInfoBox(InfoBoxWidget* ibox);
    QList<InfoBoxWidget*> getInfoBoxes() const { return m_boxes; }
protected:
    virtual void resizeEvent(QResizeEvent * event);
private:
    QList<InfoBoxWidget*> m_boxes;
};

/** Small optianally transparent box for display of text messages. */
class InfoBoxWidget : public QWidget
{
    Q_OBJECT
public:
    /** Create one infobox. */
    InfoBoxWidget(bool shade, QPoint pos, QStringList str = QStringList(), QWidget* parent = 0);
    /** Destructor */
    virtual ~InfoBoxWidget();

    /** Check whether box is shaded. In this case only one line is shown. */
    bool shaded() const { return m_shaded; }
public slots:
    /** Set information about time. Data is taken from KStarsData. */
    void slotTimeChanged();
    /** Set information about location. Data is taken from KStarsData. */
    void slotGeoChanged();
    /** Set information about object. */
    void slotObjectChanged(SkyObject* obj);
    /** Set information about pointing. */
    void slotPointChanged(SkyPoint* p);
protected:
    virtual void resizeEvent(QResizeEvent * event);
    virtual void paintEvent(QPaintEvent* event);
    virtual void mouseDoubleClickEvent(QMouseEvent * event );
    virtual void mouseMoveEvent(QMouseEvent* event );
    virtual void mouseReleaseEvent(QMouseEvent* event);
    virtual void mousePressEvent(QMouseEvent* event);
private:
    /** Uset to set information about object. */
    void setPoint(QString name, SkyPoint* p);
    /** Recalculate size of widet */
    void updateSize();
    
    QStringList m_strings;  // list of string to show
    bool m_grabbed;         // True if widget is dragged around
    bool m_shaded;          // True if widget if shaded
};

#endif /* INFOBOXWIDGET_H_ */
