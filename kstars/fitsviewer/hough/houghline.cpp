/*
    SPDX-FileCopyrightText: 2019 Patrick Molenaar <pr_molenaar@hotmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "houghline.h"

#include <qmath.h>
#include <QVector>
#include <fits_debug.h>

/**
 * Initialises the hough line
 */
HoughLine::HoughLine(double theta, double r, int width, int height, int score)
    : QLineF()
{
    this->theta = theta;
    this->r = r;
    this->score = score;

    setP1(RotatePoint(0, r, theta, width, height));
    setP2(RotatePoint(width - 1, r, theta, width, height));
}

QPointF HoughLine::RotatePoint(int x1, double r, double theta, int width, int height)
{
    int hx, hy;

    hx = qFloor((width + 1) / 2.0);
    hy = qFloor((height + 1) / 2.0);

    double sinAngle = qSin(-theta);
    double cosAngle = qCos(-theta);

    // translate point back to origin:
    double x2 = x1 - hx;
    double y2 = r - hy;

    // rotate point
    double xnew = x2 * cosAngle - y2 * sinAngle;
    double ynew = x2 * sinAngle + y2 * cosAngle;

    // translate point back:
    x2 = xnew + hx;
    y2 = ynew + hy;

    return QPointF(x2, y2);
}

int HoughLine::getScore() const
{
    return score;
}

double HoughLine::getR() const
{
    return r;
}

double HoughLine::getTheta() const
{
    return theta;
}

void HoughLine::setTheta(const double theta)
{
    this->theta = theta;
}

bool HoughLine::compareByScore(const HoughLine *line1,const HoughLine *line2)
{
    return (line1->getScore() < line2->getScore());
}

bool HoughLine::compareByTheta(const HoughLine *line1,const HoughLine *line2)
{
    return (line1->getTheta() < line2->getTheta());
}

void HoughLine::printHoughLine()
{
    qCDebug(KSTARS_FITS) << "Houghline: [score: " << score << ", r: " << r << ", theta: " << theta << " [rad]="
                         << (theta * 180.0 / M_PI) << " [deg], p1: " << p1().x() << ", " << p1().y() << ", p2: "
                         << p2().x() << ", " << p2().y() << "]";
}

/**
 * Sources for intersection and distance calculations came from
 *     http://paulbourke.net/geometry/pointlineplane/
 * Also check https://doc.qt.io/archives/qt-4.8/qlinef.html for more line methods
 */
HoughLine::IntersectResult HoughLine::Intersect(const HoughLine& other_line, QPointF& intersection)
{
    double denom = ((other_line.p2().y() - other_line.p1().y()) * (p2().x() - p1().x())) -
            ((other_line.p2().x() - other_line.p1().x()) * (p2().y() - p1().y()));

    double nume_a = ((other_line.p2().x() - other_line.p1().x()) * (p1().y() - other_line.p1().y())) -
            ((other_line.p2().y() - other_line.p1().y()) * (p1().x() - other_line.p1().x()));

    double nume_b = ((p2().x() - p1().x()) * (p1().y() - other_line.p1().y())) -
            ((p2().y() - p1().y()) * (p1().x() - other_line.p1().x()));

    if (denom == 0.0f)
    {
        if(nume_a == 0.0f && nume_b == 0.0f)
        {
            return COINCIDENT;
        }
        return PARALLEL;
    }

    double ua = nume_a / denom;
    double ub = nume_b / denom;

    if(ua >= 0.0f && ua <= 1.0f && ub >= 0.0f && ub <= 1.0f)
    {
        // Get the intersection point.
        intersection.setX(qreal(p1().x() + ua * (p2().x() - p1().x())));
        intersection.setY(qreal(p1().y() + ua * (p2().y() - p1().y())));
        return INTERESECTING;
    }

    return NOT_INTERESECTING;
}

double HoughLine::Magnitude(const QPointF& point1, const QPointF& point2)
{
    QPointF vector = point2 - point1;
    return qSqrt(vector.x() * vector.x() + vector.y() * vector.y());
}

bool HoughLine::DistancePointLine(const QPointF& point, QPointF& intersection, double& distance)
{
    double lineMag = length();

    double U = qreal((((point.x() - p1().x()) * (p2().x() - p1().x())) +
            ((point.y() - p1().y()) * (p2().y() - p1().y()))) /
            (lineMag * lineMag));

    if (U < 0.0 || U > 1.0) {
        return false; // closest point does not fall within the line segment
    }

    intersection.setX(p1().x() + U * (p2().x() - p1().x()));
    intersection.setY(p1().y() + U * (p2().y() - p1().y()));

    distance = Magnitude(point, intersection);

    return true;
}

void HoughLine::Offset(const int offsetX, const int offsetY)
{
    setP1(QPointF(p1().x() + offsetX, p1().y() + offsetY));
    setP2(QPointF(p2().x() + offsetX, p2().y() + offsetY));
}

void HoughLine::getSortedTopThreeLines(QVector<HoughLine*> &houghLines, QVector<HoughLine*> &top3Lines)
{
    // Sort houghLines by score (highest scores are clearest lines)
    // For use of sort compare methods see: https://www.off-soft.net/en/develop/qt/qtb1.html
    qSort(houghLines.begin(), houghLines.end(), HoughLine::compareByScore);

    // Get top three lines (these should represent the three lines matching the bahtinov mask lines
    top3Lines = houghLines.mid(0, 3);
    // Verify the angle of these lines with regard to the bahtinov mask angle, correct the angle if necessary
    HoughLine* lineR = top3Lines[0];
    HoughLine* lineG = top3Lines[1];
    HoughLine* lineB = top3Lines[2];
    double thetaR = lineR->getTheta();
    double thetaG = lineG->getTheta();
    double thetaB = lineB->getTheta();
    // Calculate angle between each line
    double bahtinovMaskAngle = qDegreesToRadians(20.0 + 5.0); // bahtinov mask angle plus 5 degree margin
    double dGR = thetaR - thetaG;
    double dBG = thetaB - thetaG;
    double dBR = thetaB - thetaR;
    if (dGR > bahtinovMaskAngle && dBR > bahtinovMaskAngle) {
        // lineR has theta that is 180 degrees rotated
        thetaR -= M_PI;
        // update theta
        lineR->setTheta(thetaR);
    }
    if (dBR > bahtinovMaskAngle && dBG > bahtinovMaskAngle) {
        // lineB has theta that is 180 degrees rotated
        thetaB -= M_PI;
        // update theta
        lineB->setTheta(thetaB);
    }
    if (dGR > bahtinovMaskAngle && dBG > bahtinovMaskAngle) {
        // lineG has theta that is 180 degrees rotated
        thetaG -= M_PI;
        // update theta
        lineG->setTheta(thetaG);
    }
    // Now sort top3lines array according to calculated new angles
    qSort(top3Lines.begin(),top3Lines.end(), HoughLine::compareByTheta);
}
