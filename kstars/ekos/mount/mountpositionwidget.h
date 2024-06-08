/*  Widget to display the mount position.
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_mountpositionwidget.h"

#include "indi/indistd.h"
#include "indi/indimount.h"

#include <QObject>
#include <QWidget>

namespace Ekos
{
class MountPositionWidget : public QWidget, public Ui::MountPositionWidget
{
    Q_OBJECT
public:
    explicit MountPositionWidget(QWidget *parent = nullptr);

    /**
         * @brief updateTelescopeCoords Update the coordinates
         * @param position latest coordinates the mount reports it is pointing to
         * @param ha hour angle of the latest coordinates
         */
    void updateTelescopeCoords(const SkyPoint &position, const dms &ha);

    // J2000 flag
    bool isJ2000Enabled();
    void setJ2000Enabled(bool enabled);

signals:
    void J2000Enabled(bool enabled);

};

} // namespace
