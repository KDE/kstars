/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2024 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include "ui_camera.h"
#include "ui_limits.h"
#include "ui_calibrationoptions.h"
#include "customproperties.h"
#include "rotatorsettings.h"

namespace Ekos
{

class Capture;
class CaptureDeviceAdaptor;
class CaptureProcess;
class CaptureModuleState;
class ScriptsManager;
class FilterManager;

class Camera : public QWidget, public Ui::Camera
{
    Q_OBJECT
    friend class Capture;
public:
    explicit Camera(bool standAlone = false, QWidget *parent = nullptr);

    // ////////////////////////////////////////////////////////////////////
    // device control
    // ////////////////////////////////////////////////////////////////////
    // Gain
    // This sets and gets the custom properties target gain
    // it does not access the ccd gain property
    void setGain(double value);
    double getGain();

    void setOffset(double value);
    double getOffset();

    /**
     * @brief Add new Rotator
     * @param name name of the new rotator
    */
    void setRotator(QString name);

    // ////////////////////////////////////////////////////////////////////
    // Settings
    // ////////////////////////////////////////////////////////////////////
    QVariantMap getAllSettings() const;
    /**
     * @brief syncLimitSettings Update Limits UI from Options
     */
    void syncLimitSettings();

    /**
     * @brief settleSettings Run this function after timeout from debounce timer to update database
     * and emit settingsChanged signal. This is required so we don't overload output.
     */
    void settleSettings();

    // ////////////////////////////////////////////////////////////////////
    // Optical Train handling
    // ////////////////////////////////////////////////////////////////////

    // Utilities for storing stand-alone variables.
    void storeTrainKey(const QString &key, const QStringList &list);
    void storeTrainKeyString(const QString &key, const QString &str);

    // ////////////////////////////////////////////////////////////////////
    // Filter Manager and filters
    // ////////////////////////////////////////////////////////////////////

    QSharedPointer<FilterManager> &filterManager()
    {
        return m_FilterManager;
    }

    /**
     * @brief shortcut for updating the current filter information for the state machine
     */
    void updateCurrentFilterPosition();

    // ////////////////////////////////////////////////////////////////////
    // Devices and process engine
    // ////////////////////////////////////////////////////////////////////

    /**
     * @brief activeJob Shortcut to device adapter
     */
    QSharedPointer<CaptureDeviceAdaptor> devices()
    {
        return m_DeviceAdaptor;
    }
    // TODO: remove this when refactoring Capture --> Camera is finished
    void setDeviceAdaptor(QSharedPointer<CaptureDeviceAdaptor> newDeviceAdaptor)
    {
        m_DeviceAdaptor = newDeviceAdaptor;
    }

    QSharedPointer<CaptureProcess> process()
    {
        return m_captureProcess;
    }
    void setCaptureProcess(QSharedPointer<CaptureProcess> newCaptureProcess)
    {
        m_captureProcess = newCaptureProcess;
    }

    // shortcut for the module state
    QSharedPointer<CaptureModuleState> state() const
    {
        return m_captureModuleState;
    }

    void setCaptureModuleState(QSharedPointer<CaptureModuleState> newCaptureModuleState)
    {
        m_captureModuleState = newCaptureModuleState;
    }

    // Shortcut to the active camera held in the device adaptor
    ISD::Camera *activeCamera();

    /**
     * @brief currentScope Retrieve the scope parameters from the optical train.
     */
    QJsonObject currentScope();

    /**
     * @brief currentReducer Retrieve the reducer parameters from the optical train.
     */
    double currentReducer();

    /**
     * @brief currentAperture Determine the current aperture
     * @return
     */
    double currentAperture();

    void setForceTemperature(bool enabled)
    {
        cameraTemperatureS->setChecked(enabled);
    }

    // ////////////////////////////////////////////////////////////////////
    // sub dialogs
    // ////////////////////////////////////////////////////////////////////

