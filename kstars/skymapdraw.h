/***************************************************************************
                 skymapdrawabstract.h  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Dec 20 2010 05:04 AM UTC-6
    copyright            : (C) 2010 Akarsh Simha
    email                : akarsh.simha@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKYMAPDRAWABSTRACT_H_
#define SKYMAPDRAWABSTRACT_H_

/**
 *@short This class defines the methods that both rendering engines
 *       (GLPainter and QPainter) must implement. This also allows us to add
 *       more rendering engines if required.
 *@version 1.0
 *@author Akarsh Simha <akarsh.simha@kdemail.net>
 */

// In summary, this is a class created by stealing all the
// drawing-related methods from the old SkyMap class

class SkyMapDrawAbstract {

 public:


    // *********************** "IMPURE" VIRTUAL METHODS ******************* //
    // NOTE: The following methods are implemented using native
    //   QPainter in both cases. So it's virtual, but not pure virtual

    /**Draw a dashed line from the Angular-Ruler start point to the current mouse cursor,
    	*when in Angular-Ruler mode.
    	*@param psky reference to the QPainter on which to draw (this should be the Sky pixmap). 
    	*/
    void drawAngleRuler( QPainter &psky );



