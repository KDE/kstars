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
#include <QString>
#include <QStringList>

class SkyPoint;
class SkyObject;

class InfoBoxWidget : public QWidget
{
    Q_OBJECT
public:
    InfoBoxWidget(bool shade, int x, int y, QStringList str = QStringList(), QWidget* parent = 0);
    virtual ~InfoBoxWidget();
public slots:
    void slotTimeChanged();
    void slotGeoChanged();
    void slotObjectChanged(SkyObject* obj);
    void slotPointChanged(SkyPoint* p);
protected:
    virtual void resizeEvent(QResizeEvent * event);
    virtual void paintEvent(QPaintEvent* event);
    virtual void mouseDoubleClickEvent(QMouseEvent * event );
    virtual void mouseMoveEvent(QMouseEvent* event );
    virtual void mouseReleaseEvent(QMouseEvent* event);
    virtual void mousePressEvent(QMouseEvent* event);
private:
    void setPoint(QString name, SkyPoint* p);
    void updateSize();
    QStringList m_strings;
    bool m_grabbed;
    bool m_shaded;
};

#endif /* INFOBOXWIDGET_H_ */
