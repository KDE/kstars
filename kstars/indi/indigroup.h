/*  INDI Group
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    JM Changelog
    2004-16-1:	Start

 */

#pragma once

#include <indiproperty.h>

#include <QString>
#include <QList>
#include <QScrollArea>
#include <QPointer>

class INDI_P;
class INDI_D;

class QFrame;
class QVBoxLayout;
class QSpacerItem;
class QScrollArea;

/**
 * @class INDI_G
 * INDI_G represents a collection of INDI properties that share a common group. The group is usually represented in the GUI as a separate tab with the device tab.
 *
 * @author Jasem Mutlaq
 */
class INDI_G: public QScrollArea
{
    public:
        INDI_G(INDI_D *idv, const QString &inName);

        bool addProperty(const INDI::Property newProperty);

        bool removeProperty(const QString &name);
        INDI_P *getProperty(const QString &name) const;
        QFrame *getContainer() const
        {
            return m_PropertiesContainer;
        }
        const QString &getName() const
        {
            return name;
        }

        INDI_D *getDevice() const
        {
            return dp;
        }

        QList<INDI_P *> getProperties() const
        {
            return m_PropertiesList;
        }

        int size() const
        {
            return m_PropertiesList.count();
        }

    private:
        // Group name
        QString name;
        // Parent device
        INDI_D *dp {nullptr};
        // Properties container
        QPointer<QFrame> m_PropertiesContainer;
        // Properties layout
        QPointer<QVBoxLayout> m_PropertiesLayout;
        // Vertical spacer
        QSpacerItem *m_VerticalSpacer {nullptr};
        QList<INDI_P *> m_PropertiesList;
        bool m_Dirty { false };
};
