/*
    SPDX-FileCopyrightText: 2011 Samikshan Bairagya <samikshan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opssupernovae.h"

#include <kconfigdialog.h>

class KStars;

/**
 * @class OpsSupernovae
 *
 * The Supernovae Tab of the Options window.  In this Tab the user can configure
 * supernovae options and select if supernovae should be drawn on the skymap.
 * Also the user is given the option to check for updates on startup. And whether
 * to be alerted on startup.
 *
 * @author Samikshan Bairagya
 * @version 1.0
 */
class OpsSupernovae : public QFrame, public Ui::OpsSupernovae
{
    Q_OBJECT

  public:
    explicit OpsSupernovae();

    ~OpsSupernovae() override = default;
};
