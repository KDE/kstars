/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    INDI Property Exporter
*/

#include "indipropertyexporter.h"
#include "indilistener.h"
#include "indiproperty.h"
#include "indistd.h"
#include <indidevapi.h>


namespace ISD
{

QJsonArray INDIPropertyExporter::exportDevicesProperties(const QList<QSharedPointer<GenericDevice>> &devices)
{
    QJsonArray devicesArray;

    for (const QSharedPointer<ISD::GenericDevice> &device : devices)
    {
        devicesArray.append(exportDeviceProperties(device));
    }

    return devicesArray;
}

QJsonObject INDIPropertyExporter::exportDeviceProperties(const QSharedPointer<GenericDevice> &device)
{
    QJsonObject deviceObject;
    deviceObject.insert("device_name", device->getDeviceName());
    deviceObject.insert("interface", static_cast<int>(device->getDriverInterface()));

    QJsonArray propertiesArray;
    INDI::BaseDevice::Properties properties = device->getProperties();
    for (const INDI::Property &prop : properties)
    {
        propertiesArray.append(exportProperty(prop));
    }
    deviceObject.insert("properties", propertiesArray);

    return deviceObject;
}

QJsonObject INDIPropertyExporter::exportProperty(INDI::Property prop)
{
    QJsonObject propertyObject;
    QJsonObject tempPropObject; // To hold the output of ISD::*ToJson

    // Use ISD::propertyToJson to populate tempPropObject with all details
    ISD::propertyToJson(prop, tempPropObject, false);

    propertyObject.insert("name", prop.getName());
    propertyObject.insert("type", propertyTypeToString(prop.getType()));
    propertyObject.insert("label", prop.getLabel());
    propertyObject.insert("group", prop.getGroupName());
    propertyObject.insert("permission", permStr(prop.getPermission()));
    propertyObject.insert("state", pstateStr(prop.getState()));

    QJsonArray elementsArray;
    QString elementsKey;

    switch (prop.getType())
    {
        case INDI_NUMBER:
            elementsKey = "numbers";
            break;
        case INDI_SWITCH:
            elementsKey = "switches";
            propertyObject.insert("rule", prop.getSwitch()->getRuleAsString());
            break;
        case INDI_TEXT:
            elementsKey = "texts";
            break;
        case INDI_LIGHT:
            elementsKey = "lights";
            break;
        default:
            break;
    }

    if (!elementsKey.isEmpty() && tempPropObject.contains(elementsKey))
    {
        elementsArray = tempPropObject[elementsKey].toArray();
    }
    propertyObject.insert("elements", elementsArray);

    return propertyObject;
}

QString INDIPropertyExporter::propertyTypeToString(INDI_PROPERTY_TYPE type)
{
    switch (type)
    {
        case INDI_NUMBER:
            return "number";
        case INDI_SWITCH:
            return "switch";
        case INDI_TEXT:
            return "text";
        case INDI_LIGHT:
            return "light";
        case INDI_BLOB:
            return "blob";
        default:
            return "unknown";
    }
}

} // namespace ISD