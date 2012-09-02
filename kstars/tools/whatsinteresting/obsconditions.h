/***************************************************************************
                          obsconditions.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/10/07
    copyright            : (C) 2012 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OBS_CONDITIONS_H
#define OBS_CONDITIONS_H

#include "kstarsdata.h"

/**
 * \class ObsConditions
 * This class deals with the observing conditions of the night sky.
 * The limiting magntude is calculated depending on the equipment
 * available to the user and the amount of light-pollution in the
 * user's current observing location.
 * \author Samikshan  Bairagya
 */
class ObsConditions
{
public:
    /**
     * \enum Equipment
     * Equipment available to the user.
     */
    enum Equipment { Telescope = 0, Binoculars, Both, None };

    /**
     * \enum TelescopeType
     * Telescope Type (Reflector/Refractor)
     */
    enum TelescopeType { Reflector = 0, Refractor };

    /**
     * \brief Constructor
     * \param bortle          Rating of light pollution based on the bortle dark-sky scale.
     * \param aperture        Aperture of equipment.
     * \param equip           Equipment available to the user.
     * \param telType         Refelctor/Refractor type of telescope (if available)
     */
    ObsConditions(int bortle, double aperture, Equipment equip, TelescopeType telType);

    /**
     * \brief Destructor
     */
    ~ObsConditions();

    /**
     * \brief Inline method to set available equipment
     */
    inline void setEquipment(Equipment equip) { m_Equip = equip; }

    /**
     * \brief Inline method to set reflector/refractor type for telescope.
     */
    inline void setTelescopeType(TelescopeType telType) { m_TelType = telType; }

    /**
     * \brief Set limiting magnitude depending on Bortle dark-sky rating.
     */
    void setLimMagnitude();

    /**
     * \brief Get optimum magnification under current observing conditions.
     * \return Get optimum magnification under current observing conditions
     */
    double getOptimumMAG();

    /**
     * \brief Get true limiting magnitude after taking equipment specifications into consideration.
     * \return True limiting magnitude after taking equipment specifications into consideration.
     */
    double getTrueMagLim();

    /**
     * \brief Evaluate visibility of sky-object based on current observing conditions.
     * \return Visibility of sky-object based on current observing conditions as a boolean.
     * \param geo       Geographic location of user.
     * \param lst       Local standard time expressed as a dms object.
     * \param so        SkyObject for which visibility is to be evaluated.
     */
    bool isVisible(GeoLocation *geo, dms *lst, SkyObject *so);

private:
    int m_BortleClass;              ///Bortle dark-sky rating (from 1-9)
    Equipment m_Equip;              ///Equipment type
    TelescopeType m_TelType;        ///Telescope type
    double m_Aperture;              ///Aperture of equipment
    double m_tParam;                ///t-parameter corresponding to telescope type
    double m_LM;                    ///Limiting Magnitude depending on m_BortleClass
};

#endif
