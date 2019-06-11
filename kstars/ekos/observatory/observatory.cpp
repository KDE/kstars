/*  Ekos Observatory Module
    Copyright (C) Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "observatory.h"

#include "ekos_observatory_debug.h"

namespace Ekos
{
Observatory::Observatory()
{
    setupUi(this);

    // status control
    mObservatoryModel = new ObservatoryModel();
    setObseratoryStatusControl(mObservatoryModel->statusControl());
    // update UI for status control
    connect(useDomeCB, &QCheckBox::clicked, this, &Ekos::Observatory::statusControlSettingsChanged);
    connect(useShutterCB, &QCheckBox::clicked, this, &Ekos::Observatory::statusControlSettingsChanged);
    connect(useWeatherCB, &QCheckBox::clicked, this, &Ekos::Observatory::statusControlSettingsChanged);
    connect(mObservatoryModel, &Ekos::ObservatoryModel::newStatus, this, &Ekos::Observatory::observatoryStatusChanged);
    // ready button deactivated
    // connect(statusReadyButton, &QPushButton::clicked, mObservatoryModel, &Ekos::ObservatoryModel::makeReady);
    statusReadyButton->setEnabled(false);

    setDomeModel(new ObservatoryDomeModel());
    setWeatherModel(new ObservatoryWeatherModel());
    statusDefinitionBox->setVisible(true);
    statusDefinitionBox->setEnabled(true);
    // make invisible, since not implemented yet
    angleLabel->setVisible(false);
    domeAngleSpinBox->setVisible(false);
    setDomeAngleButton->setVisible(false);
    weatherWarningSchedulerCB->setVisible(false);
    weatherAlertSchedulerCB->setVisible(false);
}

void Observatory::setObseratoryStatusControl(ObservatoryStatusControl control)
{
    if (mObservatoryModel != nullptr)
    {
        useDomeCB->setChecked(control.useDome);
        useShutterCB->setChecked(control.useShutter);
        useWeatherCB->setChecked(control.useWeather);
    }
}


void Observatory::setDomeModel(ObservatoryDomeModel *model)
{
    mObservatoryModel->setDomeModel(model);
    if (model != nullptr)
    {
        connect(model, &Ekos::ObservatoryDomeModel::ready, this, &Ekos::Observatory::initDome);
        connect(model, &Ekos::ObservatoryDomeModel::disconnected, this, &Ekos::Observatory::shutdownDome);
        connect(model, &Ekos::ObservatoryDomeModel::newStatus, this, &Ekos::Observatory::setDomeStatus);
        connect(model, &Ekos::ObservatoryDomeModel::newShutterStatus, this, &Ekos::Observatory::setShutterStatus);

        connect(weatherWarningShutterCB, &QCheckBox::clicked, this, &Observatory::weatherWarningSettingsChanged);
        connect(weatherWarningDomeCB, &QCheckBox::clicked, this, &Observatory::weatherWarningSettingsChanged);
        connect(weatherWarningDelaySB, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int i) { Q_UNUSED(i); weatherWarningSettingsChanged(); });

        connect(weatherAlertShutterCB, &QCheckBox::clicked, this, &Observatory::weatherAlertSettingsChanged);
        connect(weatherAlertDomeCB, &QCheckBox::clicked, this, &Observatory::weatherAlertSettingsChanged);
        connect(weatherAlertDelaySB, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int i) { Q_UNUSED(i); weatherAlertSettingsChanged(); });
    }
    else
    {
        shutdownDome();
        disconnect(model, &Ekos::ObservatoryDomeModel::newShutterStatus, this, &Ekos::Observatory::setShutterStatus);
        disconnect(model, &Ekos::ObservatoryDomeModel::newStatus, this, &Ekos::Observatory::setDomeStatus);
        disconnect(model, &Ekos::ObservatoryDomeModel::ready, this, &Ekos::Observatory::initDome);
        disconnect(model, &Ekos::ObservatoryDomeModel::disconnected, this, &Ekos::Observatory::shutdownDome);

        disconnect(weatherWarningShutterCB, &QCheckBox::clicked, this, &Observatory::weatherWarningSettingsChanged);
        disconnect(weatherWarningDomeCB, &QCheckBox::clicked, this, &Observatory::weatherWarningSettingsChanged);
        connect(weatherWarningDelaySB, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int i) { Q_UNUSED(i); weatherWarningSettingsChanged(); });

        disconnect(weatherAlertShutterCB, &QCheckBox::clicked, this, &Observatory::weatherAlertSettingsChanged);
        disconnect(weatherAlertDomeCB, &QCheckBox::clicked, this, &Observatory::weatherAlertSettingsChanged);
        connect(weatherAlertDelaySB, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int i) { Q_UNUSED(i); weatherWarningSettingsChanged(); });
    }
}

void Observatory::initDome()
{
    domeBox->setEnabled(true);

    if (getDomeModel() != nullptr)
    {
        connect(getDomeModel(), &Ekos::ObservatoryDomeModel::newLog, this, &Ekos::Observatory::appendLogText);

        if (getDomeModel()->canPark())
        {
            connect(domePark, &QPushButton::clicked, getDomeModel(), &Ekos::ObservatoryDomeModel::park);
            connect(domeUnpark, &QPushButton::clicked, getDomeModel(), &Ekos::ObservatoryDomeModel::unpark);
            domePark->setEnabled(true);
            domeUnpark->setEnabled(true);
        }
        else
        {
            domePark->setEnabled(false);
            domeUnpark->setEnabled(false);
        }

        if (getDomeModel()->hasShutter())
        {
            shutterBox->setVisible(true);
            shutterBox->setEnabled(true);
            connect(shutterOpen, &QPushButton::clicked, getDomeModel(), &Ekos::ObservatoryDomeModel::openShutter);
            connect(shutterClosed, &QPushButton::clicked, getDomeModel(), &Ekos::ObservatoryDomeModel::closeShutter);
            shutterClosed->setEnabled(true);
            shutterOpen->setEnabled(true);
        }
        else
        {
            shutterBox->setVisible(false);
            weatherWarningShutterCB->setVisible(false);
            weatherAlertShutterCB->setVisible(false);
        }

        setDomeStatus(getDomeModel()->status());
        setShutterStatus(getDomeModel()->shutterStatus());
    }

}

void Observatory::shutdownDome()
{
    domeBox->setEnabled(false);
    shutterBox->setEnabled(false);
    shutterBox->setVisible(false);
    domePark->setEnabled(false);
    domeUnpark->setEnabled(false);
    shutterClosed->setEnabled(false);
    shutterOpen->setEnabled(false);
    angleLabel->setEnabled(false);
    domeAngleSpinBox->setEnabled(false);
    setDomeAngleButton->setEnabled(false);

    disconnect(domePark, &QPushButton::clicked, getDomeModel(), &Ekos::ObservatoryDomeModel::park);
    disconnect(domeUnpark, &QPushButton::clicked, getDomeModel(), &Ekos::ObservatoryDomeModel::unpark);
}

void Observatory::setDomeStatus(ISD::Dome::Status status)
{
    switch (status) {
    case ISD::Dome::DOME_ERROR:
        break;
    case ISD::Dome::DOME_IDLE:
        domePark->setChecked(false);
        domePark->setText("PARK");
        domeUnpark->setChecked(true);
        domeUnpark->setText("UNPARKED");
        appendLogText("Dome is unparked.");
        break;
    case ISD::Dome::DOME_MOVING:
        appendLogText("Dome is moving...");
        break;
    case ISD::Dome::DOME_PARKED:
        domePark->setChecked(true);
        domePark->setText("PARKED");
        domeUnpark->setChecked(false);
        domeUnpark->setText("UNPARK");
        appendLogText("Dome is parked.");
        break;
    case ISD::Dome::DOME_PARKING:
        domePark->setText("PARKING");
        domeUnpark->setText("UNPARK");
        appendLogText("Dome is parking...");
        break;
    case ISD::Dome::DOME_UNPARKING:
        domePark->setText("PARK");
        domeUnpark->setText("UNPARKING");
        appendLogText("Dome is unparking...");
        break;
    case ISD::Dome::DOME_TRACKING:
        appendLogText("Dome is tracking.");
        break;
    default:
        break;
    }
}


void Observatory::setShutterStatus(ISD::Dome::ShutterStatus status)
{
    switch (status) {
    case ISD::Dome::SHUTTER_OPEN:
        shutterOpen->setChecked(true);
        shutterClosed->setChecked(false);
        shutterOpen->setText("OPEN");
        shutterClosed->setText("CLOSE");
        appendLogText("Shutter is open.");
        break;
    case ISD::Dome::SHUTTER_OPENING:
        shutterOpen->setText("OPENING");
        shutterClosed->setText("CLOSED");
        appendLogText("Shutter is opening...");
        break;
    case ISD::Dome::SHUTTER_CLOSED:
        shutterOpen->setChecked(false);
        shutterClosed->setChecked(true);
        shutterOpen->setText("OPEN");
        shutterClosed->setText("CLOSED");
        appendLogText("Shutter is closed.");
        break;
    case ISD::Dome::SHUTTER_CLOSING:
        shutterOpen->setText("OPEN");
        shutterClosed->setText("CLOSING");
        appendLogText("Shutter is closing...");
        break;
    default:
        break;
    }
}




void Observatory::setWeatherModel(ObservatoryWeatherModel *model)
{
    mObservatoryModel->setWeatherModel(model);

    if (model != nullptr)
    {
        connect(weatherWarningBox, &QGroupBox::clicked, model, &ObservatoryWeatherModel::setWarningActionsActive);
        connect(weatherAlertBox, &QGroupBox::clicked, model, &ObservatoryWeatherModel::setAlertActionsActive);

        connect(model, &Ekos::ObservatoryWeatherModel::ready, this, &Ekos::Observatory::initWeather);
        connect(model, &Ekos::ObservatoryWeatherModel::newStatus, this, &Ekos::Observatory::setWeatherStatus);
        connect(model, &Ekos::ObservatoryWeatherModel::disconnected, this, &Ekos::Observatory::shutdownWeather);
        connect(&weatherStatusTimer, &QTimer::timeout, [this]()
        {
            weatherWarningStatusLabel->setText(getWeatherModel()->getWarningActionsStatus());
            weatherAlertStatusLabel->setText(getWeatherModel()->getAlertActionsStatus());
        });
    }
    else
    {
        shutdownWeather();
        disconnect(model, &Ekos::ObservatoryWeatherModel::newStatus, this, &Ekos::Observatory::setWeatherStatus);
        disconnect(model, &Ekos::ObservatoryWeatherModel::disconnected, this, &Ekos::Observatory::shutdownWeather);
        disconnect(model, &Ekos::ObservatoryWeatherModel::ready, this, &Ekos::Observatory::initWeather);

        disconnect(weatherWarningBox, &QGroupBox::clicked, model, &ObservatoryWeatherModel::setWarningActionsActive);
        disconnect(weatherAlertBox, &QGroupBox::clicked, model, &ObservatoryWeatherModel::setAlertActionsActive);
    }
}

void Observatory::initWeather()
{
    weatherBox->setEnabled(true);
    weatherLabel->setEnabled(true);
    weatherActionsBox->setVisible(true);
    weatherActionsBox->setEnabled(true);
    weatherWarningBox->setChecked(getWeatherModel()->getWarningActionsActive());
    weatherAlertBox->setChecked(getWeatherModel()->getAlertActionsActive());
    setWeatherStatus(getWeatherModel()->status());
    setWarningActions(getWeatherModel()->getWarningActions());
    setAlertActions(getWeatherModel()->getAlertActions());
    weatherStatusTimer.start(1000);
}

void Observatory::shutdownWeather()
{
    weatherBox->setEnabled(false);
    weatherLabel->setEnabled(false);
    setWeatherStatus(ISD::Weather::WEATHER_IDLE);
    weatherStatusTimer.stop();
}


void Observatory::setWeatherStatus(ISD::Weather::Status status)
{
    std::string label;
    switch (status) {
    case ISD::Weather::WEATHER_OK:
        label = "security-high";
        appendLogText("Weather is OK.");
        break;
    case ISD::Weather::WEATHER_WARNING:
        label = "security-medium";
        appendLogText("Weather WARNING!");
        break;
    case ISD::Weather::WEATHER_ALERT:
        label = "security-low";
        appendLogText("!! WEATHER ALERT !!");
        break;
    default:
        label = "";
        break;
    }

    weatherStatusLabel->setPixmap(QIcon::fromTheme(label.c_str())
                                  .pixmap(QSize(48, 48)));
}


void Observatory::weatherWarningSettingsChanged()
{
    struct WeatherActions actions;
    actions.parkDome = weatherWarningDomeCB->isChecked();
    actions.closeShutter = weatherWarningShutterCB->isChecked();
    actions.delay = weatherWarningDelaySB->value();

    getWeatherModel()->setWarningActions(actions);
}

void Observatory::weatherAlertSettingsChanged()
{
    struct WeatherActions actions;
    actions.parkDome = weatherAlertDomeCB->isChecked();
    actions.closeShutter = weatherAlertShutterCB->isChecked();
    actions.delay = weatherAlertDelaySB->value();

    getWeatherModel()->setAlertActions(actions);
}

void Observatory::observatoryStatusChanged(bool ready)
{
    // statusReadyButton->setEnabled(!ready);
    statusReadyButton->setChecked(ready);
    emit newStatus(ready);
}


void Observatory::setWarningActions(WeatherActions actions)
{
    weatherWarningDomeCB->setChecked(actions.parkDome);
    weatherWarningShutterCB->setChecked(actions.closeShutter);
    weatherWarningDelaySB->setValue(actions.delay);
}


void Observatory::setAlertActions(WeatherActions actions)
{
    weatherAlertDomeCB->setChecked(actions.parkDome);
    weatherAlertShutterCB->setChecked(actions.closeShutter);
    weatherAlertDelaySB->setValue(actions.delay);
}

void Observatory::statusControlSettingsChanged()
{
    ObservatoryStatusControl control;
    control.useDome = useDomeCB->isChecked();
    control.useShutter = useShutterCB->isChecked();
    control.useWeather = useWeatherCB->isChecked();
    mObservatoryModel->setStatusControl(control);
}


void Observatory::appendLogText(const QString &text)
{
    m_LogText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                              QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_OBSERVATORY) << text;

    emit newLog(text);
}

void Observatory::clearLog()
{
    m_LogText.clear();
    emit newLog(QString());
}

}
