/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Concrete device is a device that implement specific INDI Device Interface (e.g. Telescope).
*/

#pragma once

#include "indistd.h"
#include "indipropertyswitch.h"

#include <QTimer>

namespace ISD
{

/**
 * @brief The ConcreteDevice class
 *
 * A convenience class to wrap common requests to parent generic device.
 */
class ConcreteDevice : public GDInterface
{
        Q_OBJECT
        Q_PROPERTY(QString name READ getDeviceName)
        Q_PROPERTY(bool connected READ isConnected)

    public:
        ConcreteDevice(GenericDevice *parent);
        virtual ~ConcreteDevice() override = default;

        const QString &getDeviceName() const
        {
            return m_Name;
        }

        bool isReady() const
        {
            return m_Parent->isReady();
        }
        bool isConnected() const
        {
            return m_Parent->isConnected();
        }

        void Connect()
        {
            m_Parent->Connect();
        }

        void Disconnect()
        {
            m_Parent->Disconnect();
        }

        uint32_t getDriverInterface()
        {
            return m_Parent->getDriverInterface();
        }

        GenericDevice *genericDevice() const
        {
            return m_Parent;
        }

        INDI::PropertyNumber getNumber(const QString &name) const;
        /** @return Return vector text property given its name */
        INDI::PropertyText   getText(const QString &name) const;
        /** @return Return vector switch property given its name */
        INDI::PropertySwitch getSwitch(const QString &name) const;
        /** @return Return vector light property given its name */
        INDI::PropertyLight  getLight(const QString &name) const;
        /** @return Return vector BLOB property given its name */
        INDI::PropertyBlob   getBLOB(const QString &name) const;

        /** @brief Send new property command to server */
        void sendNewProperty(INDI::Property prop);

        INDI::Property getProperty(const QString &name) const;
        Properties getProperties() const;
        const QSharedPointer<DriverInfo> &getDriverInfo() const;
        bool setConfig(INDIConfig tConfig);
        bool getMinMaxStep(const QString &propName, const QString &elementName, double *min, double *max,
                           double *step) const;
        IPState getState(const QString &propName) const;
        IPerm getPermission(const QString &propName) const;
        QString getMessage(int id) const;

        const QString &getDUBSObjectPath() const
        {
            return m_DBusObjectPath;
        }

        // No implmenetation by default
        virtual void registerProperty(INDI::Property) override {}
        virtual void removeProperty(INDI::Property) override {}
        virtual void updateProperty(INDI::Property prop) override;

        virtual void processSwitch(INDI::Property) override {}
        virtual void processText(INDI::Property) override {}
        virtual void processNumber(INDI::Property) override {}
        virtual void processLight(INDI::Property) override {}
        virtual bool processBLOB(INDI::Property) override
        {
            return false;
        }
        virtual void processMessage(int) override {}

        /**
         * @brief Register all properties.
         */
        void registeProperties();

        /**
         * @brief processProperties Process all properties
         */
        void processProperties();

    signals:
        // Connection
        void Connected();
        void Disconnected();

        // Registeration
        void propertyDefined(INDI::Property prop);
        void propertyUpdated(INDI::Property prop);
        void propertyDeleted(INDI::Property prop);

        void ready();

    protected:
        GenericDevice *m_Parent;
        QString m_Name;
        QScopedPointer<QTimer> m_ReadyTimer;
        QString m_DBusObjectPath;
        static uint8_t getID()
        {
            return m_ID++;
        }
        static uint8_t m_ID;
};

}
