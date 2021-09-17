/*
    SPDX-FileCopyrightText: 2018 Valentin Boettcher <valentin@boettcher.cf>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include "eclipsehandler.h"
#include "ksearthshadow.h"
#include "ksmoon.h"
#include "kssun.h" // NOTE: Maybe use pointers... compile time savings

// FIXME: (Valentin) WIP, To be completed (extra features)!

/**
 * @brief The LunarEclipseDetails struct
 * @short A struct to hold detail information about an eclipse
 */
struct LunarEclipseDetails {
    enum EVENT {
        BEGIN_PENUMBRA_CONTACT,
        BEGIN_UMBRA_CONTACT,
        END_PENUMBRA_CONTACT,
        END_UMBRA_CONTACT,
        BEGIN_FULL_UMBRA,
        END_FULL_UMBRA,
        BEGIN_FULL_PENUMRA,
        END_FULL_PENUMRA,
        CLOSEST_APPROACH
    };

    bool available = false;
    QMap<long double, EVENT> eclipseTimes;
    // More Later
};

/**
 * @brief The LunarEclipseEvent class
 * @short implementation of the EclipseEvent for the LunarEclipseHandler
 */
class LunarEclipseEvent : public EclipseEvent
{
    Q_OBJECT
public:
    LunarEclipseEvent(long double jd, GeoLocation geoPlace, ECLIPSE_TYPE type, KSEarthShadow::ECLIPSE_TYPE detailed_type);

    ~LunarEclipseEvent() override {} // empty for now

    QString getExtraInfo() override;

    /**
     * @brief getDetailedType
     * @return Type of the eclipse as in KSEarthShadow::ECLIPSE_TYPE
     */
    KSEarthShadow::ECLIPSE_TYPE getDetailedType() { return m_detailedType; }

    QString getEclipsingObjectName() override { return i18n("Earth Shadow"); }
    QString getEclipsedObjectName() override  { return i18n("Moon"); }

    SkyObject * getEclipsingObjectFromSkyComposite() override;

    bool hasDetails() override { return false; } // false for now!

public slots:
    void slotShowDetails() override;

private:
    KSEarthShadow::ECLIPSE_TYPE m_detailedType;
    LunarEclipseDetails m_details;
    // More Later_details;
};


/**
 * @brief The LunarEclipseHandler class
 * @short Calculate lunar eclipses.
 *
 * Calculates lunar eclipses by looking for them close to full moon.
 */
class LunarEclipseHandler : public EclipseHandler
{
    Q_OBJECT
public:
    explicit LunarEclipseHandler(QObject * parent = nullptr);
    virtual ~LunarEclipseHandler() override;

    EclipseVector computeEclipses(long double startJD, long double endJD) override;

    // FIXME: (Valentin) Not yet finished. Returns empty Details!
    LunarEclipseDetails findEclipseDetails(LunarEclipseEvent *event);

protected:
    double findInitialStep(long double, long double) override
        { return (m_mode == CLOSEST_APPROACH) ? INITIAL_STEP : DETAIL_STEP; }

    void updatePositions(long double jd) override;

    // NOTE: This method depends on m_mode!
    dms findDistance() override;

    // NOTE: This method depends on m_mode!
    double getMaxSeparation() override;

private:
    /**
     * @brief getFullMoons
     * @param startJD start Date
     * @param endJD end Date
     * @return a vector of JDs for full moons (actually a little earlier as it doesn't matter much)
     */
    QVector<long double> getFullMoons(long double startJD, long double endJD);

    // Objects for the Calculations
    KSSun m_sun;
    KSMoon m_moon;
    KSEarthShadow m_shadow;

    // Controls the step size, is chosen small to minimize overshooting
    const double INITIAL_STEP { 0.1 };
    const double DETAIL_STEP { 0.001 };

    /**
     * @brief The MODE enum
     * @short Set the mode for the distance minimizer, i.e. which point to find.
     */
    enum {
        CLOSEST_APPROACH,
        PENUMBRA_CONTACT,
        PUNUMBRA_IMMERSION,
        UMBRA_CONTACT,
        UMBRA_IMMERSION
    } m_mode { CLOSEST_APPROACH };
};
