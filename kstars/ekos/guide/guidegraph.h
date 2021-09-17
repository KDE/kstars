/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
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
