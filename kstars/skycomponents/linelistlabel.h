
/***************************************************************************
					  linelistlabel.h  -  K Desktop Planetarium
							 -------------------
	begin				: 2007-08-08
	copyright			: (C) 2007 by James B. Bowlin
	email				: bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *																		 *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or	 *
 *   (at your option) any later version.								   *
 *																		 *
 ***************************************************************************/

#ifndef LINE_LIST_LABEL_H
#define LINE_LIST_LABEL_H


#include <QPointF>

class SkyLabeler;
class Projector;
class LineList;

/**
	*@class LabelListIndex
	*An abstract parent class to be inherited by Ecliptic and Equator. 
	*
	*@author James B. Bowlin
	*@version 0.1
	*/
class LineListLabel
{
public:

    explicit LineListLabel( const QString& text );

    enum { TopCandidate, BotCandidate, LeftCandidate, RightCandidate };

    /* @short prepare the context for selecting label position candidates.
     */
    void reset();

    /* @short draw the label if any.  Is currently called at the bottom of
     * draw() but that call could be removed and it could be called
     * externally AFTER draw() has been called so draw() can set up the label
     * position candidates.
     */
    void draw();

    void updateLabelCandidates( qreal x, qreal y,  LineList* lineList, int i );

private:
    const QString m_text;
    SkyLabeler*   m_skyLabeler;

    // these two arrays track/contain 4 candidate points
    int         m_labIndex[4];
    LineList*   m_labList[4];

    float       m_marginLeft, m_marginRight, m_marginTop, m_marginBot;
    float       m_farLeft, m_farRight, m_farTop, m_farBot;

    /* @short This routine does two things at once.  It returns the QPointF
     * corresponding to pointList[i] and also computes the angle using
     * pointList[i] and pointList[i-1] therefore you MUST ensure that:
     *
     *	   1 <= i < pointList.size().
     */
    QPointF angleAt( const Projector *proj, LineList* list, int i,
                     double *angle );
};

#endif
