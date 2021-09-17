/*
    SPDX-FileCopyrightText: 2019 Patrick Molenaar <pr_molenaar@hotmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HOUGHLINE_H_
#define HOUGHLINE_H_

#include <QLineF>
#include <QPointF>
#include <stdio.h>
#include <qmath.h>

/**
 * @class HoughLine
 * Line representation for HoughTransform
 * Based on the java implementation found on http://vase.essex.ac.uk/software/HoughTransform
 * @author Patrick Molenaar
 * @version 1.0
 */
class HoughLine : public QLineF
{
  public:
    enum IntersectResult {
        PARALLEL,
        COINCIDENT,
        NOT_INTERESECTING,
        INTERESECTING
    };

    HoughLine(double theta, double r, int width, int height, int score);
    virtual ~HoughLine() = default;
    QPointF RotatePoint(int x1, double r, double theta, int width, int height);
    IntersectResult Intersect(const HoughLine& other_line, QPointF& intersection);
    bool DistancePointLine(const QPointF& point, QPointF& intersection, double& distance);
    int getScore() const;
    double getR() const;
    double getTheta() const;
    void setTheta(const double theta);

    HoughLine& operator=(const HoughLine &other)
    {
        theta = other.getTheta();
        r = other.getR();
        score = other.getScore();
        setP1(other.p1());
        setP2(other.p2());
        return *this;
    }

    bool operator<(const HoughLine &other) const
    {
        return (score < other.getScore());
    }

    static bool compareByScore(const HoughLine *line1,const HoughLine *line2);
    static bool compareByTheta(const HoughLine *line1,const HoughLine *line2);
    static void getSortedTopThreeLines(QVector<HoughLine*> &houghLines, QVector<HoughLine*> &top3Lines);

    void printHoughLine();
    void Offset(const int offsetX, const int offsetY);

  private:
    double Magnitude(const QPointF& point1, const QPointF& point2);

    int score;
    double theta;
    double r;
    QPointF beginPoint;
    QPointF endPoint;
};

#endif
