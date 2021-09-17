/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarsh.simha@kdemail.net>
    SPDX-FileCopyrightText: 2018 Valentin Boettcher <valentin@boettcher.cf>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include "dms.h"
#include "skyobjects/ksplanet.h"
#include "skycomponents/typedef.h"

#include <QObject>
#include <QMap>
#include <memory>

/**
 * @class ApproachSolver
 * @short Implements algorithms to find close approaches of two objects on the sky.
 * A class that implements a method to compute close approaches between any two solar system
 * objects excluding planetary moons. Given two such objects, this class has implementations of
 * algorithms required to find the time of closest approach in a given range of time. It is meant
 * to be subclassed and provides the boilerplate (+ common interface) for classes like KSKonjunct
 * and EclipseHandler.
 *
 * @author Akarsh Simha
 * @version 2.0
 */
class ApproachSolver : public QObject
{
    Q_OBJECT
public:
    explicit ApproachSolver(QObject *parent = nullptr);

    /**
     * @short Sets the geographic location to compute conjunctions at
     *
     * @param geo  Pointer to the GeoLocation object
     */
    void setGeoLocation(GeoLocation *geo);

    /**
     * @short Compute the closest approach of two planets in the given range
     *
     * @param startJD  Julian Day corresponding to start of the calculation period
     * @param stopJD   Julian Day corresponding to end of the calculation period
     * @param callback A callback function
     * @return Hash containing julian days of close conjunctions against separation
     */
    QMap<long double, dms> findClosestApproach(long double startJD,
                                               long double stopJD,
                                               const std::function<void (long double, dms)> &callback = {}); // FIXME: QMap is awkward!

    /**
     * @brief getGeoLocation
     * @return the currently set GeoLocation
     */
    GeoLocation * getGeoLocation() { return m_geoPlace; }


    /**
     * @brief setMaxSeparation
     * @param sep - maximum allowed anglar separation
     */
    void setMaxSeparation(double sep) { m_maxSeparation = sep; }
    void setMaxSeparation(dms sep) { m_maxSeparation = sep.radians(); }

signals:
    /**
     * @brief solverMadeProgress
     * @param progress - progress in percent
     */
    void solverMadeProgress(int progress);

protected:
    // TODO: This one may be moved to KSConjunct

    /**
     * @brief getMaxSeparation
     * @return the maximum separation allowed, based on the (guaranteed to be up-to-date)
     * parameters of the objects if overwritten. Here it's just a constant.
     */
    virtual double getMaxSeparation() { return m_maxSeparation; }

    /**
     * @brief findSkyPointDistance
     * @param obj1
     * @param obj2
     * @return the angular distance between two SkyPoints in arcminutes
     */
    dms findSkyPointDistance(SkyPoint * obj1, SkyPoint * obj2);

    /**
     * @short Finds the angular distance between two solar system objects.
     * @return The angular distance between the two bodies.
     */
    virtual dms findDistance() = 0;

    /**
     * @brief updatePositions
     * @short Update the positions of the objects involved.
     * @param jd  Julian Day corresponding to the time of computation
     */
    virtual void updatePositions(long double jd) = 0;

    /**
     * @brief findStep
     * @return the step size used by findClosestApproach (in Julian Days)
     *
     * @short Make this as big as possible. The bigger it is, the more likely is a skip over...
     */
    virtual double findInitialStep(long double startJD, long double stopJD) = 0;

    /**
     * @short Compute the precise value of the extremum once the extremum has been detected.
     *
     * @param out  A pointer to a QPair that stores the Julian Day and Separation corresponding to the extremum
     * @param jd  Julian day corresponding to the endpoint of the interval where extremum was detected.
     * @param step  The step in jd taken during computation earlier. (Defines the interval size)
     * @param prevSign The previous sign of increment in moving from jd - step to jd
     *
     * @return true if the extremum is a minimum
     */
    bool findPrecise(QPair<long double, dms> *out, long double jd,
                     double step, int prevSign);

    KSPlanet m_Earth;

private:
    /**
     * @brief updateAndFindDistance
     * @param jd Julian Date for which to calculate
     * @return output of ApproachSolver::findDistance
     */
    dms updateAndFindDistance(long double jd) {
        updatePositions(jd);
        return findDistance();
    }

    /**
     * @short Return the sign of an angle
     *
     * @param a  The angle whose sign has to be returned
     *
     * @return (-1, 0, 1) if a.radians() is (-ve, zero, +ve)
     */
    int sgn(dms a);

    GeoLocation * m_geoPlace { nullptr };
    double m_maxSeparation;
};
