/*  Supernova Object
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Based on Samikshan Bairagya GSoC work.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "skyobject.h"

/**
 * @class Supernova
 * Represents the supernova object. It is a subclass of the SkyObject class.
 * This class has the information for different supernovae.
 *
 * N.B. This was modified to use the Open Supernova Project
 *
 * @note The Data File Contains the following parameters
 * @li sName        Designation
 * @li RA           Right Ascension
 * @li Dec          Declination
 * @li type         Supernova Type
 * @li hostGalaxy   Host Galaxy for the supernova
 * @li date         Discovery date yyyy/mm/dd
 * @li sRedShift    Redshift
 * @li sMag         Maximum Apparent magnitude
 *
 * @author Samikshan Bairagya
 * @author Jasem Mutlaq
 */
class Supernova : public SkyObject
{
  public:
    explicit Supernova(const QString &sName, dms ra, dms dec, const QString &type = QString(),
                       const QString &hostGalaxy = QString(), const QString &date = QString(), float sRedShift = 0.0,
                       float sMag = 99.9);
    /**
     * @return a clone of this object
     * @note See SkyObject::clone()
     */
    Supernova *clone() const override;

    ~Supernova() override = default;

    /** @return the type of the supernova */
    inline QString getType() const { return type; }

    /** @return the host galaxy of the supernova */
    inline QString getHostGalaxy() const { return hostGalaxy; }

    /** @return the date the supernova was observed */
    inline QString getDate() const { return date; }

    /** @return the date the supernova was observed */
    inline float getRedShift() const { return redShift; }

    void initPopupMenu(KSPopupMenu *) override;

  private:
    QString type, hostGalaxy, date;
    float redShift { 0 };
};
