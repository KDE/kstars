/***************************************************************************
                          infoboxes.cpp  -  description
                             -------------------
    begin                : Wed Jun 5 2002
    copyright            : (C) 2002 by Jason Harris
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

#include "infoboxes.h"

#include <KGlobal>
#include <KLocale>
#include <kdebug.h>

#include <QPainter>
#include <QMouseEvent>

#include "kstarsdatetime.h"
#include "dms.h"
#include "geolocation.h"
#include "skyobjects/skypoint.h"
#include "skycomponents/skylabeler.h"

// FIXME: Refactor constructor >14 parameters is WAY to many
InfoBoxes::InfoBoxes( int w, int h, const QPoint &tp, bool tshade,
                      const QPoint &gp, bool gshade, const QPoint &fp, bool fshade,
                      const QColor &colorText, const QColor &colorGrab, const QColor &colorBG ) :
        boxColor(colorText), grabColor(colorGrab), bgColor(colorBG),
        GeoBox(0), FocusBox(0), TimeBox(0)
{
    int tx = tp.x();
    int ty = tp.y();
    int gx = gp.x();
    int gy = gp.y();
    int fx = fp.x();
    int fy = fp.y();

    GrabbedBox = 0;
    GrabPos = QPoint( 0, 0 );
    Visible = true;

    Width = w;
    Height = h;

    //Create infoboxes
    GeoBox   = new InfoBox( gx, gy, gshade, QString(), QString() );
    TimeBox  = new InfoBox( tx, ty, tshade, QString(), QString(), QString() );
    FocusBox = new InfoBox( fx, fy, fshade, QString(), QString(), QString() );
    m_box[0] = GeoBox;
    m_box[1] = TimeBox;
    m_box[2] = FocusBox;

    fixCollisions( TimeBox );
    fixCollisions( FocusBox );

    resize( w, h );
}

// FIXME: Refactor constructor >14 parameters is WAY to many
InfoBoxes::InfoBoxes( int w, int h, int tx, int ty, bool tshade,
                      int gx, int gy, bool gshade, int fx, int fy, bool fshade,
                      const QColor &colorText, const QColor &colorGrab, const QColor &colorBG ) :
boxColor(colorText), grabColor(colorGrab), bgColor(colorBG) {

    GrabbedBox = 0;
    GrabPos = QPoint( 0, 0 );
    Visible = true;

    Width = w;
    Height = h;

    //Create infoboxes
    GeoBox   = new InfoBox( gx, gy, gshade, QString(), QString() );
    TimeBox  = new InfoBox( tx, ty, tshade, QString(), QString(), QString() );
    FocusBox = new InfoBox( fx, fy, fshade, QString(), QString(), QString() );
    m_box[0] = GeoBox;
    m_box[1] = TimeBox;
    m_box[2] = FocusBox;

    fixCollisions( TimeBox );
    fixCollisions( FocusBox );

    resize( w, h );
}

InfoBoxes::~InfoBoxes(){
    delete FocusBox;
    delete TimeBox;
    delete GeoBox;
}

void InfoBoxes::resize( int w, int h ) {
    Width = w;
    Height = h;
    checkBorders(false);
}

void InfoBoxes::drawBoxes( QPainter &p, const QColor &FGColor, const QColor &grabColor,
                           const QColor &bgColor, unsigned int bgMode ) {

    if (  ! isVisible() ) return;

    p.setPen( QPen( FGColor ) );

    for ( int i = 0; i < 3; i++ ) {
        InfoBox* box = m_box[i];
        if ( ! box->isVisible() ) continue;
        if ( GrabbedBox == i + 1 ) {
            p.setPen( QPen( grabColor ) );
            p.drawRect( box->x(), box->y(), box->width(), box->height() );
            p.setPen( QPen( FGColor ) );
        }
        box->draw( p, bgColor, bgMode );
        m_saveFocus[i][0] = box->x();
        m_saveFocus[i][1] = box->y();
    }
}

void InfoBoxes::reserveBoxes( QPainter& psky )
{
    if ( ! isVisible() ) return;

    SkyLabeler* skyLabeler = SkyLabeler::Instance();
    for ( int i = 0; i < 3; i++ ) {
        InfoBox* box = m_box[i];
        if ( ! box->isVisible() )
            continue;
        // FIXME: m_saveFocus could be uninitialized in this function 
        int x = m_saveFocus[i][0];
        int y = m_saveFocus[i][1];
        skyLabeler->markRect( x, y, box->width(), box->height(), psky );
    }
}

bool InfoBoxes::grabBox( QMouseEvent *e ) {
    if ( GeoBox->rect().contains( e->pos() ) ) {
        GrabbedBox = 1;
        GrabPos.setX( e->x() - GeoBox->x() );
        GrabPos.setY( e->y() - GeoBox->y() );
        return true;
    } else if ( TimeBox->rect().contains( e->pos() ) ) {
        GrabbedBox = 2;
        GrabPos.setX( e->x() - TimeBox->x() );
        GrabPos.setY( e->y() - TimeBox->y() );
        return true;
    } else if ( FocusBox->rect().contains( e->pos() ) ) {
        GrabbedBox = 3;
        GrabPos.setX( e->x() - FocusBox->x() );
        GrabPos.setY( e->y() - FocusBox->y() );
        return true;
    } else {
        GrabbedBox = 0; //this is probably redundant, because mouseRelease should call unGrabBox()...
        return false;
    }
}

bool InfoBoxes::unGrabBox( void ) {
    if ( GrabbedBox ) {
        GrabbedBox = 0;
        checkBorders();
        return true;
    } else {
        return false;
    }
}

bool InfoBoxes::dragBox( QMouseEvent *e ) {
    switch( GrabbedBox ) {
    case 1: //GeoBox
        GeoBox->move( e->x() - GrabPos.x(), e->y() - GrabPos.y() );
        fixCollisions( GeoBox );
        return true;
        break;
    case 2: //TimeBox
        TimeBox->move( e->x() - GrabPos.x(), e->y() - GrabPos.y() );
        fixCollisions( TimeBox );
        return true;
        break;
    case 3: //FocusBox
        FocusBox->move( e->x() - GrabPos.x(), e->y() - GrabPos.y() );
        fixCollisions( FocusBox );
        return true;
        break;
    default: //no box is grabbed; return false
        return false;
    }
}

bool InfoBoxes::shadeBox( QMouseEvent *e ) {
    if ( GeoBox->rect().contains( e->pos() ) ) {
        GeoBox->toggleShade();
        if ( GeoBox->rect().bottom() > height() ) GeoBox->move( GeoBox->x(), height() - GeoBox->height() );
        if ( GeoBox->rect().right() > width() ) GeoBox->move( width() - GeoBox->width(), GeoBox->y() );
        if ( GeoBox->anchorBottom() ) GeoBox->move( GeoBox->x(), height() - GeoBox->height() );
        if ( GeoBox->anchorRight() ) GeoBox->move( width() - GeoBox->width(), GeoBox->y() );
        fixCollisions( TimeBox );
        fixCollisions( FocusBox );
        return true;
    } else if ( TimeBox->rect().contains( e->pos() ) ) {
        TimeBox->toggleShade();
        if ( TimeBox->rect().bottom() > height() ) TimeBox->move( TimeBox->x(), height() - TimeBox->height() );
        if ( TimeBox->rect().right() > width() ) TimeBox->move( width() - TimeBox->width(), TimeBox->y() );
        if ( TimeBox->anchorBottom() ) TimeBox->move( TimeBox->x(), height() - TimeBox->height() );
        if ( TimeBox->anchorRight() ) TimeBox->move( width() - TimeBox->width(), TimeBox->y() );
        fixCollisions( GeoBox );
        fixCollisions( FocusBox );
        return true;
    } else if ( FocusBox->rect().contains( e->pos() ) ) {
        FocusBox->toggleShade();
        if ( FocusBox->rect().bottom() > height() ) FocusBox->move( FocusBox->x(), height() - FocusBox->height() );
        if ( FocusBox->rect().right() > width() ) FocusBox->move( width() - FocusBox->width(), FocusBox->y() );
        if ( FocusBox->anchorBottom() ) FocusBox->move( FocusBox->x(), height() - FocusBox->height() );
        if ( FocusBox->anchorRight() ) FocusBox->move( width() - FocusBox->width(), FocusBox->y() );
        fixCollisions( TimeBox );
        fixCollisions( GeoBox );
        return true;
    }
    return false;
}

bool InfoBoxes::fixCollisions( InfoBox *target ) {
    int dLeft(0), dRight(0), dUp(0), dDown(0);
    QRect area = QRect( 0, 0, Width, Height );
    QRect t = target->rect();
    QRect Box1, Box2;

    //Set Box1 and Box2 to the rects of the other two InfoBoxes, unless
    //they are not visible (if so, set a null QRect)
    if ( target == GeoBox ) {
        if ( FocusBox->isVisible() ) Box1 = FocusBox->rect();
        else Box1 = QRect(0,0,0,0);

        if ( TimeBox->isVisible() ) Box2 = TimeBox->rect();
        else Box2 = QRect(0,0,0,0);

    } else if ( target == FocusBox ) {
        if ( GeoBox->isVisible() ) Box1 = GeoBox->rect();
        else Box1 = QRect(0,0,0,0);

        if ( TimeBox->isVisible() ) Box2 = TimeBox->rect();
        else Box2 = QRect(0,0,0,0);

    } else if ( target == TimeBox ) {
        if ( FocusBox->isVisible() ) Box1 = FocusBox->rect();
        else Box1 = QRect(0,0,0,0);

        if ( GeoBox->isVisible() ) Box2 = GeoBox->rect();
        else Box2 = QRect(0,0,0,0);

    } else { return false; } //none of the Boxes match target!

    //Shrink Box1 and Box2 by one pixel in each direction.  This will make the
    //Edges of adjacent boxes line up more nicely.
    if ( Box1.width() ) Box1.setCoords( Box1.left()+1, Box1.top()+1, Box1.right()-1, Box1.bottom()-1 );
    if ( Box2.width() ) Box2.setCoords( Box2.left()+1, Box2.top()+1, Box2.right()-1, Box2.bottom()-1 );

    //First, make sure target box is within area rect.
    if ( ! area.contains( t ) ) {
        /*		if ( t.x() < area.x() ) target->move( area.x(), t.y() );
        		if ( t.y() < area.y() ) target->move( t.x(), area.y() );
        		if ( t.right() > area.right() ){ target->move( area.right() - t.width(), t.y() ); }
        		if ( t.bottom() > area.bottom() ) target->move( t.x(), area.bottom() - t.height() );*/

        int x = t.x(), y = t.y();

        if ( t.x() < area.x() ) x = area.x();
        if ( t.y() < area.y() ) y = area.y();
        if ( t.right() > area.right() ) x = area.right() - t.width();
        if ( t.bottom() > area.bottom() ) y = area.bottom() - t.height();
        target->move(x,y);

        //Reset t
        t = target->rect();
    }

    QRect upRect = t;
    QRect downRect = t;
    QRect leftRect = t;
    QRect rightRect = t;

    //Fix collisions
    if ( t.intersects( Box1 ) || t.intersects( Box2 ) ) {
        //move t to the left one pixel at a time until there is no
        //intersection with Box1 or Box2.
        while ( leftRect.intersects( Box1 ) || leftRect.intersects( Box2 ) ) {
            ++dLeft;
            leftRect.moveTopLeft( QPoint( t.x() - dLeft, t.y() ) );
        }
        //If leftRect is outside area, set dLeft to a nonsense large value
        if ( !area.contains( leftRect ) ) { dLeft = 100000; }
        //repeat for right, up and down directions.
        while ( rightRect.intersects( Box1 ) || rightRect.intersects( Box2 ) ) {
            ++dRight;
            rightRect.moveTopLeft( QPoint( t.x() + dRight, t.y() ) );
        }
        if ( !area.contains( rightRect ) ) { dRight = 100000; }

        while ( upRect.intersects( Box1 ) || upRect.intersects( Box2 ) ) {
            ++dUp;
            upRect.moveTopLeft( QPoint( t.x(), t.y() - dUp ) );
        }
        if ( !area.contains( upRect ) ) { dUp = 100000; }

        while ( downRect.intersects( Box1 ) || downRect.intersects( Box2 ) ) {
            ++dDown;
            downRect.moveTopLeft( QPoint( t.x(), t.y() + dDown ) );
        }
        if ( !area.contains( downRect ) ) { dDown = 100000; }


        //find the smallest displacement, and move the target box there.
        //if the smallest displacement is 100000, then the function has failed
        //to find any legal position; return false.
        int dmin = dLeft;
        if ( dRight < dmin ) dmin = dRight;
        if ( dDown < dmin ) dmin = dDown;
        if ( dUp < dmin ) dmin = dUp;

    if ( dmin == 100000 ) { return false; }
        else if ( dmin == dLeft ) {
            target->move( leftRect.x(), leftRect.y() );
        } else if ( dmin == dRight ) {
            target->move( rightRect.x(), rightRect.y() );
        } else if ( dmin == dUp ) {
            target->move( upRect.x(), upRect.y() );
        } else if ( dmin == dDown ) {
            target->move( downRect.x(), downRect.y() );
        }
    }
    else  // no intersection with other boxes
        return true;

    //Set Anchor flags based on new position
    if ( target->rect().right() >= width()-2 ) target->setAnchorRight( true );
    else target->setAnchorRight( false );
    if ( target->rect().bottom() >= height()-2 ) target->setAnchorBottom(true);
    else target->setAnchorBottom(false);

    //Final check to see if we're still inside area (we may not be if target
    //is bigger than area)
    if ( area.contains( target->rect() ) ) return true;
    else return false;
}

