/*
    SPDX-FileCopyrightText: 2023 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_manualpulse.h"
#include "indi/indicommon.h"
#include "skypoint.h"
#include "ekos/guide/guideinterface.h"

#include <QDialog>

namespace Ekos
{
/**
 * @brief The ManualPulse class generates manual pulses for testing purposes to see how much the mount react to the pulses. This could be used for diagnostics purposes.
 */
class ManualPulse : public QDialog, public Ui::ManualPulse
{
    Q_OBJECT
public:
    ManualPulse(QWidget *parent = nullptr);

    /**
     * @brief reset Reset RA & DE offset calculations
     */
    void reset();

    /**
     * @brief setMountCoords calculate new RA/DE offsets
     * @param position updated mount position
     */
    void setMountCoords(const SkyPoint &position);

signals:
    void newSinglePulse(GuideDirection dir, int msecs, CaptureAfterPulses followWithCapture);

private:
    SkyPoint m_LastCoord;
};
}
