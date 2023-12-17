/*
    SPDX-FileCopyrightText: 2023 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    Interface defining logging infrastructure for EKOS modules
    SPDX-FileCopyrightText: 2015 Daniel Leu <daniel_mihai.leu@cti.pub.ro>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

namespace Ekos
{
class ModuleLogger
{
public:
    /**
     * @brief appendLogText Append a new line to the logging.
     */
    virtual void appendLogText(const QString &) {};

}; // class ModuleLogger
} // namespace
