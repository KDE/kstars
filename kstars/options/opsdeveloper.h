/*
    SPDX-FileCopyrightText: 2022 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsdeveloper.h"

#include <kconfigdialog.h>

class KStars;

/**
 * @class OpsDeveloper
 *
 * The Developer Tab of the Options window.  In this Tab the user can configure
 * options mostly relevant for developers.
 *
 * @author Hy Murveit
 * @version 1.0
 */
class OpsDeveloper : public QFrame, public Ui::OpsDeveloper
{
    Q_OBJECT

  public:
    explicit OpsDeveloper();

    ~OpsDeveloper() override = default;
};
