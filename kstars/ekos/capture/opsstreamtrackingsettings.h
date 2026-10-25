/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsstreamtrackingsettings.h"
#include <QWidget>

namespace Ekos
{

/**
 * @class OpsStreamTrackingSettings
 * @brief Capture settings for keeping a planet, the Moon or the Sun centered from the camera's STREAM_TRACKING offsets.
 */
class OpsStreamTrackingSettings : public QWidget, public Ui::OpsStreamTrackingSettings
{
        Q_OBJECT

    public:
        explicit OpsStreamTrackingSettings();
        virtual ~OpsStreamTrackingSettings() override = default;
};
}
