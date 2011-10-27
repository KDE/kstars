/***************************************************************************
                          slide.h  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Oct 10 2011
    copyright            : (C) 2011 by Łukasz Jaśkiewicz
    email                : lucas.jaskiewicz@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SLIDE_H
#define SLIDE_H

#include "skyguides.h"

#include <QList>
#include "skypoint.h"

class SkyGuides::Slide
{
public:
    Slide(const QString &title, const QString &subtitle, const QString &text, const SkyPoint centerPoint,
          const QList<Image*> &images, const QList<Link*> &links)
    {
        setSlide(title, subtitle, text, centerPoint, images, links);
    }

    QString title() const { return m_Title; }
    QString subtitle() const { return m_Subtitle; }
    QString text() const { return m_Text; }
    SkyPoint centerPoint() const { return m_CenterPoint; }
    QList<Image*>* images() { return &m_Images; }
    QList<Link*>* links() { return &m_Links; }

    void setSlide(const QString &title, const QString &subtitle, const QString &text, const SkyPoint centerPoint,
                  const QList<Image*> &images, const QList<Link*> &links);

private:
    QString m_Title;
    QString m_Subtitle;
    QString m_Text;
    SkyPoint m_CenterPoint;
    QList<Image*> m_Images;
    QList<Link*> m_Links;
};

#endif // SLIDE_H
