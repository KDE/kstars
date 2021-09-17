/*
    SPDX-FileCopyrightText: 2002 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QVector>
#include "planetmoons.h"

class KSNumbers;
class KSPlanet;
class KSSun;

/**
  *@class JupiterMoons
  *Implements the four largest moons of Jupiter.
  *See Chapter 43 of "Astronomical Algorithms"by Jean Meeus for details
  *
  *TODO: make the moons SkyObjects, rather than just points.
  *
  *@author Jason Harris
  *@version 1.0
  */
class JupiterMoons : public PlanetMoons
{
    public:
        /**
              *Constructor.  Assign the name of each moon,
              *and initialize their XYZ positions to zero.
              */
        JupiterMoons();

        /**
              *Destructor.  Delete moon objects.
              */
        ~JupiterMoons() override;

        /**
              *@short Find the positions of each Moon, relative to Jupiter.
              *We use an XYZ coordinate system, centered on Jupiter,
              *where the X-axis corresponds to Jupiter's Equator,
              *the Y-Axis is parallel to Jupiter's Poles, and the
              *Z-axis points along the line joining the Earth and
              *Jupiter.  Once the XYZ positions are known, this
              *function also computes the RA,Dec positions of each
              *Moon, and sets the inFront bool variable to indicate
              *whether the Moon is nearer to us than Jupiter or not
              *(this information is used to determine whether the
              *Moon should be drawn on top of Jupiter, or vice versa).
              *
              *See "Astronomical Algorithms" bu Jean Meeus, Chapter 43.
              *
              *@param num pointer to the KSNumbers object describing
              *the date/time at which to find the positions.
              *@param jup pointer to the jupiter object
              *@param sunptr pointer to the Sun object
              */
        void findPosition(const KSNumbers *num, const KSPlanetBase *jup, const KSSun *sunptr) override;
};

