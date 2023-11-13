/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

// This header file includes constants used by several Aberration Inspector classes

namespace Ekos
{

// tileID defines the 9 tiles in the mosaic
typedef enum
{
    TILE_TL = 0,
    TILE_TM,
    TILE_TR,
    TILE_CL,
    TILE_CM,
    TILE_CR,
    TILE_BL,
    TILE_BM,
    TILE_BR,
    // Add a max for array indexing
    NUM_TILES
} tileID;

// Tile names and colours
static const QString TILE_NAME[NUM_TILES] = {"TL", "T", "TR", "L", "C", "R", "BL", "B", "BR"};
static const QString TILE_LONGNAME[NUM_TILES] = {"Top Left", "Top", "Top Right", "Left", "Centre", "Right", "Bottom Left", "Bottom", "Bottom Right"};
static const QString TILE_COLOUR[NUM_TILES] = {"red", "cyan", "green", "magenta", "blue", "grey", "darkGreen", "darkCyan", "darkRed"};

// Debug flag
// false - production mode
// true - used for production support to enable table widget to be editable to simulate other user's data
constexpr bool ABINS_DEBUG { false };
}
