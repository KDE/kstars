/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QMetaEnum>

#pragma once

class collimationoverlaytype : QObject
{
    Q_OBJECT

  public:
        enum Types
        {
            Anchor,
            Ellipse,
            Rectangle,
            Line
        };
        Q_ENUM(Types);
};
