/*  Ekos guide drift graph
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
                  2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

namespace GuideGraph
{
// These are used to index driftGraph->Graph().
enum DRIFT_GRAPH_INDICES
{
    G_RA = 0,
    G_DEC = 1,
    G_RA_HIGHLIGHT = 2,
    G_DEC_HIGHLIGHT = 3,
    G_RA_PULSE = 4,
    G_DEC_PULSE = 5,
    G_SNR = 6,
    G_RA_RMS = 7,
    G_DEC_RMS = 8,
    G_RMS = 9
};
}  // namespace
