/*
    SPDX-FileCopyrightText: 2010 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SKYMAPQDRAW_H_
#define SKYMAPQDRAW_H_

#include "skymapdrawabstract.h"

#include <QWidget>

/**
 *@short This class draws the SkyMap using native QPainter. It
 * implements SkyMapDrawAbstract
 *@version 1.0
 *@author Akarsh Simha <akarsh.simha@kdemail.net>
 */

class SkyMapQDraw : public QWidget, public SkyMapDrawAbstract
{
    Q_OBJECT

  public:
    /**
         *@short Constructor
         */
    explicit SkyMapQDraw(SkyMap *parent);

    /**
         *@short Destructor
         */
    ~SkyMapQDraw() override;

  protected:
    void paintEvent(QPaintEvent *e) override;

    void resizeEvent(QResizeEvent *e) override;

    QPixmap *m_SkyPixmap;
};

#endif
