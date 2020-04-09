/***************************************************************************
                          fitsskyobject.cpp  -  FITS Image
                             -------------------
    begin                : Tue Apr 07 2020
    copyright            : (C) 2004 by Jasem Mutlaq, (C) 2020 by Eric Dejouhanet
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Some code fragments were adapted from Peter Kirchgessner's FITS plugin*
 *   See http://members.aol.com/pkirchg for more details.                  *
 ***************************************************************************/

#ifndef FITSSKYOBJECT_H
#define FITSSKYOBJECT_H

#include <QObject>

class SkyObject;

class FITSSkyObject : public QObject
{
    Q_OBJECT

public:
    /** @brief Locate a SkyObject at a pixel position.
     * @param object is the SkyObject to locate in the frame.
     * @param xPos and yPos are the pixel position of the SkyObject in the frame.
     */
    explicit FITSSkyObject(SkyObject /*const*/ *object, int xPos, int yPos);

public:
    /** @brief Getting the SkyObject this instance locates.
     */
    SkyObject /*const*/ *skyObject();

public:
    /** @brief Getting the pixel position of the SkyObject this instance locates. */
    /** @{ */
    int x() const;
    int y() const;
    /** @} */

public:
    /** @brief Setting the pixel position of the SkyObject this instance locates. */
    /** @{ */
    void setX(int xPos);
    void setY(int yPos);
    /** @} */

protected:
    SkyObject /*const*/ *skyObjectStored { nullptr };
    int xLoc { 0 };
    int yLoc { 0 };
};

#endif // FITSSKYOBJECT_H
