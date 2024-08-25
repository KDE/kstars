/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_focusmodule.h"
#include "focus.h"


namespace Ekos
{


class FocusModule : public QWidget, public Ui::FocusManager
{
        Q_OBJECT

public:
        FocusModule();
        ~FocusModule();

        // ////////////////////////////////////////////////////////////////////
        // Access to the focusers
        // ////////////////////////////////////////////////////////////////////

        QSharedPointer<Focus> &focuser(int i);

        QSharedPointer<Focus> mainFocuser();

        /**
         * @brief find the focuser using the given train
         * @param train optical train name
         * @param addIfNecessary if true, add a new camera with the given train, if none uses this train
         * @return index in the list of focusers (@see #camera(int))
         */
        int findFocuser(QString train, bool addIfNecessary);

        /**
         * @brief checkFocus Given the minimum required HFR, check focus and calculate HFR. If current HFR exceeds required HFR, start autofocus process, otherwise do nothing.
         * @param requiredHFR Minimum HFR to trigger autofocus process.
         * @param trainname name of the optical train to select the focuser
         */
        void checkFocus(double requiredHFR, const QString &trainname);

        /**
         * @brief Run the autofocus process for the currently selected filter
         * @param The reason Autofocus has been called.
         * @param trainname name of the optical train to select the focuser
         */
        void runAutoFocus(const AutofocusReason autofocusReason, const QString &reasonInfo, const QString &trainname);

        /**
         * @brief Move the focuser to the initial focus position.
         * @param trainname name of the optical train to select the focuser
         */
        void resetFocuser(const QString &trainname);

        /**
         * @brief Abort the autofocus operation.
         * @param trainname name of the optical train to select the focuser
         */
        void abort(const QString &trainname);

        /**
         * @brief adaptiveFocus moves the focuser between subframes to stay at focus
         * @param trainname name of the optical train to select the focuser
         */
        void adaptiveFocus(const QString &trainname);

        /**
         * @brief React when a meridian flip has been started
         * @param trainname name of the optical train to select the focuser
         */
        void meridianFlipStarted(const QString &trainname);

        // Update Mount module status
        void setMountStatus(ISD::Mount::Status newState);

        // Update Altitude From Mount
        void setMountCoords(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &ha);

        // ////////////////////////////////////////////////////////////////////
        // Device handling
        // ////////////////////////////////////////////////////////////////////
        /**
         * @brief removeDevice Remove device from Focus module
         * @param deviceRemoved pointer to device
         */
        void removeDevice(const QSharedPointer<ISD::GenericDevice> &deviceRemoved);

        /**
         * @brief addTemperatureSource Add temperature source to the list of available sources.
         * @param newSource Device with temperature reporting capability
         * @return True if added successfully, false if duplicate or failed to add.
         */
        bool addTemperatureSource(const QSharedPointer<ISD::GenericDevice> &device);

        /**
         * @brief syncCameraInfo Read current CCD information and update settings accordingly.
         */
        void syncCameraInfo(const char *devicename);

        /**
         * @brief Check all focusers and make sure information is updated accordingly.
         */
        void checkFocusers()
        {
            // iterate over all focusers
            for (auto focuser : m_Focusers)
                focuser->checkFocuser();
        }

        /**
         * @brief Check all CCDs and make sure information is updated accordingly.
         */
        void checkCameras()
        {
            // iterate over all focusers
            for (auto focuser : m_Focusers)
                focuser->checkCamera();
        }


        // ////////////////////////////////////////////////////////////////////
        // Module logging
        // ////////////////////////////////////////////////////////////////////
        void clearLog();
        void appendLogText(const QString &logtext);
        void appendFocusLogText(const QString &lines);

        QStringList logText()
        {
            return m_LogText;
        }
        QString getLogText()
        {
            return m_LogText.join("\n");
        }


signals:
        void newLog(const QString &text);
        void suspendGuiding();
        void resumeGuiding();
        void newStatus(FocusState state, const QString &trainname);
        void focusAdaptiveComplete(bool success, const QString &trainname);
        void newHFR(double hfr, int position, bool inAutofocus, const QString &trainname);
        void newFocusTemperatureDelta(double delta, double absTemperature, const QString &trainname);
        void inSequenceAF(bool requested, const QString &trainname);


private:
        // ////////////////////////////////////////////////////////////////////
        // focuser handling
        // ////////////////////////////////////////////////////////////////////

        /**
         * @brief addFocuser Add a new focuser under focus management control
         * @param trainname name of the optical train
         */
        QSharedPointer<Focus> addFocuser(const QString &trainname = "");

        void initFocuser(QSharedPointer<Focus> newFocuser);

        /**
         * @brief Update the focuser
         * @param ID that holds the focuser
         * @param current focuser is valid
         */
        void updateFocuser(int tabID, bool isValid);

        void closeFocuserTab(int tabIndex);

        void checkCloseFocuserTab(int tabIndex);

        // ////////////////////////////////////////////////////////////////////
        // Helper functions
        // ////////////////////////////////////////////////////////////////////
        /**
         * @brief findUnusedOpticalTrain Find the name of the first optical train that is not used by another tab
         * @return
         */
        const QString findUnusedOpticalTrain();

        // ////////////////////////////////////////////////////////////////////
        // Attributes
        // ////////////////////////////////////////////////////////////////////
        QList<QSharedPointer<Focus>> m_Focusers;

        /// They're generic GDInterface because they could be either ISD::Camera or ISD::FilterWheel or ISD::Weather
        QList<QSharedPointer<ISD::GenericDevice>> m_TemperatureSources;

        // ////////////////////////////////////////////////////////////////////
        // Logging
        // ////////////////////////////////////////////////////////////////////
        QStringList m_LogText;
        QFile m_FocusLogFile;
        QString m_FocusLogFileName;
        bool m_FocusLogEnabled { false };

};

}
