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

#include <QVBoxLayout>
#include <QGridLayout>
#include <QLayout>
#include <QFrame>
#include <QKeyEvent>

#include <kdebug.h>
#include <klocale.h>
#include <KPlotWidget>
#include <KPlotObject>
#include <KPlotAxis>

#include "simclock.h"
#include "dms.h"
#include "ksnumbers.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skyobjects/ksplanet.h"
#include "skyobjects/jupitermoons.h"
#include "skycomponents/skymapcomposite.h"


JMoonTool::JMoonTool(QWidget *parent)
        : KDialog( parent )
{
    ksw = (KStars*)parent;
    QFrame *page = new QFrame(this);
    setMainWidget( page );
    setCaption( i18n("Jupiter Moons Tool") );
    setButtons( KDialog::Close );
    setModal( false );

    QVBoxLayout *vlay = new QVBoxLayout( page );
    vlay->setMargin( 0 );
    vlay->setSpacing( 0 );

    colJp = QColor(Qt::white);
    colIo = QColor(Qt::red);
    colEu = QColor(Qt::yellow);
    colGn = QColor(Qt::cyan);
    colCa = QColor(Qt::green);

    QLabel *labIo = new QLabel( i18n("Io"), page );
    QLabel *labEu = new QLabel( i18n("Europa"), page );
    QLabel *labGn = new QLabel( i18n("Ganymede"), page );
    QLabel *labCa = new QLabel( i18n("Callisto"), page );

    labIo->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    labEu->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    labGn->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    labCa->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );
    labIo->setAlignment( Qt::AlignHCenter );
    labEu->setAlignment( Qt::AlignHCenter );
    labGn->setAlignment( Qt::AlignHCenter );
    labCa->setAlignment( Qt::AlignHCenter );

    QPalette p = palette();
    p.setColor( QPalette::Window, Qt::black );
    p.setColor( QPalette::WindowText, colIo );
    labIo->setPalette( p );
    p.setColor( QPalette::WindowText, colEu );
    labEu->setPalette( p );
    p.setColor( QPalette::WindowText, colGn );
    labGn->setPalette( p );
    p.setColor( QPalette::WindowText, colCa );
    labCa->setPalette( p );
    labIo->setAutoFillBackground( true );
    labEu->setAutoFillBackground( true );
    labGn->setAutoFillBackground( true );
    labCa->setAutoFillBackground( true );

    QGridLayout *glay = new QGridLayout();
    glay->addWidget( labIo, 0, 0 );
    glay->addWidget( labEu, 1, 0 );
    glay->addWidget( labGn, 0, 1 );
    glay->addWidget( labCa, 1, 1 );

    pw = new KPlotWidget( page );
    pw->setShowGrid( false );
    pw->setAntialiasing( true );
    pw->setLimits( -12.0, 12.0, -11.0, 11.0 );
    pw->axis(KPlotWidget::BottomAxis)->setLabel( i18n( "offset from Jupiter (arcmin)" ) );
    pw->axis(KPlotWidget::LeftAxis)->setLabel( i18n( "time since now (days)" ) );
    vlay->addLayout( glay );
    vlay->addWidget( pw );
    resize( 350, 600 );

    initPlotObjects();
    update();
}

JMoonTool::~JMoonTool()
{
}

void JMoonTool::initPlotObjects() {
    KPlotObject *orbit[4];
    KPlotObject *jpath;
    long double jd0 = ksw->data()->ut().djd();
    KSSun *ksun = (KSSun*)ksw->data()->skyComposite()->findByName( "Sun" );
    KSPlanet *jup = (KSPlanet*)ksw->data()->skyComposite()->findByName( i18n("Jupiter") );
    JupiterMoons jm;

    pw->removeAllPlotObjects();

    orbit[0] = new KPlotObject( colIo, KPlotObject::Lines, 1.0 );
    orbit[1] = new KPlotObject( colEu, KPlotObject::Lines, 1.0 );
    orbit[2] = new KPlotObject( colGn, KPlotObject::Lines, 1.0 );
    orbit[3] = new KPlotObject( colCa, KPlotObject::Lines, 1.0 );
    jpath    = new KPlotObject( colJp, KPlotObject::Lines, 1.0 );

    QRectF dataRect = pw->dataRect();
    double dy = 0.01*dataRect.height();

    //t is the offset from jd0, in days.
    for ( double t=dataRect.y(); t<=dataRect.bottom(); t+=dy ) {
        KSNumbers num( jd0 + t );
        jm.findPosition( &num, jup, ksun );

        //jm.x(i) tells the offset from Jupiter, in units of Jupiter's angular radius.
        //multiply by 0.5*jup->angSize() to get arcminutes
        for ( unsigned int i=0; i<4; ++i )
            orbit[i]->addPoint( 0.5*jup->angSize()*jm.x(i), t );

        jpath->addPoint( 0.0, t );
    }

    for ( unsigned int i=0; i<4; ++i )
        pw->addPlotObject( orbit[i] );

    pw->addPlotObject( jpath );
}

void JMoonTool::keyPressEvent( QKeyEvent *e ) {
    QRectF dataRect = pw->dataRect();
    switch ( e->key() ) {
    case Qt::Key_BracketRight:
        {
            double dy = 0.02*dataRect.height();
            pw->setLimits( dataRect.x(), dataRect.right(), dataRect.y()+dy, dataRect.bottom()+dy );
            initPlotObjects();
            pw->update();
            break;
        }
    case Qt::Key_BracketLeft:
        {
            double dy = 0.02*dataRect.height();
            pw->setLimits( dataRect.x(), dataRect.right(), dataRect.y()-dy, dataRect.bottom()-dy );
            initPlotObjects();
            pw->update();
            break;
        }
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        {
            if ( dataRect.height() > 2.0 ) {
                double dy = 0.45*dataRect.height();
                double y0 = dataRect.y() + 0.5*dataRect.height();
                pw->setLimits( dataRect.x(), dataRect.right(), y0-dy, y0+dy );
                initPlotObjects();
                pw->update();
            }
            break;
        }
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        {
            if ( dataRect.height() < 40.0 ) {
                double dy = 0.55*dataRect.height();
                double y0 = dataRect.y() + 0.5*dataRect.height();
                pw->setLimits( dataRect.x(), dataRect.right(), y0-dy, y0+dy );
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

    default: { e->ignore(); break; }
    }
}

#include "jmoontool.moc"
