/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtGlobal>

/**
 * @short  A 16-byte structure that holds star data for really faint stars.
 *
 * @author Akarsh Simha
 * @version 1.0
 */
struct DeepStarData
{
    qint32 RA { 0 };  /**< Raw signed 32-bit RA value. Needs to be multiplied by the scale (1e6) */
    qint32 Dec { 0 }; /**< Raw signed 32-bit DE value. Needs to be multiplied by the scale (1e6) */
    qint16 dRA { 0 };
    qint16 dDec { 0 };
    qint16 B { 0 };
    qint16 V { 0 };
};
