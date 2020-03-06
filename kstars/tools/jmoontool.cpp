/***************************************************************************
                          jmoontool.cpp  -  Display overhead view of the solar system
                             -------------------
    begin                : Sun May 25 2003
    copyright            : (C) 2003 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "jmoontool.h"

#include "ksnumbers.h"
#include "kstars.h"

#include "skymapcomposite.h"
#include "skyobjects/jupitermoons.h"
#include "skyobjects/ksplanet.h"
#include "skyobjects/kssun.h"

#include <KPlotting/KPlotObject>
#include <KPlotting/KPlotWidget>
#include <KPlotAxis>

#include <QFrame>
#include <QGridLayout>
#include <QKeyEvent>
#include <QVBoxLayout>

JMoonTool::JMoonTool(QWidget *parent) : QDialog(parent)
{
    ksw          = (KStars *)parent;
    QFrame *page = new QFrame(this);

    setWindowTitle(i18n("Jupiter Moons Tool"));
    setModal(false);
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    QVBoxLayout *vlay = new QVBoxLayout;
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);

    setLayout(vlay);

    colJp = QColor(Qt::white);
    colIo = QColor(Qt::red);
    colEu = QColor(Qt::yellow);
    colGn = QColor(Qt::cyan);
    colCa = QColor(Qt::green);

    QLabel *labIo = new QLabel(i18n("Io"), page);
    QLabel *labEu = new QLabel(i18n("Europa"), page);
    QLabel *labGn = new QLabel(i18n("Ganymede"), page);
    QLabel *labCa = new QLabel(i18n("Callisto"), page);

    labIo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    labEu->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    labGn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    labCa->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    labIo->setAlignment(Qt::AlignHCenter);
    labEu->setAlignment(Qt::AlignHCenter);
    labGn->setAlignment(Qt::AlignHCenter);
    labCa->setAlignment(Qt::AlignHCenter);

    QPalette p = palette();
    p.setColor(QPalette::Window, Qt::black);
    p.setColor(QPalette::WindowText, colIo);
    labIo->setPalette(p);
    p.setColor(QPalette::WindowText, colEu);
    labEu->setPalette(p);
    p.setColor(QPalette::WindowText, colGn);
    labGn->setPalette(p);
    p.setColor(QPalette::WindowText, colCa);
    labCa->setPalette(p);
    labIo->setAutoFillBackground(true);
    labEu->setAutoFillBackground(true);
    labGn->setAutoFillBackground(true);
    labCa->setAutoFillBackground(true);

    QGridLayout *glay = new QGridLayout();
    glay->addWidget(labIo, 0, 0);
    glay->addWidget(labEu, 1, 0);
    glay->addWidget(labGn, 0, 1);
    glay->addWidget(labCa, 1, 1);

    pw = new KPlotWidget(page);
    pw->setShowGrid(false);
    pw->setAntialiasing(true);
    pw->setLimits(-12.0, 12.0, -11.0, 11.0);
    pw->axis(KPlotWidget::BottomAxis)->setLabel(i18n("offset from Jupiter (arcmin)"));
    pw->axis(KPlotWidget::LeftAxis)->setLabel(i18n("time since now (days)"));
    vlay->addLayout(glay);
    vlay->addWidget(pw);
    resize(350, 600);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    vlay->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    initPlotObjects();
    update();
}

void JMoonTool::initPlotObjects()
{
    KPlotObject *orbit[4];
    KPlotObject *jpath;
    long double jd0 = ksw->data()->ut().djd();
    KSSun *ksun     = (KSSun *)ksw->data()->skyComposite()->findByName("Sun");
    KSPlanet *jup   = (KSPlanet *)ksw->data()->skyComposite()->findByName(i18n("Jupiter"));
    JupiterMoons jm;

    pw->removeAllPlotObjects();

    orbit[0] = new KPlotObject(colIo, KPlotObject::Lines, 1.0);
    orbit[1] = new KPlotObject(colEu, KPlotObject::Lines, 1.0);
    orbit[2] = new KPlotObject(colGn, KPlotObject::Lines, 1.0);
    orbit[3] = new KPlotObject(colCa, KPlotObject::Lines, 1.0);
    jpath    = new KPlotObject(colJp, KPlotObject::Lines, 1.0);

    QRectF dataRect = pw->dataRect();
    double dy       = 0.01 * dataRect.height();

    /*Debug
    //For testing position of each satellite
    for(double t = dataRect.y(); t <= dataRect.bottom(); t += 1){
        KSNumbers num(jd0 + t);
        jm.findPosition(&num, jup, ksun);

        qDebug() << "Position [Io] : " << 0.5 * jup->angSize() * jm.x(0) << " at " << t << " arcmin";
        qDebug() << "Position [Europa] : " << 0.5 * jup->angSize() * jm.x(1) << " at " << t << " arcmin";
        qDebug() << "Position [Ganymede] : " << 0.5 * jup->angSize() * jm.x(2) << " at " << t << " arcmin";
        qDebug() << "Position [Callisto] : " << 0.5 * jup->angSize() * jm.x(3) << " at " << t << " arcmin";
    }
    */

    //t is the offset from jd0, in days.
    for (double t = dataRect.y(); t <= dataRect.bottom(); t += dy)
    {
        KSNumbers num(jd0 + t);
        jm.findPosition(&num, jup, ksun);

        //jm.x(i) tells the offset from Jupiter, in units of Jupiter's angular radius.
        //multiply by 0.5*jup->angSize() to get arcminutes
        for (unsigned int i = 0; i < 4; ++i)
            orbit[i]->addPoint(0.5 * jup->angSize() * jm.x(i), t);

        jpath->addPoint(0.0, t);
    }

    for (unsigned int i = 0; i < 4; ++i)
        pw->addPlotObject(orbit[i]);

    pw->addPlotObject(jpath);
}

void JMoonTool::keyPressEvent(QKeyEvent *e)
{
    QRectF dataRect = pw->dataRect();
    switch (e->key())
    {
        case Qt::Key_BracketRight:
        {
            double dy = 0.02 * dataRect.height();
            pw->setLimits(dataRect.x(), dataRect.right(), dataRect.y() + dy, dataRect.bottom() + dy);
            initPlotObjects();
            pw->update();
            break;
        }
        case Qt::Key_BracketLeft:
        {
            double dy = 0.02 * dataRect.height();
            pw->setLimits(dataRect.x(), dataRect.right(), dataRect.y() - dy, dataRect.bottom() - dy);
            initPlotObjects();
            pw->update();
            break;
        }
        case Qt::Key_Plus:
        case Qt::Key_Equal:
        {
            if (dataRect.height() > 2.0)
            {
                double dy = 0.45 * dataRect.height();
                double y0 = dataRect.y() + 0.5 * dataRect.height();
                pw->setLimits(dataRect.x(), dataRect.right(), y0 - dy, y0 + dy);
                initPlotObjects();
                pw->update();
            }
            break;
        }
        case Qt::Key_Minus:
        case Qt::Key_Underscore:
        {
            if (dataRect.height() < 40.0)
            {
                double dy = 0.55 * dataRect.height();
                double y0 = dataRect.y() + 0.5 * dataRect.height();
                pw->setLimits(dataRect.x(), dataRect.right(), y0 - dy, y0 + dy);
                initPlotObjects();
                pw->update();
            }
            break;
        }
        case Qt::Key_Escape:
        {
            close();
            break;
        }

        default:
        {
            e->ignore();
            break;
        }
    }
}
