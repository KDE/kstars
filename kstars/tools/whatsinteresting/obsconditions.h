/*
    SPDX-FileCopyrightText: 2012 Samikshan Bairagya <samikshan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kstarsdata.h"

/**
 * @class ObsConditions
 *
 * This class deals with the observing conditions of the night sky.
 * The limiting magnitude is calculated depending on the equipment
 * available to the user and the amount of light-pollution in the
 * user's current observing location.
 *
 * @author Samikshan  Bairagya
 */
class ObsConditions
{
  public:
    /**
     * @enum Equipment
     *
     * Equipment available to the user.
     */
    enum Equipment
    {
        Telescope,
        Binoculars,
        Both,
        None
    };

    /**
     * @enum TelescopeType
     *
     * Telescope Type (Reflector/Refractor)
     */
    enum TelescopeType
    {
        Reflector = 0,
        Refractor,
        Invalid
    };

    /**
     * @brief Constructor
     *
     * @param bortle          Rating of light pollution based on the bortle dark-sky scale.
     * @param aperture        Aperture of equipment.
     * @param equip           Equipment available to the user.
     * @param telType         Reflector/Refractor type of telescope (if available)
     */
    ObsConditions(int bortle, double aperture, Equipment equip, TelescopeType telType);

    ~ObsConditions() = default;

    /** Inline method to set available equipment */
    inline void setEquipment(Equipment equip) { m_Equip = equip; }

    /** Inline method to set reflector/refractor type for telescope. */
    inline void setTelescopeType(TelescopeType telType) { m_TelType = telType; }

    /** Set limiting magnitude depending on Bortle dark-sky rating. */
    void setLimMagnitude();

    /** Set new observing conditions. */
    void setObsConditions(int bortle, double aperture, Equipment equip, TelescopeType telType);

    /**
     * @brief Get optimum magnification under current observing conditions.
     *
     * @return Get optimum magnification under current observing conditions
     */
    double getOptimumMAG();

    /**
     * @brief Get true limiting magnitude after taking equipment specifications into consideration.
     *
     * @return True limiting magnitude after taking equipment specifications into consideration.
     */
    double getTrueMagLim();

    /**
     * @brief Evaluate visibility of sky-object based on current observing conditions.
     *
     * @param geo       Geographic location of user.
     * @param lst       Local standard time expressed as a dms object.
     * @param so        SkyObject for which visibility is to be evaluated.
     * @return Visibility of sky-object based on current observing conditions as a boolean.
     */
    bool isVisible(GeoLocation *geo, dms *lst, SkyObject *so);

    /**
     * @brief Create QMap<int, double> to be initialised to static member variable m_LMMap
     *
     * @return QMap<int, double> to be initialised to static member variable m_LMMap
     */
    static QMap<int, double> setLMMap();

  private:
    /// Bortle dark-sky rating (from 1-9)
    int m_BortleClass { 0 };
    /// Equipment type
    Equipment m_Equip;
    /// Telescope type
    TelescopeType m_TelType;
    /// Aperture of equipment
    double m_Aperture { 0 };
    /// t-parameter corresponding to telescope type
    double m_tParam { 0 };
    /// Naked-eye limiting magnitude depending on m_BortleClass
    double m_LM { 0 };
    /// Lookup table mapping Bortle Scale values to corresponding limiting magnitudes
    static const QMap<int, double> m_LMMap;
};
