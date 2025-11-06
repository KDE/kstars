
/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    INDI Property Exporter
*/

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QSharedPointer>
#include <QString>

#include "indi/indistd.h"
#include "indi/indicommon.h"
#include "indi/indiproperty.h"
#include <libindi/indidevapi.h>

namespace ISD
{
class INDIPropertyExporter
{
    public:
        static QJsonArray exportDevicesProperties(const QList<QSharedPointer<GenericDevice>> &devices);

    private:
        static QJsonObject exportDeviceProperties(const QSharedPointer<GenericDevice> &device);
        static QJsonObject exportProperty(INDI::Property prop);

        // Helper function for enum to string conversion
        static QString propertyTypeToString(INDI_PROPERTY_TYPE type);
};
} // namespace ISD