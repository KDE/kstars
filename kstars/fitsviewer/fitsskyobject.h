/*
    SPDX-FileCopyrightText: 2004 Jasem Mutlaq
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Some code fragments were adapted from Peter Kirchgessner's FITS plugin
    SPDX-FileCopyrightText: Peter Kirchgessner <http://members.aol.com/pkirchg>
*/

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