    void openExposureCalculatorDialog();

    // Script Manager
    void handleScriptsManager();

    /**
     * @brief showTemperatureRegulation Toggle temperature regulation dialog which sets temperature ramp and threshold
     */
    void showTemperatureRegulation();

    // ////////////////////////////////////////////////////////////////////
    // Standalone editor
    // ////////////////////////////////////////////////////////////////////
    /**
     *  Gets called when the stand-alone editor gets a show event.
     * Do this initialization here so that if the live capture module was
     * used after startup, it will have set more recent remembered values.
     */
    void onStandAloneShow(QShowEvent* event);

    bool m_standAlone {false};
    void setStandAloneGain(double value);
    void setStandAloneOffset(double value);

    CustomProperties *customPropertiesDialog() const
    {
        return m_customPropertiesDialog.get();
    }


    // ////////////////////////////////////////////////////////////////////
    // Access to properties
    // ////////////////////////////////////////////////////////////////////
    QVariantMap &settings()
    {
        return m_settings;
    }
    void setSettings(const QVariantMap &newSettings)
    {
        m_settings = newSettings;
    }

signals:
    // communication with other modules
    void settingsUpdated(const QVariantMap &settings);
    void newLog(const QString &text);

private slots:
    // ////////////////////////////////////////////////////////////////////
    // slots handling device and module events
    // ////////////////////////////////////////////////////////////////////

    void setVideoStreamEnabled(bool enabled);

    // Cooler
    void setCoolerToggled(bool enabled);

private:

    /**
     * @brief initCamera Initialize all camera settings
     */
    void initCamera();
    // ////////////////////////////////////////////////////////////////////
    // Device updates
    // ////////////////////////////////////////////////////////////////////
    // Rotator
    void updateRotatorAngle(double value);
    void setRotatorReversed(bool toggled);

    /**
     * @brief updateCCDTemperature Update CCD temperature in capture module.
     * @param value Temperature in celcius.
     */
    void updateCCDTemperature(double value);

    // ////////////////////////////////////////////////////////////////////
    // UI controls
    // ////////////////////////////////////////////////////////////////////
    void checkFrameType(int index);

    /**
     * @brief updateStartButtons Update the start and the pause button to new states of capturing
     * @param start start capturing
     * @param pause pause capturing
     */
    void updateStartButtons(bool start, bool pause = false);

    // ////////////////////////////////////////////////////////////////////
    // Sub dialogs
    // ////////////////////////////////////////////////////////////////////
    QSharedPointer<FilterManager> m_FilterManager;
    std::unique_ptr<Ui::Limits> m_LimitsUI;
    QPointer<QDialog> m_LimitsDialog;
    std::unique_ptr<Ui::Calibration> m_CalibrationUI;
    QPointer<QDialog> m_CalibrationDialog;
    QPointer<ScriptsManager> m_scriptsManager;
    QSharedPointer<RotatorSettings> m_RotatorControlPanel;

    // ////////////////////////////////////////////////////////////////////
    // Module logging
    // ////////////////////////////////////////////////////////////////////
    void appendLogText(const QString &)
    {
        void newLog(const QString &text);
    }

    // ////////////////////////////////////////////////////////////////////
    // Attributes
    // ////////////////////////////////////////////////////////////////////
    QVariantMap m_settings;
    std::unique_ptr<CustomProperties> m_customPropertiesDialog;
    QTimer m_DebounceTimer;

    bool m_standAloneUseCcdGain { true};
    bool m_standAloneUseCcdOffset { true};

    QSharedPointer<CaptureDeviceAdaptor> m_DeviceAdaptor;
    QSharedPointer<CaptureProcess> m_captureProcess;
    QSharedPointer<CaptureModuleState> m_captureModuleState;

    // Controls
    double GainSpinSpecialValue { INVALID_VALUE };
    double OffsetSpinSpecialValue { INVALID_VALUE };
};
} // namespace Ekos
