/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtGlobal>

/**
 * @short  A 32-byte Structure that holds star data
 *
 * @author Akarsh Simha
 * @version 1.0
 */
struct StarData
{
    qint32 RA { 0 };  /**< Raw signed 32-bit RA value. Needs to be multiplied by the scale (1e6) */
    qint32 Dec { 0 }; /**< Raw signed 32-bit DE value. Needs to be multiplied by the scale (1e6) */
    qint32 dRA { 0 };
    qint32 dDec { 0 };
    qint32 parallax { 0 };
    qint32 HD { 0 };  /**< unsigned 32-bit Henry Draper Index. No scaling is required. */
    qint16 mag { 0 }; /**< signed 16-bit raw magnitude. Needs to be divided by the scale (1e2) */
    qint16 bv_index { 0 };
    char spec_type[2];
    char flags { 0 };
    char unused { 0 };
};
