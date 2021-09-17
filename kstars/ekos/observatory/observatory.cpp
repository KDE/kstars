/*  Ekos Observatory Module
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@t-online.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kstarsdata.h"

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
        connect(model, &Ekos::ObservatoryDomeModel::newParkStatus, this, &Ekos::Observatory::setDomeParkStatus);
        connect(model, &Ekos::ObservatoryDomeModel::newShutterStatus, this, &Ekos::Observatory::setShutterStatus);
        connect(model, &Ekos::ObservatoryDomeModel::azimuthPositionChanged, this, &Ekos::Observatory::domeAzimuthChanged);
        connect(model, &Ekos::ObservatoryDomeModel::newAutoSyncStatus, this, &Ekos::Observatory::showAutoSync);

        // motion controls
        connect(motionMoveAbsButton, &QCheckBox::clicked, [this]()
        {
            mObservatoryModel->getDomeModel()->setAzimuthPosition(absoluteMotionSB->value());
        });

        connect(motionMoveRelButton, &QCheckBox::clicked, [this]()
        {
            mObservatoryModel->getDomeModel()->setRelativePosition(relativeMotionSB->value());
        });

        // abort button
        connect(motionAbortButton, &QPushButton::clicked, model, &ObservatoryDomeModel::abort);

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
    }
    else
    {
        shutdownDome();


        disconnect(weatherWarningShutterCB, &QCheckBox::clicked, this, &Observatory::weatherWarningSettingsChanged);
        disconnect(weatherWarningDomeCB, &QCheckBox::clicked, this, &Observatory::weatherWarningSettingsChanged);
        connect(weatherWarningDelaySB, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int i)
        {
            Q_UNUSED(i)
            weatherWarningSettingsChanged();
        });

        disconnect(weatherAlertShutterCB, &QCheckBox::clicked, this, &Observatory::weatherAlertSettingsChanged);
        disconnect(weatherAlertDomeCB, &QCheckBox::clicked, this, &Observatory::weatherAlertSettingsChanged);
        connect(weatherAlertDelaySB, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int i)
        {
            Q_UNUSED(i)
            weatherWarningSettingsChanged();
        });
    }
}

void Observatory::initDome()
{
    domeBox->setEnabled(true);

    if (getDomeModel() != nullptr)
    {
        connect(getDomeModel(), &Ekos::ObservatoryDomeModel::newLog, this, &Ekos::Observatory::appendLogText);

        // dome motion buttons
        connect(motionCWButton, &QPushButton::clicked, [ = ](bool checked)
        {
            getDomeModel()->moveDome(true, checked);
        });
        connect(motionCCWButton, &QPushButton::clicked, [ = ](bool checked)
        {
            getDomeModel()->moveDome(false, checked);
        });

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

        if (getDomeModel()->isRolloffRoof())
        {
            SlavingBox->setVisible(false);
            domeAzimuthPosition->setText(i18nc("Not Applicable", "N/A"));
            enableMotionControl(true);
        }
        else
        {
            // initialize the dome motion controls
            domeAzimuthChanged(getDomeModel()->azimuthPosition());

            // slaving
            showAutoSync(getDomeModel()->isAutoSync());
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
        if (getDomeModel()->hasShutter())
        {
            shutterBox->setVisible(true);
            shutterBox->setEnabled(true);
            connect(shutterOpen, &QPushButton::clicked, getDomeModel(), &Ekos::ObservatoryDomeModel::openShutter);
            connect(shutterClosed, &QPushButton::clicked, getDomeModel(), &Ekos::ObservatoryDomeModel::closeShutter);
            shutterClosed->setEnabled(true);
            shutterOpen->setEnabled(true);
            setShutterStatus(getDomeModel()->shutterStatus());
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
        setDomeParkStatus(getDomeModel()->parkStatus());

        // enable the UI controls for dome weather actions
        initWeatherActions(getWeatherModel() != nullptr && getWeatherModel()->isActive());
    }

}

void Observatory::shutdownDome()
{
    shutterBox->setEnabled(false);
    shutterBox->setVisible(false);
    domePark->setEnabled(false);
    domeUnpark->setEnabled(false);
    shutterClosed->setEnabled(false);
    shutterOpen->setEnabled(false);

    disconnect(domePark, &QPushButton::clicked, getDomeModel(), &Ekos::ObservatoryDomeModel::park);
    disconnect(domeUnpark, &QPushButton::clicked, getDomeModel(), &Ekos::ObservatoryDomeModel::unpark);

    // disable the UI controls for dome weather actions
    initWeatherActions(false);
    statusDefinitionBox->setVisible(false);
    domeBox->setEnabled(false);
}

void Observatory::setDomeStatus(ISD::Dome::Status status)
{
    qCDebug(KSTARS_EKOS_OBSERVATORY) << "Setting dome status to " << status;

    switch (status)
    {
        case ISD::Dome::DOME_ERROR:
            appendLogText(i18n("%1 error. See INDI log for details.",
                               getDomeModel()->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            motionCWButton->setChecked(false);
            motionCCWButton->setChecked(false);
            break;

        case ISD::Dome::DOME_IDLE:
            motionCWButton->setChecked(false);
            motionCWButton->setEnabled(true);
            motionCCWButton->setChecked(false);
            motionCCWButton->setEnabled(true);

            appendLogText(i18n("%1 is idle.", getDomeModel()->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            break;

        case ISD::Dome::DOME_MOVING_CW:
            motionCWButton->setChecked(true);
            motionCWButton->setEnabled(false);
            motionCCWButton->setChecked(false);
            motionCCWButton->setEnabled(true);
            if (getDomeModel()->isRolloffRoof())
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
            if (getDomeModel()->isRolloffRoof())
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

            appendLogText(i18n("%1 is parked.", getDomeModel()->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            break;

        case ISD::Dome::DOME_PARKING:
            toggleButtons(domePark, i18n("Parking"), domeUnpark, i18n("Unpark"));
            motionCWButton->setEnabled(true);

            if (getDomeModel()->isRolloffRoof())
                domeAzimuthPosition->setText(i18n("Closing"));
            else
                enableMotionControl(false);

            motionCWButton->setChecked(false);
            motionCCWButton->setChecked(true);

            appendLogText(i18n("%1 is parking...", getDomeModel()->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            break;

        case ISD::Dome::DOME_UNPARKING:
            toggleButtons(domeUnpark, i18n("Unparking"), domePark, i18n("Park"));
            motionCCWButton->setEnabled(true);

            if (getDomeModel()->isRolloffRoof())
                domeAzimuthPosition->setText(i18n("Opening"));
            else
                enableMotionControl(false);

            motionCWButton->setChecked(true);
            motionCCWButton->setChecked(false);

            appendLogText(i18n("%1 is unparking...", getDomeModel()->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
            break;

        case ISD::Dome::DOME_TRACKING:
            enableMotionControl(true);
            motionCWButton->setEnabled(true);
            motionCCWButton->setChecked(true);
            appendLogText(i18n("%1 is tracking.", getDomeModel()->isRolloffRoof() ? i18n("Rolloff roof") : i18n("Dome")));
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

            if (getDomeModel()->isRolloffRoof())
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

            if (getDomeModel()->isRolloffRoof())
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
            when.setTime_t(static_cast<uint>(last.key));
            updateSensorGraph(it->first, when, last.value);
        }
    }

    // force an update to the current graph
    if (!selectedSensorID.isEmpty())
        selectedSensorChanged(selectedSensorID);
}

void Observatory::setWeatherModel(ObservatoryWeatherModel *model)
{
    mObservatoryModel->setWeatherModel(model);

    // disable the weather UI
    enableWeather(false);

    if (model != nullptr)
    {
        connect(model, &Ekos::ObservatoryWeatherModel::ready, this, &Ekos::Observatory::initWeather);
        connect(model, &Ekos::ObservatoryWeatherModel::newWeatherData, this, &Ekos::Observatory::newWeatherData);
    }
    else
        shutdownWeather();

    // make invisible, since not implemented yet
    weatherWarningSchedulerCB->setVisible(false);
    weatherAlertSchedulerCB->setVisible(false);
}

void Observatory::enableMotionControl(bool enabled)
{
    MotionBox->setEnabled(enabled);

    // absolute motion controls
    if (getDomeModel()->canAbsoluteMove())
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
    if (getDomeModel()->canRelativeMove())
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
    if (getDomeModel()->isRolloffRoof())
    {
        motionCWButton->setText(i18n("Open"));
        motionCCWButton->setText(i18n("Close"));
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
    if (getDomeModel() == nullptr)
        showAutoSync(false);
    else
    {
        getDomeModel()->setAutoSync(enabled);
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
    // initialize the weather sensor data group box
    sensorDataBoxLayout = new QGridLayout();
    sensorData->setLayout(sensorDataBoxLayout);

    enableWeather(true);
    initSensorGraphs();

    connect(weatherWarningBox, &QGroupBox::clicked, getWeatherModel(), &ObservatoryWeatherModel::setWarningActionsActive);
    connect(weatherAlertBox, &QGroupBox::clicked, getWeatherModel(), &ObservatoryWeatherModel::setAlertActionsActive);

    connect(getWeatherModel(), &Ekos::ObservatoryWeatherModel::newStatus, this, &Ekos::Observatory::setWeatherStatus);
    connect(getWeatherModel(), &Ekos::ObservatoryWeatherModel::disconnected, this, &Ekos::Observatory::shutdownWeather);
    connect(clearGraphHistory, &QPushButton::clicked, this, &Observatory::clearSensorDataHistory);
    connect(autoscaleValuesCB, &QCheckBox::clicked, [this](bool checked)
    {
        getWeatherModel()->setAutoScaleValues(checked);
        this->refreshSensorGraph();
    });
    connect(&weatherStatusTimer, &QTimer::timeout, [this]()
    {
        weatherWarningStatusLabel->setText(getWeatherModel()->getWarningActionsStatus());
        weatherAlertStatusLabel->setText(getWeatherModel()->getAlertActionsStatus());
    });

    weatherBox->setEnabled(true);
    autoscaleValuesCB->setChecked(getWeatherModel()->autoScaleValues());
    weatherWarningBox->setChecked(getWeatherModel()->getWarningActionsActive());
    weatherAlertBox->setChecked(getWeatherModel()->getAlertActionsActive());
    setWeatherStatus(getWeatherModel()->status());
    setWarningActions(getWeatherModel()->getWarningActions());
    setAlertActions(getWeatherModel()->getAlertActions());

    initWeatherActions(true);
    weatherStatusTimer.start(1000);
    if (getWeatherModel()->refresh() == false)
        appendLogText(i18n("Refreshing weather data failed."));
    // avoid double init
    disconnect(getWeatherModel(), &Ekos::ObservatoryWeatherModel::ready, this, &Ekos::Observatory::initWeather);
}

void Observatory::shutdownWeather()
{
    weatherStatusTimer.stop();
    setWeatherStatus(ISD::Weather::WEATHER_IDLE);
    enableWeather(false);
    // disable the UI controls for weather actions
    initWeatherActions(false);
    // catch re-connect
    if (getWeatherModel() != nullptr)
        connect(getWeatherModel(), &Ekos::ObservatoryWeatherModel::ready, this, &Ekos::Observatory::initWeather);

}

void Observatory::initWeatherActions(bool enabled)
{
    if (enabled && getDomeModel() != nullptr && getDomeModel()->isActive())
    {
        // make the entire box visible
        weatherActionsBox->setVisible(true);
        weatherActionsBox->setEnabled(true);

        // enable warning and alert action control
        weatherAlertDomeCB->setEnabled(true);
        weatherWarningDomeCB->setEnabled(true);

        // only domes with shutters need shutter action controls
        if (getDomeModel()->hasShutter())
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


void Observatory::updateSensorGraph(QString sensor_label, QDateTime now, double value)
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
    sensorGraphData[id]->append(QCPGraphData(static_cast<double>(now.toTime_t()), value));

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

void Observatory::updateSensorData(const std::vector<ISD::Weather::WeatherData> &data)
{
    QDateTime now = KStarsData::Instance()->lt();

    for (auto &oneEntry : data)
    {
        QString const id = oneEntry.label;

        if (sensorDataWidgets[id] == nullptr)
        {
            QPushButton* labelWidget = new QPushButton(oneEntry.label);
            labelWidget->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
            labelWidget->setCheckable(true);
            labelWidget->setStyleSheet("QPushButton:checked\n{\nbackground-color: maroon;\nborder: 1px outset;\nfont-weight:bold;\n}");
            // we need the object name since the label may contain '&' for keyboard shortcuts
            labelWidget->setObjectName(oneEntry.label);

            QLineEdit* valueWidget = new QLineEdit(QString().setNum(oneEntry.value, 'f', 2));
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
            sensorDataWidgets[id]->first->setText(QString(oneEntry.label));
            sensorDataWidgets[id]->second->setText(QString().setNum(oneEntry.value, 'f', 2));
        }

        // store sensor data unit if necessary
        updateSensorGraph(oneEntry.label, now, oneEntry.value);
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
        QDateTime when = QDateTime::fromTime_t(sensorGraphs->graph(0)->dataMainKey(index));

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
    if (getWeatherModel()->autoScaleValues() == false)
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
                break;
            case ISD::Weather::WEATHER_WARNING:
                label = "security-medium";
                appendLogText(i18n("Weather Warning"));
                break;
            case ISD::Weather::WEATHER_ALERT:
                label = "security-low";
                appendLogText(i18n("Weather Alert"));
                break;
            default:
                label = "";
                break;
        }

        weatherStatusLabel->setPixmap(QIcon::fromTheme(label).pixmap(QSize(28, 28)));
        m_WeatherStatus = status;
    }

    // update weather sensor data
    updateSensorData(getWeatherModel()->getWeatherData());

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

    getWeatherModel()->setWarningActions(actions);
}

void Observatory::weatherAlertSettingsChanged()
{
    struct WeatherActions actions;
    actions.parkDome = weatherAlertDomeCB->isChecked();
    actions.closeShutter = weatherAlertShutterCB->isChecked();
    // Fixme: not implemented yet
    actions.stopScheduler = false;
    actions.delay = static_cast<unsigned int>(weatherAlertDelaySB->value());

    getWeatherModel()->setAlertActions(actions);
}

void Observatory::observatoryStatusChanged(bool ready)
{
    // statusReadyButton->setEnabled(!ready);
    statusReadyButton->setChecked(ready);
    emit newStatus(ready);
}

void Observatory::domeAzimuthChanged(double position)
{
    domeAzimuthPosition->setText(QString::number(position, 'f', 2));
}


void Observatory::setWarningActions(WeatherActions actions)
{
    if (getDomeModel() != nullptr)
        weatherWarningDomeCB->setChecked(actions.parkDome);
    else
        weatherWarningDomeCB->setChecked(actions.parkDome);

    if (getDomeModel() != nullptr && getDomeModel()->hasShutter())
        weatherWarningShutterCB->setChecked(actions.closeShutter);
    else
        weatherWarningShutterCB->setChecked(actions.closeShutter);

    weatherWarningDelaySB->setValue(static_cast<int>(actions.delay));
}


void Observatory::setAlertActions(WeatherActions actions)
{
    if (getDomeModel() != nullptr)
        weatherAlertDomeCB->setChecked(actions.parkDome);
    else
        weatherAlertDomeCB->setChecked(false);

    if (getDomeModel() != nullptr && getDomeModel()->hasShutter())
        weatherAlertShutterCB->setChecked(actions.closeShutter);
    else
        weatherAlertShutterCB->setChecked(false);

    weatherAlertDelaySB->setValue(static_cast<int>(actions.delay));
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
    mObservatoryModel->setStatusControl(control);
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

}
