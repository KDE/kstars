/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Concrete device is a device that implement specific INDI Device Interface (e.g. Telescope).
*/

#pragma once

#include "indistd.h"
#include "indipropertyswitch.h"

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

    public:
        ConcreteDevice(GenericDevice *parent);
        virtual ~ConcreteDevice() override = default;

        const QString &getDeviceName() const
        {
            return m_Name;
        }

        bool isConnected() const
        {
            return m_Parent->isConnected();
        }

        GenericDevice *genericDevice() const
        {
            return m_Parent;
        }

        INDI::PropertyView<INumber> *getNumber(const QString &name) const;
        /** @return Return vector text property given its name */
        INDI::PropertyView<IText>   *getText(const QString &name) const;
        /** @return Return vector switch property given its name */
        INDI::PropertyView<ISwitch> *getSwitch(const QString &name) const;
        /** @return Return vector light property given its name */
        INDI::PropertyView<ILight>  *getLight(const QString &name) const;
        /** @return Return vector BLOB property given its name */
        INDI::PropertyView<IBLOB>   *getBLOB(const QString &name) const;

        /** @brief Send new Text command to server */
        void sendNewText(ITextVectorProperty *pp);
        /** @brief Send new Text command to server */
        void sendNewText(const char *deviceName, const char *propertyName, const char *elementName, const char *text);
        /** @brief Send new Number command to server */
        void sendNewNumber(INumberVectorProperty *pp);
        /** @brief Send new Number command to server */
        void sendNewNumber(const char *deviceName, const char *propertyName, const char *elementName, double value);
        /** @brief Send new Switch command to server */
        void sendNewSwitch(ISwitchVectorProperty *pp);
        /** @brief Send new Switch command to server */
        void sendNewSwitch(const char *deviceName, const char *propertyName, const char *elementName);

        INDI::Property getProperty(const QString &name) const;
        Properties getProperties() const;
        DriverInfo *getDriverInfo() const;
        bool setConfig(INDIConfig tConfig);
        bool getMinMaxStep(const QString &propName, const QString &elementName, double *min, double *max,
                           double *step) const;
        IPState getState(const QString &propName) const;
        IPerm getPermission(const QString &propName) const;
        QString getMessage(int id) const;

        // No implmenetation by default
        virtual void registerProperty(INDI::Property prop) override
        {
            Q_UNUSED(prop)
        }
        virtual void removeProperty(const QString &name) override
        {
            Q_UNUSED(name)
        }
        virtual void processSwitch(ISwitchVectorProperty *svp) override
        {
            Q_UNUSED(svp)
        }
        virtual void processText(ITextVectorProperty *tvp) override
        {
            Q_UNUSED(tvp)
        }
        virtual void processNumber(INumberVectorProperty *nvp) override
        {
            Q_UNUSED(nvp)
        }
        virtual void processLight(ILightVectorProperty *lvp) override
        {
            Q_UNUSED(lvp)
        }
        virtual void processBLOB(IBLOB *bp) override
        {
            Q_UNUSED(bp)
        }
        virtual void processMessage(int messageID) override
        {
            Q_UNUSED(messageID)
        }

        /**
         * @brief makeReady Register all properties and emit the ready() signal.
         */
        void makeReady();

    signals:
        // Connection
        void Connected();
        void Disconnected();

        // Registeration
        void propertyDefined(INDI::Property prop);
        void propertyDeleted(const QString &name);

        // Updates
        void switchUpdated(ISwitchVectorProperty *svp);
        void textUpdated(ITextVectorProperty *tvp);
        void numberUpdated(INumberVectorProperty *nvp);
        void lightUpdated(ILightVectorProperty *lvp);
        void BLOBUpdated(IBLOB *bp);

        void ready();

    protected:
        GenericDevice *m_Parent;
        QString m_Name;
};

}
