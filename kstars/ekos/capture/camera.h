/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include "ui_camera.h"

namespace Ekos
{

class Camera : public QWidget, public Ui::Camera
{
    Q_OBJECT
public:
    explicit Camera(QWidget *parent = nullptr);

};
} // namespace Ekos