bool InfoBoxes::timeChanged( const KStarsDateTime &ut, const KStarsDateTime &lt, dms *lst ) {
    QString ot1 = TimeBox->text1();
    QString ot2 = TimeBox->text2();
    QString ot3 = TimeBox->text3();

    TimeBox->setText1( i18nc( "Local Time", "LT: " ) + 
                        KGlobal::locale()->formatTime( lt.time(), true ) + "   " + 
                        KGlobal::locale()->formatDate( lt.date() ) );
    TimeBox->setText2( i18nc( "Universal Time", "UT: " ) + 
                        KGlobal::locale()->formatTime( ut.time(), true ) + "   " + 
                        KGlobal::locale()->formatDate( ut.date() ) );

    QString STString;
    STString = STString.sprintf( "%02d:%02d:%02d   ", lst->hour(), lst->minute(), lst->second() );

    //Don't use KLocale::formatNumber() for Julian Day because we don't want
    //thousands-place separators
    QString JDString = QString::number( ut.djd(), 'f', 2 );
    JDString.replace( '.', KGlobal::locale()->decimalSymbol() );

    TimeBox->setText3( i18nc( "Sidereal Time", "ST: " ) + STString +
                       i18nc( "Julian Day", "JD: " ) + JDString );

    if ( ot1 == TimeBox->text1() && ot2 == TimeBox->text2() &&
            ot3 == TimeBox->text3() )
        return false;
    else {
        checkBorders();
        return true;
    }
}

