/*
    SPDX-FileCopyrightText: 2026 Pavan <pk6122004@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "mount_guider.h"

#include <QString>
#include <memory>

class MountGuiderFactory
{
    public:
        /**
         * @brief Detect the mount type based on the mount name lookup table.
         */
        static QString detectMountType(const QString &mountName);

        /**
         * @brief Read mount_type field from a weights JSON file.
         */
        static QString readMountTypeFromWeights(const QString &weightsPath);

        /**
         * @brief Create a guider instance for the given mount type string.
         */
        static std::unique_ptr<MountSpecificGuider> create(const QString &mountType);

        /**
         * @brief Create guider from weights file (reads mount_type from JSON).
         */
        static std::unique_ptr<MountSpecificGuider> createFromWeights(const QString &weightsPath);
};
