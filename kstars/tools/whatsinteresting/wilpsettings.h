/*
    SPDX-FileCopyrightText: 2013 Samikshan Bairagya <samikshan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_wilpsettings.h"

class KStars;

/**
 * @class WILPSettings
 * @brief User interface for "Light Pollution Settings" page in WI settings dialog
 * This class deals with light pollution settings for WI. The user sets the bortle
 * dark-sky rating for the night sky, and this value is used to calculate one of the
 * parameters that decides the limiting mangnitude.
 *
 * @author Samikshan Bairagya
 */
class WILPSettings : public QFrame, public Ui::WILPSettings
{
    Q_OBJECT

  public:
    explicit WILPSettings(KStars *ks);

  private:
    KStars *m_Ks { nullptr };
};