bool InfoBoxes::geoChanged(const GeoLocation *geo) {
    QString ot1 = GeoBox->text1();
    QString ot2 = GeoBox->text2();

    QString name = geo->translatedName() + ", ";
    if ( ! geo->province().isEmpty() ) name += geo->translatedProvince() + ",  ";
    name += geo->translatedCountry();
    GeoBox->setText1( name );

    GeoBox->setText2( i18nc( "Longitude", "Long:" ) + ' ' +
                      KGlobal::locale()->formatNumber( geo->lng()->Degrees(),3) + "   " +
                      i18nc( "Latitude", "Lat:" ) + ' ' +
                      KGlobal::locale()->formatNumber( geo->lat()->Degrees(),3) );

    if ( ot1 == GeoBox->text1() && ot2 == GeoBox->text2() )
        return false;
    else {
        checkBorders();
        return true;
    }
}

bool InfoBoxes::focusObjChanged( const QString &n ) {
    QString ot1 = FocusBox->text1();

    FocusBox->setText1( i18n( "Focused on: " ) + n );
    if ( ot1 == FocusBox->text1() ) return false;
    else {
        checkBorders();
        return true;
    }
}

bool InfoBoxes::focusCoordChanged(const SkyPoint *p) {
    QString ot2 = FocusBox->text2();
    QString ot3 = FocusBox->text3();

    FocusBox->setText2( i18nc( "Right Ascension", "RA" ) + ": " + p->ra()->toHMSString() +
                        "  " + i18nc( "Declination", "Dec" ) +  ": " + p->dec()->toDMSString(true) );
    FocusBox->setText3( i18nc( "Azimuth", "Az" ) + ": " + p->az()->toDMSString(true) +
                        "  " + i18nc( "Altitude", "Alt" ) + ": " + p->alt()->toDMSString(true) );

    if ( ot2 == FocusBox->text2() && ot3 == FocusBox->text3() )
        return false;
    else {
        checkBorders();
        return true;
    }
}

