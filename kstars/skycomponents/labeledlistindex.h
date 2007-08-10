
/***************************************************************************
					  labeledlistindex.h  -  K Desktop Planetarium
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

#ifndef LABELED_LIST_INDEX_H
#define LABELED_LIST_INDEX_H

#define NCIRCLE 360   //number of segments used to define equator, ecliptic

#include <QPointF>

#include "noprecessindex.h"

class SkyLabeler;
class SkyMap;

/**
	*@class LabelListIndex
	*An abstract parent class to be inherited by Ecliptic and Equator. 
	*
	*@author James B. Bowlin
	*@version 0.1
	*/
class LabeledListIndex : public NoPrecessIndex
{
	public:
	
		LabeledListIndex( SkyComponent *parent, const QString& name );

		enum { TopCandidate, BotCandidate, LeftCandidate, RightCandidate };

		/* @short we override draw() in order to prepare the context for
		 * selecting label position candidates.
		 */
		void Draw( KStars *kstars, QPainter &psky, double scale );

		/* @short draw the label if any.  Is currently called at the bottom of
		 * draw() but that call could be removed and it could be called
		 * externally AFTER draw() has been called so draw() can set up the label
		 * position candidates.
		 */
		void drawLabels( KStars* kstars, QPainter& psky, double scale );
		
		void updateLabelCandidates( const QPointF& o, LineList* lineList, int i ) {
			updateLabelCandidates( o.x(), o.y(), lineList, i );
		}

		void updateLabelCandidates( const QPoint& o, LineList* lineList, int i ) {
			updateLabelCandidates( (qreal) o.x(), (qreal) o.y(), lineList, i );
		}

		void updateLabelCandidates( qreal x, qreal y,  LineList* lineList, int i );

		virtual void draw( KStars *ks, QPainter &psky, double scale );

		SkyLabeler* skyLabeler() { return m_skyLabeler; }

	private:
		SkyLabeler* m_skyLabeler;

		// these two arrays track/contain 4 candidate points
		int         m_labIndex[4];
		LineList*   m_labList[4];

		float       m_marginLeft, m_marginRight, m_marginTop, m_marginBot;
		float       m_farLeft, m_farRight, m_farTop, m_farBot;
		
		/* @short This routine does two things at once.  It returns the QPointF
		 * coresponding to pointList[i] and also computes the angle using
		 * pointList[i] and pointList[i-1] therefore you MUST ensure that:
		 *
		 *	   1 <= i < pointList.size().
		 */
		QPointF angleAt( SkyMap* map, LineList* list, int i,
						 double *angle, double scale );
};

#endif
