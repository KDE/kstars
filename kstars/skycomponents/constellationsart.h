/***************************************************************************
                          constellationsart.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2015-05-27
    copyright            : (C) 2015 by M.S.Adityan
    email                : msadityan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef CONSTELLATIONSART_H
#define CONSTELLATIONSART_H

#include <QDebug>
#include <QString>
#include <QDir>
#include "skycomponent.h"
#include "culturelist.h"
#include "kstars/skypainter.h"


/** @class ConstellationsArt
 * @short Represents images for sky cultures
 * @author M.S.Adityan
 */

class ConstellationsArt
{
private:
    QString abbrev, imageFileName;
    int x1,y1,x2,y2,x3,y3,hd1,hd2,hd3;

   /* bool artDisplayed;

    Q_OBJECT
    Q_PROPERTY(bool artDisplayed
    READ getArt
    WRITE setArt
    NOTIFY artDisplayedChanged) */

    /*Draw the constellation art
    void draw( SkyPainter* skyp ) const; */

public:

    //Constructor
    explicit ConstellationsArt(int serial);

    //Destructor
    virtual ~ConstellationsArt();

    //@return the object's x1
    inline int getx1() const { return x1; }

    //@return the object's y1
    inline int gety1() const { return y1; }

    //@return the object's x2
    inline int getx2() const { return x2; }

    //@return the object's y2
    inline int gety2() const { return y2; }

    //@return the object's x3
    inline int getx3() const { return x3; }

    //@return the object's y3
    inline int gety3() const { return y3; }

    //@return the object's hd1
    inline int gethd1() const { return hd1; }

    //@return the object's hd2
    inline int gethd2() const { return hd2; }

    //@return the object's hd3
    inline int gethd3() const { return hd3; }


   /* //Getters and setters
public slots:
    //Sets whether constellation art is be displayed
    void setArt(const bool displayed);

    //Gets if constellation art is displayed
    bool getArt(void) const;

signals:

    void artDisplayedChanged(const bool displayed) const; */

};

#endif // CONSTELLATIONSART_H
