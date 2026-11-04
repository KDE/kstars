/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indidustcap.h"
#include "indistd.h"

namespace ISD
{
/**
 * @class LightBox
 * Handles operation of a remotely controlled light box.
 *
 * @author Jasem Mutlaq
 */
class LightBox : public ConcreteDevice
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.INDI.LightBox")
        Q_PROPERTY(uint16_t brightness READ brightness)

    public:
        /**
         * @brief LightStatus enum representing the current state of the light box
         */
        typedef enum
        {
            LIGHT_OFF,
            LIGHT_ON,
            LIGHT_BUSY,
            LIGHT_ERROR,
            LIGHT_UNKNOWN
        } LightStatus;

        explicit LightBox(GenericDevice *parent);

        virtual void processSwitch(INDI::Property prop) override;
        virtual void processNumber(INDI::Property prop) override;

        Q_SCRIPTABLE virtual bool isLightOn();
        LightStatus lightStatus() const
        {
            return m_LightStatus;
        }

        /**
         * @brief brightness Get current brightness level
         * @return Brightness level (0-65535)
         */
        uint16_t brightness() const
        {
            return m_Brightness;
        }

        /**
         * @brief isBrightnessControllable Check whether this light box is able to
         * adjust the brightness of its light source.
         *
         * This is a purely generic capability check: it only tests whether the
         * standard INDI light box intensity property is present and writable.
         * No assumption is made about the driver, the vendor or the model, and no
         * assumption is made about how the device achieves a given brightness.
         * @return True if brightness can be commanded, false otherwise.
         */
        bool isBrightnessControllable() const;

        /**
         * @brief brightnessMin Lowest brightness value accepted by the device.
         * @return Minimum brightness as published by the device, 0 if unavailable.
         */
        uint16_t brightnessMin() const;

        /**
         * @brief brightnessMax Highest brightness value accepted by the device.
         * @return Maximum brightness as published by the device, 0 if unavailable.
         */
        uint16_t brightnessMax() const;

        /**
         * @brief brightnessStep Smallest brightness increment accepted by the device.
         * @return Step size as published by the device, 1 if the device does not
         *         publish a usable step.
         */
        uint16_t brightnessStep() const;

        /**
         * @brief brightnessState Current state of the brightness property.
         *
         * Devices that require time to reach a requested brightness report
         * IPS_BUSY until the new value has been applied.
         * @return the INDI state of the brightness property.
         */
        IPState brightnessState() const;

        static const QString getStatusString(LightStatus status, bool translated = true);

    Q_SIGNALS:
        void newLightStatus(ISD::LightBox::LightStatus status);

        /**
         * @brief newBrightnessStatus Emitted whenever the device reports a brightness update.
         * @param value brightness value reported by the device
         * @param state state of the brightness property. IPS_BUSY means the device is
         *        still settling, IPS_ALERT that the last request failed.
         */
        void newBrightnessStatus(uint16_t value, IPState state);


    public Q_SLOTS:
        /**
         * @brief SetBrightness Set light box brightness levels if dimmable.
         * @param val Value of brightness level.
         * @return True if operation is successful, false otherwise.
         */
        Q_SCRIPTABLE bool setBrightness(uint16_t val);

        /**
         * @brief SetLightEnabled Turn on/off light
         * @param enable true to turn on, false to turn off
         * @return True if operation is successful, false otherwise.
         */
        Q_SCRIPTABLE bool setLightEnabled(bool enable);

    private:
        LightStatus m_LightStatus { LIGHT_OFF };
        uint16_t m_Brightness { 0 };
        static const QList<KLocalizedString> lightStates;
};
}
