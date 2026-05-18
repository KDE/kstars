/*
    SPDX-FileCopyrightText: 2013-2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// Constants used across align implementation files
#define MAXIMUM_SOLVER_ITERATIONS 10
#define CAPTURE_RETRY_DELAY       10000
#define CAPTURE_ROTATOR_DELAY     5000  // After 5 seconds estimated value should be not bad
#define PAH_CUTOFF_FOV            10    // Minimum FOV width in arcminutes for PAH to work
#define ROTATOR_FLIP_TOLERANCE    10    // Tolerance in degrees to detect a flipped PA (~180°)

// Macros for PolarAlignmentAssistant access
#define CHECK_PAH(x) \
    m_PolarAlignmentAssistant && m_PolarAlignmentAssistant->x
#define RUN_PAH(x) \
    if (m_PolarAlignmentAssistant) m_PolarAlignmentAssistant->x

#include <cmath>

namespace Ekos
{
inline bool isEqual(double a, double b)
{
    return std::abs(a - b) < 0.01;
}
}
