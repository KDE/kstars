/*
    SPDX-FileCopyrightText: 2018 Valentin Boettcher <valentin@boettcher.cf>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include "geolocation.h"
#include "approachsolver.h"
#include "skycomponents/typedef.h"

class KSPlanetBase;

/**
 * @brief The EclipseEvent class
 * @short Abstract container/interface for a eclipse event.
 * @note We do not use the QObject hierarchy here as it would be inefficient
 */
class EclipseEvent : public QObject {
    Q_OBJECT

public:
    /**
     * @brief The ECLIPSE_TYPE_T enum
     * @short Basic eclipse type. May be supplemented
     * by subclasses.
     */
    enum ECLIPSE_TYPE {
        PARTIAL,
        FULL
    };

    EclipseEvent(long double jd, GeoLocation geoPlace, ECLIPSE_TYPE type);

    virtual ~EclipseEvent(); // empty for now

    /**
     * @brief getJD
     * @return the julian date of the event
     */
    long double getJD() { return m_jd; }

    /**
     * @brief getExtraInfo
     * @return information to display in an extra column
     * of the overview table.
     */
    virtual QString getExtraInfo() { return ""; }

    /**
     * @brief getType
     * @return the type of the eclipse
     */
    ECLIPSE_TYPE getType() { return m_type; }

    /**
     * @brief getEclipsingObjectName
     * @return the name of the eclipsing object
     * @note maybe store those objects as clones...
     */
    virtual QString getEclipsingObjectName() = 0;

    /**
     * @brief getEclipsingObjectName
     * @return the name of the eclipsed object
     * @note maybe store those objects as clones...
     */
    virtual QString getEclipsedObjectName() = 0;

    /**
     * @brief getGeolocation
     * @return geolocation for which the event is valid
     */
    GeoLocation getGeolocation() { return m_geoPlace; }

    /**
     * @brief getEclipsingObjectFromSkyComposite
     * @return a pointer to the skymap instance of the ecl. obj.
     */
    virtual SkyObject * getEclipsingObjectFromSkyComposite() = 0;

    /**
     * @brief hasDetails
     * @return whether a details widget can be shown
     */
    virtual bool hasDetails() { return false; }

public slots:
    /**
     * @brief showDetails
     * @short (if implemented) shows a widget with details about the eclipse
     */
    virtual void slotShowDetails() { return; }

private:
    ECLIPSE_TYPE m_type;
    GeoLocation m_geoPlace;

    /**
     * @brief jd - date of the event
     */
    long double m_jd;
};

/**
 * @brief The EclipseHandler class
 *
 * This is a base class for providing a common interface
 * for eclipse events which can be quite different in nature. It is
 * meant to be subclassed. (Check LunarEclipseHandler as an example)
 *
 * @todo remove uglieness from KSConjunct (export m_object... functionality to separate class!,
 *   that findinitialstepsize isn't nice either)
 *
 * @note I've integrated the `findDetails` stuff in the eclipse handler because it already has
 *  the `beef` it takes (instances and methods). OOP is not always the way.
 */
class EclipseHandler : public ApproachSolver
{
    Q_OBJECT
public:
    typedef QVector<EclipseEvent_s> EclipseVector;

    explicit EclipseHandler(QObject * parent = nullptr);
    virtual ~EclipseHandler() override;

    /**
     * @brief compute
     * @short Implements the details for finding *all* the eclipses
     * in a given time-frame. Should call findEclipse intelligently.
     * e.g. only if the moon is full for lunar eclipses et-cetera
     *
     * @returns A vector of shared pointers to eclipse events.
     */
    virtual EclipseVector computeEclipses(long double startJD, long double endJD) = 0;

    /**
     * @brief getEvents
     * @short May be used if the return value of computeEclipses is being ignored.
     * @note The underlying vector changes after every call to computeEclipses.
     *
     * @return A vector of shared pointers to eclipse events.
     */
    QVector<EclipseEvent_s> getEvents() { return m_events; }

signals:
    /**
     * @brief signalEventFound
     * @short A signal to be dispatched as soon as a new Event is found.
     * @note Has to emitted by a subclass!
     * @param event
     */
    void signalEventFound(EclipseEvent_s event);

    /**
     * @brief signalProgress
     * @short gives the progress of the computation in percent
     */
    void signalProgress(int);

    /**
     * @brief signalComputationFinished
     * @short signals the end of the computation
     */
    void signalComputationFinished();

protected:
    virtual double findInitialStep(long double startJD, long double stopJD) override { return double(stopJD - startJD) / 4.0; }

private:
    QVector<EclipseEvent_s> m_events;
};
