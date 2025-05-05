/*  Ekos Observatory Module
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kstarsdata.h"

#include "observatory.h"
#include "observatoryadaptor.h"
#include "Options.h"

#include "ekos_observatory_debug.h"

#include <QToolTip>
#include <QButtonGroup>

namespace Ekos
{
Observatory::Observatory()
{
    qRegisterMetaType<ISD::Weather::Status>("ISD::Weather::Status");
    qDBusRegisterMetaType<ISD::Weather::Status>();

    new ObservatoryAdaptor(this);
    QDBusConnection::sessionBus().registerObject("/KStars/Ekos/Observatory", this);

    setupUi(this);

    // status control
    //setObseratoryStatusControl(m_StatusControl);
    // update UI for status control
    connect(useDomeCB, &QCheckBox::clicked, this, &Ekos::Observatory::statusControlSettingsChanged);
    connect(useShutterCB, &QCheckBox::clicked, this, &Ekos::Observatory::statusControlSettingsChanged);
    connect(useWeatherCB, &QCheckBox::clicked, this, &Ekos::Observatory::statusControlSettingsChanged);
    // ready button deactivated
    // connect(statusReadyButton, &QPushButton::clicked, mObservatoryModel, &Ekos::ObservatoryModel::makeReady);
    statusReadyButton->setEnabled(false);

    // weather controls
    connect(weatherWarningShutterCB, &QCheckBox::clicked, this, &Observatory::weatherWarningSettingsChanged);
    connect(weatherWarningDomeCB, &QCheckBox::clicked, this, &Observatory::weatherWarningSettingsChanged);
    connect(weatherWarningDelaySB, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int i)
    {
        Q_UNUSED(i)
        weatherWarningSettingsChanged();
    });

    connect(weatherAlertShutterCB, &QCheckBox::clicked, this, &Observatory::weatherAlertSettingsChanged);
    connect(weatherAlertDomeCB, &QCheckBox::clicked, this, &Observatory::weatherAlertSettingsChanged);
    connect(weatherAlertDelaySB, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int i)
    {
        Q_UNUSED(i)
        weatherAlertSettingsChanged();
    });

    // read the default values
    warningActionsActive = Options::warningActionsActive();
    m_WarningActions.parkDome = Options::weatherWarningCloseDome();
    m_WarningActions.closeShutter = Options::weatherWarningCloseShutter();
    m_WarningActions.delay = Options::weatherWarningDelay();
    alertActionsActive = Options::alertActionsActive();
    m_AlertActions.parkDome = Options::weatherAlertCloseDome();
    m_AlertActions.closeShutter = Options::weatherAlertCloseShutter();
    m_AlertActions.delay = Options::weatherAlertDelay();
    m_autoScaleValues = Options::weatherAutoScaleValues();

    // not implemented yet
    m_WarningActions.stopScheduler = false;
    m_AlertActions.stopScheduler = false;

    warningTimer.setInterval(static_cast<int>(m_WarningActions.delay * 1000));
    warningTimer.setSingleShot(true);
    alertTimer.setInterval(static_cast<int>(m_AlertActions.delay * 1000));
    alertTimer.setSingleShot(true);

    connect(&warningTimer, &QTimer::timeout, [this]()
    {
        execute(m_WarningActions);
    });
    connect(&alertTimer, &QTimer::timeout, [this]()
    {
        execute(m_AlertActions);
    });

    connect(weatherSourceCombo, &QComboBox::currentTextChanged, this, &Observatory::setWeatherSource);

    // initialize the weather sensor data group box
    sensorDataBoxLayout = new QGridLayout();
    sensorData->setLayout(sensorDataBoxLayout);

    initSensorGraphs();

    connect(weatherWarningBox, &QGroupBox::clicked, this, &Observatory::setWarningActionsActive);
    connect(weatherAlertBox, &QGroupBox::clicked, this, &Observatory::setAlertActionsActive);

    connect(clearGraphHistory, &QPushButton::clicked, this, &Observatory::clearSensorDataHistory);
    connect(autoscaleValuesCB, &QCheckBox::clicked, [this](bool checked)
    {
        setAutoScaleValues(checked);
        refreshSensorGraph();
    });
    connect(&weatherStatusTimer, &QTimer::timeout, [this]()
    {
        weatherWarningStatusLabel->setText(getWarningActionsStatus());
        weatherAlertStatusLabel->setText(getAlertActionsStatus());
    });


}

bool Observatory::setDome(ISD::Dome *device)
{
    if (m_Dome == device)
        return false;

    if (m_Dome)
        m_Dome->disconnect(this);

    m_Dome = device;

    domeBox->setEnabled(true);

    // Connect signals
    connectDomeSignals();

    if (m_Dome->canPark())
    {
        domePark->setEnabled(true);
        domeUnpark->setEnabled(true);
    }
    else
    {
        domePark->setEnabled(false);
        domeUnpark->setEnabled(false);
    }

    enableMotionControl(true);

    if (m_Dome->isRolloffRoof())
    {
        SlavingBox->setVisible(false);
        domeAzimuthPosition->setText(i18nc("Not Applicable", "N/A"));
    }
    else
    {
        // initialize the dome motion controls
        domeAzimuthChanged(m_Dome->position());

        // slaving
        showAutoSync(m_Dome->isAutoSync());
        connect(slavingEnableButton, &QPushButton::clicked, this, [this]()
        {
            enableAutoSync(true);
        });
        connect(slavingDisableButton, &QPushButton::clicked, this, [this]()
        {
            enableAutoSync(false);
        });
    }

    // shutter handling
    if (m_Dome->hasShutter())
    {
        shutterBox->setVisible(true);
        shutterBox->setEnabled(true);
        connect(shutterOpen, &QPushButton::clicked, m_Dome, &ISD::Dome::openShutter);
        connect(shutterClosed, &QPushButton::clicked, m_Dome, &ISD::Dome::closeShutter);
        shutterClosed->setEnabled(true);
        shutterOpen->setEnabled(true);
        setShutterStatus(m_Dome->shutterStatus());
        useShutterCB->setVisible(true);
    }
    else
    {
        shutterBox->setVisible(false);
        weatherWarningShutterCB->setVisible(false);
        weatherAlertShutterCB->setVisible(false);
        useShutterCB->setVisible(false);
    }

    // abort button should always be available
    motionAbortButton->setEnabled(true);

    statusDefinitionBox->setVisible(true);
    statusDefinitionBox->setEnabled(true);

    // update the dome parking status
    setDomeParkStatus(m_Dome->parkStatus());

    // enable the UI controls for dome weather actions
    initWeatherActions(m_Dome && m_WeatherSource);

    return true;
}

void Observatory::shutdownDome()
{
    shutterBox->setEnabled(false);
    shutterBox->setVisible(false);
    domePark->setEnabled(false);
    domeUnpark->setEnabled(false);
    shutterClosed->setEnabled(false);
    shutterOpen->setEnabled(false);

    if (m_Dome)
        disconnect(m_Dome);

    // disable the UI controls for dome weather actions
    initWeatherActions(false);
    statusDefinitionBox->setVisible(false);
    domeBox->setEnabled(false);
}

void Observatory::startupDome()
{
    shutterBox->setEnabled(true);
    domePark->setEnabled(true);
    domeUnpark->setEnabled(true);
    shutterClosed->setEnabled(true);
    shutterOpen->setEnabled(true);

    // Reconnect signals if dome is valid
    if (m_Dome)
        connectDomeSignals();

    // enable the UI controls for dome weather actions
    initWeatherActions(m_Dome && m_WeatherSource);
    statusDefinitionBox->setVisible(true);
    domeBox->setEnabled(true);
}

void Observatory::connectDomeSignals()
{
    if (!m_Dome)
        return;

    // Disconnect first to avoid duplicate connections if called multiple times
    disconnect(m_Dome, nullptr, this, nullptr);

    connect(m_Dome, &ISD::Dome::Connected, this, &Ekos::Observatory::startupDome);
    connect(m_Dome, &ISD::Dome::Disconnected, this, &Ekos::Observatory::shutdownDome);

    connect(m_Dome, &ISD::Dome::newStatus, this, &Ekos::Observatory::setDomeStatus);
    connect(m_Dome, &ISD::Dome::newParkStatus, this, &Ekos::Observatory::setDomeParkStatus);
    connect(m_Dome, &ISD::Dome::newShutterStatus, this, &Ekos::Observatory::setShutterStatus);
    connect(m_Dome, &ISD::Dome::positionChanged, this, &Ekos::Observatory::domeAzimuthChanged);
    connect(m_Dome, &ISD::Dome::newAutoSyncStatus, this, &Ekos::Observatory::showAutoSync);

    // motion controls
    connect(motionMoveAbsButton, &QCheckBox::clicked, [this]()
    {
        m_Dome->setPosition(absoluteMotionSB->value());
    });

    connect(motionMoveRelButton, &QCheckBox::clicked, [this]()
    {
        m_Dome->setRelativePosition(relativeMotionSB->value());
    });

    // abort button
    connect(motionAbortButton, &QPushButton::clicked, m_Dome, &ISD::Dome::abort);

    // dome motion buttons
    connect(motionCWButton, &QPushButton::clicked, [ = ](bool checked)
    {
        m_Dome->moveDome(ISD::Dome::DOME_CW, checked ? ISD::Dome::MOTION_START : ISD::Dome::MOTION_STOP);
    });
    connect(motionCCWButton, &QPushButton::clicked, [ = ](bool checked)
    {
        m_Dome->moveDome(ISD::Dome::DOME_CCW, checked ? ISD::Dome::MOTION_START : ISD::Dome::MOTION_STOP);
    });

    // Park/Unpark buttons
    if (m_Dome->canPark())
    {
        connect(domePark, &QPushButton::clicked, m_Dome, &ISD::Dome::park);
        connect(domeUnpark, &QPushButton::clicked, m_Dome, &ISD::Dome::unpark);
    }

    // Slaving buttons (only if not rolloff)
    if (!m_Dome->isRolloffRoof())
    {
        connect(slavingEnableButton, &QPushButton::clicked, this, [this]()
        {
            enableAutoSync(true);
        });
        connect(slavingDisableButton, &QPushButton::clicked, this, [this]()
        {
            enableAutoSync(false);
        });
    }

    // Shutter buttons (only if has shutter)
    if (m_Dome->hasShutter())
    {
        connect(shutterOpen, &QPushButton::clicked, m_Dome, &ISD::Dome::openShutter);
        connect(shutterClosed, &QPushButton::clicked, m_Dome, &ISD::Dome::closeShutter);
    }
}

void Observatory::setDomeStatus(ISD::Dome::Status status)
{
    qCDebug(KSTARS_EKOS_OBSERVATORY) << "Setting dome status to " << status;

    switch (status)
    {
        case ISD::Dome::DOME_ERROR:
            appendLogText(i18n("%1 error. See INDI log for details.",
                               m_Dome->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            motionCWButton->setChecked(false);
            motionCCWButton->setChecked(false);
            break;

        case ISD::Dome::DOME_IDLE:
            motionCWButton->setChecked(false);
            motionCWButton->setEnabled(true);
            motionCCWButton->setChecked(false);
            motionCCWButton->setEnabled(true);

            appendLogText(i18n("%1 is idle.", m_Dome->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            break;

        case ISD::Dome::DOME_MOVING_CW:
            motionCWButton->setChecked(true);
            motionCWButton->setEnabled(false);
            motionCCWButton->setChecked(false);
            motionCCWButton->setEnabled(true);
            if (m_Dome->isRolloffRoof())
            {
                domeAzimuthPosition->setText(i18n("Opening"));
                toggleButtons(domeUnpark, i18n("Unparking"), domePark, i18n("Park"));
                appendLogText(i18n("Rolloff roof opening..."));
            }
            else
            {
                appendLogText(i18n("Dome is moving clockwise..."));
            }
            break;

        case ISD::Dome::DOME_MOVING_CCW:
            motionCWButton->setChecked(false);
            motionCWButton->setEnabled(true);
            motionCCWButton->setChecked(true);
            motionCCWButton->setEnabled(false);
            if (m_Dome->isRolloffRoof())
            {
                domeAzimuthPosition->setText(i18n("Closing"));
                toggleButtons(domePark, i18n("Parking"), domeUnpark, i18n("Unpark"));
                appendLogText(i18n("Rolloff roof is closing..."));
            }
            else
            {
                appendLogText(i18n("Dome is moving counter clockwise..."));
            }
            break;

        case ISD::Dome::DOME_PARKED:
            setDomeParkStatus(ISD::PARK_PARKED);

            appendLogText(i18n("%1 is parked.", m_Dome->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            break;

        case ISD::Dome::DOME_PARKING:
            toggleButtons(domePark, i18n("Parking"), domeUnpark, i18n("Unpark"));
            motionCWButton->setEnabled(true);

            if (m_Dome->isRolloffRoof())
                domeAzimuthPosition->setText(i18n("Closing"));
            else
                enableMotionControl(false);

            motionCWButton->setChecked(false);
            motionCCWButton->setChecked(true);

            appendLogText(i18n("%1 is parking...", m_Dome->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            break;

        case ISD::Dome::DOME_UNPARKING:
            toggleButtons(domeUnpark, i18n("Unparking"), domePark, i18n("Park"));
            motionCCWButton->setEnabled(true);

            if (m_Dome->isRolloffRoof())
                domeAzimuthPosition->setText(i18n("Opening"));
            else
                enableMotionControl(false);

            motionCWButton->setChecked(true);
            motionCCWButton->setChecked(false);

            appendLogText(i18n("%1 is unparking...", m_Dome->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            break;

        case ISD::Dome::DOME_TRACKING:
            enableMotionControl(true);
            motionCWButton->setEnabled(true);
            motionCCWButton->setChecked(true);
            appendLogText(i18n("%1 is tracking.", m_Dome->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            break;
    }
}

void Observatory::setDomeParkStatus(ISD::ParkStatus status)
{
    qCDebug(KSTARS_EKOS_OBSERVATORY) << "Setting dome park status to " << status;
    switch (status)
    {
        case ISD::PARK_UNPARKED:
            activateButton(domePark, i18n("Park"));
            buttonPressed(domeUnpark, i18n("Unparked"));
            motionCWButton->setChecked(false);
            motionCWButton->setEnabled(true);
            motionCCWButton->setChecked(false);

            if (m_Dome->isRolloffRoof())
                domeAzimuthPosition->setText(i18n("Open"));
            else
                enableMotionControl(true);
            break;

        case ISD::PARK_PARKED:
            buttonPressed(domePark, i18n("Parked"));
            activateButton(domeUnpark, i18n("Unpark"));
            motionCWButton->setChecked(false);
            motionCCWButton->setChecked(false);
            motionCCWButton->setEnabled(false);

            if (m_Dome->isRolloffRoof())
                domeAzimuthPosition->setText(i18n("Closed"));
            else
                enableMotionControl(false);
            break;

        default:
            break;
    }
}


void Observatory::setShutterStatus(ISD::Dome::ShutterStatus status)
{
    qCDebug(KSTARS_EKOS_OBSERVATORY) << "Setting shutter status to " << status;

    switch (status)
    {
        case ISD::Dome::SHUTTER_OPEN:
            buttonPressed(shutterOpen, i18n("Opened"));
            activateButton(shutterClosed, i18n("Close"));
            appendLogText(i18n("Shutter is open."));
            break;

        case ISD::Dome::SHUTTER_OPENING:
            toggleButtons(shutterOpen, i18n("Opening"), shutterClosed, i18n("Close"));
            appendLogText(i18n("Shutter is opening..."));
            break;

        case ISD::Dome::SHUTTER_CLOSED:
            buttonPressed(shutterClosed, i18n("Closed"));
            activateButton(shutterOpen, i18n("Open"));
            appendLogText(i18n("Shutter is closed."));
            break;
        case ISD::Dome::SHUTTER_CLOSING:
            toggleButtons(shutterClosed, i18n("Closing"), shutterOpen, i18n("Open"));
            appendLogText(i18n("Shutter is closing..."));
            break;
        default:
            break;
    }
}

void Observatory::enableWeather(bool enable)
{
    weatherBox->setEnabled(enable);
    clearGraphHistory->setVisible(enable);
    clearGraphHistory->setEnabled(enable);
    autoscaleValuesCB->setVisible(enable);
    sensorData->setVisible(enable);
    sensorGraphs->setVisible(enable);
}

void Observatory::clearSensorDataHistory()
{
    std::map<QString, QVector<QCPGraphData>*>::iterator it;

    for (it = sensorGraphData.begin(); it != sensorGraphData.end(); ++it)
    {
        QVector<QCPGraphData>* graphDataVector = it->second;
        if (graphDataVector->size() > 0)
        {
            // we keep only the last one
            QCPGraphData last = graphDataVector->last();
            graphDataVector->clear();
            QDateTime when = QDateTime();
            when.setSecsSinceEpoch(static_cast<uint>(last.key));
            updateSensorGraph(it->first, when, last.value);
        }
    }

    // force an update to the current graph
    if (!selectedSensorID.isEmpty())
        selectedSensorChanged(selectedSensorID);
}

bool Observatory::addWeatherSource(ISD::Weather *device)
{
    // No duplicates
    if (m_WeatherSources.contains(device))
        return false;

    // Disconnect all
    for (auto &oneWeatherSource : m_WeatherSources)
        oneWeatherSource->disconnect(this);

    // If default source is empty or matches current device then let's set the current weather source to it
    auto defaultSource = Options::defaultObservatoryWeatherSource();
    if (m_WeatherSource == nullptr || defaultSource.isEmpty() || device->getDeviceName() == defaultSource)
        m_WeatherSource = device;
    m_WeatherSources.append(device);

    weatherSourceCombo->blockSignals(true);
    weatherSourceCombo->clear();
    for (auto &oneSource : m_WeatherSources)
        weatherSourceCombo->addItem(oneSource->getDeviceName());
    if (defaultSource.isEmpty())
        Options::setDefaultObservatoryWeatherSource(weatherSourceCombo->currentText());
    else
        weatherSourceCombo->setCurrentText(defaultSource);
    weatherSourceCombo->blockSignals(false);

    initWeather();

    // make invisible, since not implemented yet
    weatherWarningSchedulerCB->setVisible(false);
    weatherAlertSchedulerCB->setVisible(false);

    return true;
}

void Observatory::enableMotionControl(bool enabled)
{
    MotionBox->setEnabled(enabled);

    // absolute motion controls
    if (m_Dome->canAbsoluteMove())
    {
        motionMoveAbsButton->setEnabled(enabled);
        absoluteMotionSB->setEnabled(enabled);
    }
    else
    {
        motionMoveAbsButton->setEnabled(false);
        absoluteMotionSB->setEnabled(false);
    }

    // relative motion controls
    if (m_Dome->canRelativeMove())
    {
        motionMoveRelButton->setEnabled(enabled);
        relativeMotionSB->setEnabled(enabled);
        motionCWButton->setEnabled(enabled);
        motionCCWButton->setEnabled(enabled);
    }
    else
    {
        motionMoveRelButton->setEnabled(false);
        relativeMotionSB->setEnabled(false);
        motionCWButton->setEnabled(false);
        motionCCWButton->setEnabled(false);
    }

    // special case for rolloff roofs
    if (m_Dome->isRolloffRoof())
    {
        motionCWButton->setText(i18n("Open"));
        motionCWButton->setToolTip(QString());
        motionCCWButton->setText(i18n("Close"));
        motionCCWButton->setToolTip(QString());
        motionCWButton->setEnabled(enabled);
        motionCCWButton->setEnabled(enabled);
        motionMoveAbsButton->setVisible(false);
        motionMoveRelButton->setVisible(false);
        absoluteMotionSB->setVisible(false);
        relativeMotionSB->setVisible(false);
    }
}

void Observatory::enableAutoSync(bool enabled)
{
    if (m_Dome == nullptr)
        showAutoSync(false);
    else
    {
        m_Dome->setAutoSync(enabled);
        showAutoSync(enabled);
    }
}

void Observatory::showAutoSync(bool enabled)
{
    slavingEnableButton->setChecked(enabled);
    slavingDisableButton->setChecked(! enabled);
}

void Observatory::initWeather()
{
    enableWeather(true);
    weatherBox->setEnabled(true);
    setWeatherSource(weatherSourceCombo->currentText());

    connect(m_WeatherSource, &ISD::Weather::newStatus, this, &Ekos::Observatory::setWeatherStatus);
    connect(m_WeatherSource, &ISD::Weather::newData, this, &Ekos::Observatory::newWeatherData);
    connect(m_WeatherSource, &ISD::Weather::newData, this, &Ekos::Observatory::updateSensorData);
    connect(m_WeatherSource, &ISD::Weather::Disconnected, this, &Ekos::Observatory::shutdownWeather);

    autoscaleValuesCB->setChecked(autoScaleValues());
    weatherWarningBox->setChecked(getWarningActionsActive());
    weatherAlertBox->setChecked(getAlertActionsActive());
    setWeatherStatus(m_WeatherSource->status());
    setWarningActions(getWarningActions());
    setAlertActions(getAlertActions());
    initWeatherActions(true);
    weatherStatusTimer.start(1000);
}

void Observatory::shutdownWeather()
{
    weatherStatusTimer.stop();
    setWeatherStatus(ISD::Weather::WEATHER_IDLE);
    enableWeather(false);
    // disable the UI controls for weather actions
    initWeatherActions(false);
}

void Observatory::initWeatherActions(bool enabled)
{
    if (enabled && m_Dome != nullptr && m_Dome->isConnected())
    {
        // make the entire box visible
        weatherActionsBox->setVisible(true);
        weatherActionsBox->setEnabled(true);

        // enable warning and alert action control
        weatherAlertDomeCB->setEnabled(true);
        weatherWarningDomeCB->setEnabled(true);

        // only domes with shutters need shutter action controls
        if (m_Dome->hasShutter())
        {
            weatherAlertShutterCB->setEnabled(true);
            weatherWarningShutterCB->setEnabled(true);
        }
        else
        {
            weatherAlertShutterCB->setEnabled(false);
            weatherWarningShutterCB->setEnabled(false);
        }
    }
    else
    {
        weatherActionsBox->setVisible(false);
        weatherActionsBox->setEnabled(false);
    }
}


void Observatory::updateSensorGraph(const QString &sensor_label, QDateTime now, double value)
{
    // we assume that labels are unique and use the full label as identifier
    QString id = sensor_label;

    // lazy instantiation of the sensor data storage
    if (sensorGraphData[id] == nullptr)
    {
        sensorGraphData[id] = new QVector<QCPGraphData>();
        sensorRanges[id] = value > 0 ? 1 : (value < 0 ? -1 : 0);
    }

    // store the data
    sensorGraphData[id]->append(QCPGraphData(static_cast<double>(now.toSecsSinceEpoch()), value));

    // add data for the graphs we display
    if (selectedSensorID == id)
    {
        // display first point in scattered style
        if (sensorGraphData[id]->size() == 1)
            sensorGraphs->graph()->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QPen(Qt::black, 0), QBrush(Qt::green),
                                                   5));
        else
            sensorGraphs->graph()->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssNone));

        // display data point
        sensorGraphs->graph()->addData(sensorGraphData[id]->last().key, sensorGraphData[id]->last().value);

        // determine where the x axis is relatively to the value ranges
        if ((sensorRanges[id] > 0 && value < 0) || (sensorRanges[id] < 0 && value > 0))
            sensorRanges[id] = 0;

        refreshSensorGraph();
    }
}

void Observatory::updateSensorData(const QJsonArray &data)
{
    if (data.empty())
        return;

    QDateTime now = KStarsData::Instance()->lt();

    for (const auto &oneEntry : std::as_const(data))
    {
        auto label = oneEntry[QString("label")].toString();
        auto value = oneEntry[QString("value")].toDouble();

        auto id = oneEntry[QString("label")].toString();

        if (sensorDataWidgets[id] == nullptr)
        {
            QPushButton* labelWidget = new QPushButton(label, this);
            labelWidget->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
            labelWidget->setCheckable(true);
            labelWidget->setStyleSheet("QPushButton:checked\n{\nbackground-color: maroon;\nborder: 1px outset;\nfont-weight:bold;\n}");
            // we need the object name since the label may contain '&' for keyboard shortcuts
            labelWidget->setObjectName(label);

            QLineEdit* valueWidget = new QLineEdit(QString().setNum(value, 'f', 2), this);
            // fix width to enable stretching of the graph
            valueWidget->setMinimumWidth(96);
            valueWidget->setMaximumWidth(96);
            valueWidget->setReadOnly(true);
            valueWidget->setAlignment(Qt::AlignRight);

            sensorDataWidgets[id] = new QPair<QAbstractButton*, QLineEdit*>(labelWidget, valueWidget);

            sensorDataBoxLayout->addWidget(labelWidget, sensorDataBoxLayout->rowCount(), 0);
            sensorDataBoxLayout->addWidget(valueWidget, sensorDataBoxLayout->rowCount() - 1, 1);

            // initial graph selection
            if (!selectedSensorID.isEmpty() && id.indexOf('(') > 0 && id.indexOf('(') < id.indexOf(')'))
            {
                selectedSensorID = id;
                labelWidget->setChecked(true);
            }

            sensorDataNamesGroup->addButton(labelWidget);
        }
        else
        {
            sensorDataWidgets[id]->first->setText(label);
            sensorDataWidgets[id]->second->setText(QString().setNum(value, 'f', 2));
        }

        // store sensor data unit if necessary
        updateSensorGraph(label, now, value);
    }
}

void Observatory::mouseOverLine(QMouseEvent *event)
{
    double key = sensorGraphs->xAxis->pixelToCoord(event->localPos().x());
    QCPGraph *graph = qobject_cast<QCPGraph *>(sensorGraphs->plottableAt(event->pos(), false));

    if (graph)
    {
        int index = sensorGraphs->graph(0)->findBegin(key);
        double value   = sensorGraphs->graph(0)->dataMainValue(index);
        QDateTime when = QDateTime::fromSecsSinceEpoch(sensorGraphs->graph(0)->dataMainKey(index));

        QToolTip::showText(
            event->globalPos(),
            i18n("%1 = %2 @ %3", selectedSensorID, value, when.toString("hh:mm")));
    }
    else
    {
        QToolTip::hideText();
    }
}


void Observatory::refreshSensorGraph()
{
    sensorGraphs->rescaleAxes();

    // restrict the y-Axis to the values range
    if (autoScaleValues() == false)
    {
        if (sensorRanges[selectedSensorID] > 0)
            sensorGraphs->yAxis->setRangeLower(0);
        else if (sensorRanges[selectedSensorID] < 0)
            sensorGraphs->yAxis->setRangeUpper(0);
    }

    sensorGraphs->replot();
}

void Observatory::selectedSensorChanged(QString id)
{
    QVector<QCPGraphData> *data = sensorGraphData[id];

    if (data != nullptr)
    {
        // copy the graph data to the graph container
        QCPGraphDataContainer *container = new QCPGraphDataContainer();
        for (QVector<QCPGraphData>::iterator it = data->begin(); it != data->end(); ++it)
            container->add(QCPGraphData(it->key, it->value));

        sensorGraphs->graph()->setData(QSharedPointer<QCPGraphDataContainer>(container));
        selectedSensorID = id;
        refreshSensorGraph();
    }
}

void Observatory::setWeatherStatus(ISD::Weather::Status status)
{
    QString label;
    if (status != m_WeatherStatus)
    {
        switch (status)
        {
            case ISD::Weather::WEATHER_OK:
                label = "security-high";
                appendLogText(i18n("Weather is OK"));
                warningTimer.stop();
                alertTimer.stop();
                break;
            case ISD::Weather::WEATHER_WARNING:
                label = "security-medium";
                appendLogText(i18n("Weather Warning"));
                alertTimer.stop();
                startWarningTimer();
                break;
            case ISD::Weather::WEATHER_ALERT:
                label = "security-low";
                appendLogText(i18n("Weather Alert"));
                warningTimer.stop();
                startAlertTimer();
                break;
            default:
                label = QString();
                break;
        }

        weatherStatusLabel->setPixmap(QIcon::fromTheme(label).pixmap(QSize(28, 28)));
        m_WeatherStatus = status;
        emit newStatus(m_WeatherStatus);
    }

    // update weather sensor data
    if (m_WeatherSource)
        updateSensorData(m_WeatherSource->data());

}

void Observatory::initSensorGraphs()
{
    // set some pens, brushes and backgrounds:
    sensorGraphs->xAxis->setBasePen(QPen(Qt::white, 1));
    sensorGraphs->yAxis->setBasePen(QPen(Qt::white, 1));
    sensorGraphs->xAxis->setTickPen(QPen(Qt::white, 1));
    sensorGraphs->yAxis->setTickPen(QPen(Qt::white, 1));
    sensorGraphs->xAxis->setSubTickPen(QPen(Qt::white, 1));
    sensorGraphs->yAxis->setSubTickPen(QPen(Qt::white, 1));
    sensorGraphs->xAxis->setTickLabelColor(Qt::white);
    sensorGraphs->yAxis->setTickLabelColor(Qt::white);
    sensorGraphs->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    sensorGraphs->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    sensorGraphs->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    sensorGraphs->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    sensorGraphs->xAxis->grid()->setSubGridVisible(true);
    sensorGraphs->yAxis->grid()->setSubGridVisible(true);
    sensorGraphs->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    sensorGraphs->yAxis->grid()->setZeroLinePen(Qt::NoPen);
    sensorGraphs->xAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    sensorGraphs->yAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    QLinearGradient plotGradient;
    plotGradient.setStart(0, 0);
    plotGradient.setFinalStop(0, 350);
    plotGradient.setColorAt(0, QColor(80, 80, 80));
    plotGradient.setColorAt(1, QColor(50, 50, 50));
    sensorGraphs->setBackground(plotGradient);
    QLinearGradient axisRectGradient;
    axisRectGradient.setStart(0, 0);
    axisRectGradient.setFinalStop(0, 350);
    axisRectGradient.setColorAt(0, QColor(80, 80, 80));
    axisRectGradient.setColorAt(1, QColor(30, 30, 30));
    sensorGraphs->axisRect()->setBackground(axisRectGradient);

    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("hh:mm");
    dateTicker->setTickCount(2);
    sensorGraphs->xAxis->setTicker(dateTicker);

    // allow dragging in all directions
    sensorGraphs->setInteraction(QCP::iRangeDrag, true);
    sensorGraphs->setInteraction(QCP::iRangeZoom);

    // create the universal graph
    QCPGraph *graph = sensorGraphs->addGraph();
    graph->setPen(QPen(Qt::darkGreen, 2));
    graph->setBrush(QColor(10, 100, 50, 70));

    // ensure that the 0-line is visible
    sensorGraphs->yAxis->setRangeLower(0);

    sensorDataNamesGroup = new QButtonGroup();
    // enable changing the displayed sensor
    connect(sensorDataNamesGroup, static_cast<void (QButtonGroup::*)(QAbstractButton*)>(&QButtonGroup::buttonClicked), [this](
                QAbstractButton * button)
    {
        selectedSensorChanged(button->objectName());
    });

    // show current temperature below the mouse
    connect(sensorGraphs, &QCustomPlot::mouseMove, this, &Ekos::Observatory::mouseOverLine);

}

void Observatory::weatherWarningSettingsChanged()
{
    struct WeatherActions actions;
    actions.parkDome = weatherWarningDomeCB->isChecked();
    actions.closeShutter = weatherWarningShutterCB->isChecked();
    // Fixme: not implemented yet
    actions.stopScheduler = false;
    actions.delay = static_cast<unsigned int>(weatherWarningDelaySB->value());

    setWarningActions(actions);
}

void Observatory::weatherAlertSettingsChanged()
{
    struct WeatherActions actions;
    actions.parkDome = weatherAlertDomeCB->isChecked();
    actions.closeShutter = weatherAlertShutterCB->isChecked();
    // Fixme: not implemented yet
    actions.stopScheduler = false;
    actions.delay = static_cast<unsigned int>(weatherAlertDelaySB->value());

    setAlertActions(actions);
}

void Observatory::domeAzimuthChanged(double position)
{
    domeAzimuthPosition->setText(QString::number(position, 'f', 2));
}

void Observatory::setWarningActions(WeatherActions actions)
{
    if (m_Dome != nullptr)
        weatherWarningDomeCB->setChecked(actions.parkDome);
    else
        weatherWarningDomeCB->setChecked(actions.parkDome);

    if (m_Dome != nullptr && m_Dome->hasShutter())
        weatherWarningShutterCB->setChecked(actions.closeShutter);
    else
        weatherWarningShutterCB->setChecked(actions.closeShutter);

    weatherWarningDelaySB->setValue(static_cast<int>(actions.delay));

    if (m_WeatherSource)
    {
        m_WarningActions = actions;
        Options::setWeatherWarningCloseDome(actions.parkDome);
        Options::setWeatherWarningCloseShutter(actions.closeShutter);
        Options::setWeatherWarningDelay(actions.delay);
        if (!warningTimer.isActive())
            warningTimer.setInterval(static_cast<int>(actions.delay * 1000));

        if (m_WeatherSource->status() == ISD::Weather::WEATHER_WARNING)
            startWarningTimer();
    }
}

void Observatory::setAlertActions(WeatherActions actions)
{
    if (m_Dome != nullptr)
        weatherAlertDomeCB->setChecked(actions.parkDome);
    else
        weatherAlertDomeCB->setChecked(false);

    if (m_Dome != nullptr && m_Dome->hasShutter())
        weatherAlertShutterCB->setChecked(actions.closeShutter);
    else
        weatherAlertShutterCB->setChecked(false);

    weatherAlertDelaySB->setValue(static_cast<int>(actions.delay));

    if (m_WeatherSource)
    {
        m_AlertActions = actions;
        Options::setWeatherAlertCloseDome(actions.parkDome);
        Options::setWeatherAlertCloseShutter(actions.closeShutter);
        Options::setWeatherAlertDelay(actions.delay);
        if (!alertTimer.isActive())
            alertTimer.setInterval(static_cast<int>(actions.delay * 1000));

        if (m_WeatherSource->status() == ISD::Weather::WEATHER_ALERT)
            startAlertTimer();
    }
}

void Observatory::toggleButtons(QPushButton *buttonPressed, QString titlePressed, QPushButton *buttonCounterpart,
                                QString titleCounterpart)
{
    buttonPressed->setEnabled(false);
    buttonPressed->setText(titlePressed);

    buttonCounterpart->setEnabled(true);
    buttonCounterpart->setChecked(false);
    buttonCounterpart->setCheckable(false);
    buttonCounterpart->setText(titleCounterpart);
}

void Observatory::activateButton(QPushButton *button, QString title)
{
    button->setEnabled(true);
    button->setCheckable(false);
    button->setText(title);
}

void Observatory::buttonPressed(QPushButton *button, QString title)
{
    button->setEnabled(false);
    button->setCheckable(true);
    button->setChecked(true);
    button->setText(title);

}

void Observatory::statusControlSettingsChanged()
{
    ObservatoryStatusControl control;
    control.useDome = useDomeCB->isChecked();
    control.useShutter = useShutterCB->isChecked();
    control.useWeather = useWeatherCB->isChecked();
    setStatusControl(control);
}


void Observatory::appendLogText(const QString &text)
{
    m_LogText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                              KStarsData::Instance()->lt().toString("yyyy-MM-ddThh:mm:ss"), text));

    qCInfo(KSTARS_EKOS_OBSERVATORY) << text;

    emit newLog(text);
}

void Observatory::clearLog()
{
    m_LogText.clear();
    emit newLog(QString());
}

void Observatory::setWarningActionsActive(bool active)
{
    warningActionsActive = active;
    Options::setWarningActionsActive(active);

    // stop warning actions if deactivated
    if (!active && warningTimer.isActive())
        warningTimer.stop();
    // start warning timer if activated
    else if (m_WeatherSource->status() == ISD::Weather::WEATHER_WARNING)
        startWarningTimer();
}

void Observatory::startWarningTimer()
{
    if (warningActionsActive && (m_WarningActions.parkDome || m_WarningActions.closeShutter || m_WarningActions.stopScheduler))
    {
        if (!warningTimer.isActive())
            warningTimer.start();
    }
    else if (warningTimer.isActive())
        warningTimer.stop();
}

void Observatory::setAlertActionsActive(bool active)
{
    alertActionsActive = active;
    Options::setAlertActionsActive(active);

    // stop alert actions if deactivated
    if (!active && alertTimer.isActive())
        alertTimer.stop();
    // start alert timer if activated
    else if (m_WeatherSource->status() == ISD::Weather::WEATHER_ALERT)
        startAlertTimer();
}

void Observatory::setAutoScaleValues(bool value)
{
    m_autoScaleValues = value;
    Options::setWeatherAutoScaleValues(value);
}

void Observatory::startAlertTimer()
{
    if (alertActionsActive && (m_AlertActions.parkDome || m_AlertActions.closeShutter || m_AlertActions.stopScheduler))
    {
        if (!alertTimer.isActive())
            alertTimer.start();
    }
    else if (alertTimer.isActive())
        alertTimer.stop();
}

QString Observatory::getWarningActionsStatus()
{
    if (warningTimer.isActive())
    {
        int remaining = warningTimer.remainingTime() / 1000;
        return i18np("%1 second remaining", "%1 seconds remaining", remaining);
    }

    return i18n("Status: inactive");
}

QString Observatory::getAlertActionsStatus()
{
    if (alertTimer.isActive())
    {
        int remaining = alertTimer.remainingTime() / 1000;
        return i18np("%1 second remaining", "%1 seconds remaining", remaining);
    }

    return i18n("Status: inactive");
}

void Observatory::execute(WeatherActions actions)
{
    if (!m_Dome)
        return;

    if (m_Dome->hasShutter() && actions.closeShutter)
        m_Dome->closeShutter();
    if (actions.parkDome)
        m_Dome->park();
}


void Observatory::setStatusControl(ObservatoryStatusControl control)
{
    m_StatusControl = control;
    Options::setObservatoryStatusUseDome(control.useDome);
    Options::setObservatoryStatusUseShutter(control.useShutter);
    Options::setObservatoryStatusUseWeather(control.useWeather);
}

void Observatory::removeDevice(const QSharedPointer<ISD::GenericDevice> &deviceRemoved)
{
    auto name = deviceRemoved->getDeviceName();

    // Check in Dome

    if (m_Dome && m_Dome->getDeviceName() == name)
    {
        m_Dome->disconnect(this);
        m_Dome = nullptr;
        shutdownDome();
    }

    if (m_WeatherSource && m_WeatherSource->getDeviceName() == name)
    {
        m_WeatherSource->disconnect(this);
        m_WeatherSource = nullptr;
        shutdownWeather();
    }

    // Check in Weather Sources.
    for (auto &oneSource : m_WeatherSources)
    {
        if (oneSource->getDeviceName() == name)
        {
            m_WeatherSources.removeAll(oneSource);
            weatherSourceCombo->removeItem(weatherSourceCombo->findText(name));
        }
    }
}

void Observatory::setWeatherSource(const QString &name)
{
    Options::setDefaultObservatoryWeatherSource(name);
    for (auto &oneWeatherSource : m_WeatherSources)
    {
        if (oneWeatherSource->getDeviceName() == name)
        {
            // Same source, ignore and return
            if (m_WeatherSource == oneWeatherSource)
                return;

            if (m_WeatherSource)
                m_WeatherSource->disconnect(this);

            m_WeatherSource = oneWeatherSource;

            // Must delete all the Buttons and Line-edits
            for (auto &oneWidget : sensorDataWidgets)
            {
                auto pair = oneWidget.second;
                sensorDataBoxLayout->removeWidget(pair->first);
                sensorDataBoxLayout->removeWidget(pair->second);
                pair->first->deleteLater();
                pair->second->deleteLater();
            }
            sensorDataWidgets.clear();
            initWeather();
            return;
        }
    }
}

}
