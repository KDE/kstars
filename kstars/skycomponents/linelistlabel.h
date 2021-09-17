/*
    SPDX-FileCopyrightText: 2007 James B. Bowlin <bowlin@mindspring.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QPointF>
#include <QString>

class LineList;
class Projector;
class SkyLabeler;

/**
	* @class LabelListIndex
	* An abstract parent class to be inherited by Ecliptic and Equator.
	*
	* @author James B. Bowlin
	* @version 0.1
	*/
class LineListLabel
{
  public:
    explicit LineListLabel(const QString &text);

    enum
    {
        TopCandidate,
        BotCandidate,
        LeftCandidate,
        RightCandidate
    };

    /** @short prepare the context for selecting label position candidates. */
    void reset();

    /**
     * @short draw the label if any.  Is currently called at the bottom of draw() but that call could
     * be removed and it could be called externally AFTER draw() has been called so draw() can set
     * up the label position candidates.
     */
    void draw();

    void updateLabelCandidates(qreal x, qreal y, LineList *lineList, int i);

  private:
    /**
     * @short This routine does two things at once.  It returns the QPointF
     * corresponding to pointList[i] and also computes the angle using
     * pointList[i] and pointList[i-1] therefore you MUST ensure that:
     *
     *	   1 <= i < pointList.size().
     */
    QPointF angleAt(const Projector *proj, LineList *list, int i, double *angle);

    const QString m_text;
    SkyLabeler *m_skyLabeler { nullptr };

    // these two arrays track/contain 4 candidate points
    int m_labIndex[4] { 0, 0, 0, 0 };
    LineList *m_labList[4] { nullptr, nullptr, nullptr, nullptr };

    float m_marginLeft { 0 };
    float m_marginRight { 0 };
    float m_marginTop { 0 };
    float m_marginBot { 0 };
    float m_farLeft { 0 };
    float m_farRight { 0 };
    float m_farTop { 0 };
    float m_farBot { 0 };
};
