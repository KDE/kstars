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
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Focus")

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
        int findFocuser(const QString &trainname, bool addIfNecessary);

        /**
         * @brief showOptions Open the options dialog for the currently selected focuser
         */
        void showOptions();

        /**
         * @brief Run the autofocus process for the currently selected filter
         * @param The reason Autofocus has been called.
         * @param trainname name of the optical train to select the focuser
         */
        void runAutoFocus(const AutofocusReason autofocusReason, const QString &reasonInfo, const QString &trainname);

        /**
         * @brief Reset the camera frame being used by the focuser.
         * @param trainname name of the optical train to select the focuser
         */
        void resetFrame(const QString &trainname);

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
        // DBus interface
        // ////////////////////////////////////////////////////////////////////
        /** DBUS interface function.
         *  Retrieve the status from the focuser of the given optical train.
         */
        Q_SCRIPTABLE Ekos::FocusState status(const QString &trainname);

        /** DBUS interface function.
         * Retrieve the CCD device from the focuser of the given optical train.
         */
        Q_SCRIPTABLE QString camera(const QString &trainname);

        /** DBUS interface function.
         * Retrieve the focuser device name from the focuser of the given optical train.
         */
        Q_SCRIPTABLE QString focuser(const QString &trainname);

        /** DBUS interface function.
         * Retrieve the filter wheel device name from the focuser of the given optical train.
         */
        Q_SCRIPTABLE QString filterWheel(const QString &trainname);

        /** DBUS interface function.
         * select the filter from the available filters.
         * @return Returns true if filter is found and set, false otherwise.
         */
        Q_SCRIPTABLE bool setFilter(const QString &filter, const QString &trainname);
        Q_SCRIPTABLE QString filter(const QString &trainname);

        /** DBUS interface function.
         * @return Returns Half-Flux-Radius in pixels from the focuser of the given optical train..
         */
        Q_SCRIPTABLE double getHFR(const QString &trainname);

        /** DBUS interface function.
         * Set CCD exposure value for the focuser of the given optical train.
         */
        Q_SCRIPTABLE bool setExposure(double value, const QString &trainname);
        Q_SCRIPTABLE double exposure(const QString &trainname);

        /** DBUS interface function.
         * @return Returns True if the focuser of the given optical train supports auto-focusing
         */
        Q_SCRIPTABLE bool canAutoFocus(const QString &trainname);

        /** DBUS interface function.
         * @return Returns True if the focuser of the given optical train uses the full field for focusing
         */
        Q_SCRIPTABLE bool useFullField(const QString &trainname);

        /** DBUS interface function.
         * Set CCD binning for the focuser of the given optical train.
         */
        Q_SCRIPTABLE bool setBinning(int binX, int binY, const QString &trainname);

        /** DBUS interface function.
         * Set Auto Focus options for the focuser of the given optical train. The options must be set before starting
         * the autofocus operation. If no options are set, the options loaded from the user configuration are used.
         * @param enable If true, Ekos will attempt to automatically select the best focus star in the frame.
         * If it fails to select a star, the user will be asked to select a star manually.
         */
        Q_SCRIPTABLE bool setAutoStarEnabled(bool enable, const QString &trainname);

        /** DBUS interface function.
         * Set Auto Focus options for the focuser of the given optical train. The options must be set before
         * starting the autofocus operation. If no options are set, the options loaded from the user configuration are used.
         * @param enable if true, Ekos will capture a subframe around the selected focus star.
         * The subframe size is determined by the boxSize parameter.
         */
        Q_SCRIPTABLE bool setAutoSubFrameEnabled(bool enable, const QString &trainname);

        /** DBUS interface function.
         * Set Autofocus parameters for the focuser of the given optical train.
         * @param boxSize the box size around the focus star in pixels. The boxsize is used to subframe around the focus star.
         * @param stepSize the initial step size to be commanded to the focuser. If the focuser is absolute, the step size is in ticks. For relative focusers, the focuser will be commanded to focus inward for stepSize milliseconds initially.
         * @param maxTravel the maximum steps permitted before the autofocus operation aborts.
         * @param tolerance Measure of how accurate the autofocus algorithm is. If the difference between the current HFR and minimum measured HFR is less than %tolerance after the focuser traversed both ends of the V-curve, then the focusing operation
         * is deemed successful. Otherwise, the focusing operation will continue.
         */
        Q_SCRIPTABLE bool setAutoFocusParameters(const QString &trainname, int boxSize, int stepSize, int maxTravel, double tolerance);

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
        Q_INVOKABLE void clearLog();
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

public slots:
        // ////////////////////////////////////////////////////////////////////
        // DBus interface
        // ////////////////////////////////////////////////////////////////////
        /** DBUS interface function.
         * Start the autofocus operation.
         */
        Q_SCRIPTABLE bool start(const QString &trainname);

        /** DBUS interface function.
         * Capture a focus frame.
         * @param settleTime if > 0 wait for the given time in seconds before starting to capture
         */
        Q_SCRIPTABLE bool capture(const QString &trainname, double settleTime = 0.0);

        /** DBUS interface function.
             * Focus inward
             * @param ms If set, focus inward for ms ticks (Absolute Focuser), or ms milliseconds (Relative Focuser). If not set, it will use the value specified in the options.
             */
        Q_SCRIPTABLE bool focusIn(const QString &trainname, int ms = -1);

        /** DBUS interface function.
             * Focus outward
             * @param ms If set, focus outward for ms ticks (Absolute Focuser), or ms milliseconds (Relative Focuser). If not set, it will use the value specified in the options.
             */
        Q_SCRIPTABLE bool focusOut(const QString &trainname, int ms = -1);

        /**
             * @brief checkFocus Given the minimum required HFR, check focus and calculate HFR. If current HFR exceeds required HFR, start autofocus process, otherwise do nothing.
             * @param requiredHFR Minimum HFR to trigger autofocus process.
             */
        Q_SCRIPTABLE Q_NOREPLY void checkFocus(double requiredHFR, const QString &trainname);



signals:
        Q_SCRIPTABLE void newLog(const QString &text);
        Q_SCRIPTABLE void newStatus(FocusState state, const QString &trainname);
        Q_SCRIPTABLE void newHFR(double hfr, int position, bool inAutofocus, const QString &trainname);
        void suspendGuiding();
        void resumeGuiding();
        void focusAdaptiveComplete(bool success, const QString &trainname);
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
