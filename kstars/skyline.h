/***************************************************************************
                          skyline.h  -  K Desktop Planetarium
                             -------------------
    begin                : Mon June 26 2006
    copyright            : (C) 2006 by Jason Harris
    email                : kstarss@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKYLINE_H_
#define SKYLINE_H_

#include "skypoint.h"

class dms;

/**
	*@class SkyLine
	*
	*A line segment in the sky, composed of the two SkyPoints at its endpoints.
	*SkyLines are used for constellation lines and boundaries, the coordinate 
	*grid and the equator, ecliptic and horizon.
	*
	*@note the SkyLine is always a straight line, it is not the 
	*Great Circle segment joining the two endpoints.  Therefore, line segments 
	*that need to follow great circles must be approximated with many short 
	*SkyLines.
	*/
class SkyLine {
	public:
		/**
			*Default Constructor.  Create a null SkyLine.
			*/
		SkyLine() { m_p1 = SkyPoint( 0.0, 0.0 ); m_p2 = SkyPoint( 0.0, 0.0 ); }

		/**
			*Constructor.  Create a SkyLine with the endpoints given as 
			*arguments.
			*@param p1 Reference to the SkyLine's start point.
			*@param p2 Reference to the SkyLine's end point.
			*/
		SkyLine( const SkyPoint &start, const SkyPoint &end ) { m_p1 = start; m_p2 = end; }

		/**
			*Constructor.  Create a SkyLine with the endpoints given as 
			*arguments.
			*@param start Pointer to the SkyLine's start point.
			*@param end Pointer to the SkyLine's end point.
			*/
		SkyLine( SkyPoint *start, SkyPoint *end ) { m_p1 = *start; m_p2 = *end; }

		/**
			*Destructor (empty)
			*/
			~SkyLine() {}

		/**
			*@return a const pointer to the SkyLine's start point
			*/
		inline SkyPoint* startPoint() { return &m_p1; }

		/**
			*@return a const pointer to the SkyLine's end point
			*/
		inline SkyPoint* endPoint() { return &m_p2; }

		/**
			*Set the SkyLine's start point
			*@param p1 the new start point
			*/
		inline void setStartPoint( const SkyPoint &p1 ) { m_p1 = p1; }
		inline void setStartPoint( SkyPoint *p1 ) { m_p1 = *p1; }

		/**
			*Set the SkyLine's end point
			*@param p2 the new end point
			*/
		inline void setEndPoint( const SkyPoint &p2 ) { m_p2 = p2; }
		inline void setEndPoint( SkyPoint *p2 ) { m_p2 = *p2; }

		/**
			*@return the angle subtended by the SkyLine
			*/
		dms angularSize();

	private:
		SkyPoint m_p1;
		SkyPoint m_p2;
};

#endif