void InfoBoxes::checkBorders(bool resetToDefault) {
    if (resetToDefault) {
        TimeBox->setAnchorFlag( InfoBox::AnchorNone );
        GeoBox->setAnchorFlag( InfoBox::AnchorNone );
        FocusBox->setAnchorFlag( InfoBox::AnchorNone );
    }

    if ( GeoBox->rect().right()    >= Width-2  ) GeoBox->setAnchorRight( true );
    if ( TimeBox->rect().right()   >= Width-2  ) TimeBox->setAnchorRight( true );
    if ( FocusBox->rect().right()  >= Width-2  ) FocusBox->setAnchorRight( true );
    if ( GeoBox->rect().bottom()   >= Height-2 ) GeoBox->setAnchorBottom( true );
    if ( TimeBox->rect().bottom()  >= Height-2 ) TimeBox->setAnchorBottom( true );
    if ( FocusBox->rect().bottom() >= Height-2 ) FocusBox->setAnchorBottom( true );

    if ( GeoBox->anchorRight()    ) GeoBox->move( Width, GeoBox->y() );
    if ( TimeBox->anchorRight()   ) TimeBox->move( Width, TimeBox->y() );
    if ( FocusBox->anchorRight()  ) FocusBox->move( Width, FocusBox->y() );
    if ( GeoBox->anchorBottom()   ) GeoBox->move( GeoBox->x(), Height );
    if ( TimeBox->anchorBottom()  ) TimeBox->move( TimeBox->x(), Height );
    if ( FocusBox->anchorBottom() ) FocusBox->move( FocusBox->x(), Height );
}

#include "infoboxes.moc"
