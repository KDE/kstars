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

#include <QStringList>
#include "skypoint.h"

class SkyGuides::Slide
{
public:
    Slide(const QString &title, const QString &subtitle, const QString &text, const QStringList &images,
          const SkyPoint centerPoint)
    {
        setSlide(title, subtitle, text, images, centerPoint);
    }

    QString title() const { return m_Title; }
    QString subtitle() const { return m_Subtitle; }
    QString text() const { return m_Text; }
    QStringList images() const { return m_Images; }
    SkyPoint centerPoint() const { return m_CenterPoint; }

    void setSlide(const QString &title, const QString &subtitle, const QString &text, const QStringList &images,
                  const SkyPoint centerPoint);

private:
    QString m_Title;
    QString m_Subtitle;
    QString m_Text;
    QStringList m_Images;
    SkyPoint m_CenterPoint;
    // Links
};

#endif // SLIDE_H
