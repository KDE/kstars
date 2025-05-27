/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2024 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "camera.h"
#include "capturemodulestate.h"
#include "capturedeviceadaptor.h"
#include "cameraprocess.h"
#include "ekos/manager.h"
#include "ekos/auxiliary/filtermanager.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/opticaltrainsettings.h"
#include "ekos/auxiliary/profilesettings.h"
#include <auxiliary/ksnotification.h>
#include "ekos/auxiliary/rotatorutils.h"
#include "exposurecalculator/exposurecalculatordialog.h"
#include "scriptsmanager.h"
#include "oal/observeradd.h"
#include "placeholderpath.h"
#include "ui_limits.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"

#include <ekos_capture_debug.h>

#include <QFileDialog>
#include <QStandardItemModel>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QHBoxLayout>

// These macros are used by functions in this file.
// Ideally, they should be in a common header or camera.h
#define QCDEBUG   qCDebug(KSTARS_EKOS_CAPTURE) << QString("[%1]").arg(getCameraName())
#define QCINFO    qCInfo(KSTARS_EKOS_CAPTURE) << QString("[%1]").arg(getCameraName())
#define QCWARNING qCWarning(KSTARS_EKOS_CAPTURE) << QString("[%1]").arg(getCameraName())

// KEY_ macros used in this file
#define KEY_FORMATS     "formatsList"
#define KEY_GAIN_KWD    "ccdGainKeyword" // Used in onStandAloneShow
#define KEY_OFFSET_KWD  "ccdOffsetKeyword" // Used in onStandAloneShow
#define KEY_FILTERS     "filtersList"
// KEY_TEMPERATURE, KEY_TIMESTAMP, KEY_ISOS, KEY_INDEX are in camera_device.cpp

namespace
{
// This was an anonymous namespace in camera.cpp, moved here as it's used by copyStandAloneSettings
const QStringList standAloneKeys = {KEY_FORMATS, KEY_GAIN_KWD, KEY_OFFSET_KWD,
                                    /*KEY_TEMPERATURE, KEY_TIMESTAMP,*/ KEY_FILTERS,
                                    /*KEY_ISOS, KEY_INDEX,*/ KEY_GAIN_KWD, KEY_OFFSET_KWD
                                   }; // Commented out keys that are in camera_device.cpp's syncCameraInfo
// This might need adjustment if those keys are also expected here.
// For now, assuming they are primarily for device sync.
QVariantMap copyStandAloneSettings(const QVariantMap &settings)
{
    QVariantMap foundSettings;
    for (const auto &k : standAloneKeys)
    {
        auto v = settings.find(k);
        if (v != settings.end())
            foundSettings[k] = *v;
    }
    return foundSettings;
}
}  // namespace


namespace Ekos
{

void Camera::loadGlobalSettings()
{
    QString key;
    QVariant value;

    QVariantMap settings;
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
    {
        if (oneWidget->objectName() == "opticalTrainCombo")
            continue;

        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid() && oneWidget->count() > 0)
        {
            oneWidget->setCurrentText(value.toString());
            settings[key] = value;
        }
        else
            QCDEBUG << "Option" << key << "not found!";
    }

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setValue(value.toDouble());
            settings[key] = value;
        }
        else
            QCDEBUG << "Option" << key << "not found!";
    }

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setValue(value.toInt());
            settings[key] = value;
        }
        else
            QCDEBUG << "Option" << key << "not found!";
    }

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setChecked(value.toBool());
            settings[key] = value;
        }
        else
            QCDEBUG << "Option" << key << "not found!";
    }

    // All Checkable Groupboxes
    for (auto &oneWidget : findChildren<QGroupBox*>())
    {
        if (oneWidget->isCheckable())
        {
            key = oneWidget->objectName();
            value = Options::self()->property(key.toLatin1());
            if (value.isValid())
            {
                oneWidget->setChecked(value.toBool());
                settings[key] = value;
            }
            else
                QCDEBUG << "Option" << key << "not found!";
        }
    }

    // All Radio buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setChecked(value.toBool());
            settings[key] = value;
        }
    }

    // All Line Edits
    for (auto &oneWidget : findChildren<QLineEdit*>())
    {
        if (oneWidget->objectName() == "qt_spinbox_lineedit" || oneWidget->isReadOnly())
            continue;
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setText(value.toString());
            settings[key] = value;
        }
    }
    m_GlobalSettings = settings;
    setSettings(settings);
}

void Camera::syncLimitSettings()
{
    m_LimitsUI->enforceStartGuiderDrift->setChecked(Options::enforceStartGuiderDrift());
    m_LimitsUI->startGuideDeviation->setValue(Options::startGuideDeviation());
    m_LimitsUI->enforceGuideDeviation->setChecked(Options::enforceGuideDeviation());
    m_LimitsUI->guideDeviation->setValue(Options::guideDeviation());
    m_LimitsUI->guideDeviationReps->setValue(static_cast<int>(Options::guideDeviationReps()));
    m_LimitsUI->enforceAutofocusHFR->setChecked(Options::enforceAutofocusHFR());
    m_LimitsUI->hFRThresholdPercentage->setValue(Options::hFRThresholdPercentage());
    m_LimitsUI->hFRDeviation->setValue(Options::hFRDeviation());
    m_LimitsUI->inSequenceCheckFrames->setValue(Options::inSequenceCheckFrames());
    m_LimitsUI->hFRCheckAlgorithm->setCurrentIndex(Options::hFRCheckAlgorithm());
    m_LimitsUI->enforceAutofocusOnTemperature->setChecked(Options::enforceAutofocusOnTemperature());
    m_LimitsUI->maxFocusTemperatureDelta->setValue(Options::maxFocusTemperatureDelta());
    m_LimitsUI->enforceRefocusEveryN->setChecked(Options::enforceRefocusEveryN());
    m_LimitsUI->refocusEveryN->setValue(static_cast<int>(Options::refocusEveryN()));
    m_LimitsUI->refocusAfterMeridianFlip->setChecked(Options::refocusAfterMeridianFlip());
}

void Camera::settleSettings()
{
    state()->setDirty(true);
    emit settingsUpdated(getAllSettings());
    // Save to optical train specific settings as well
    const int id = OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText());
    OpticalTrainSettings::Instance()->setOpticalTrainID(id);
    OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::Capture, m_settings);
    Options::setCaptureTrainID(id);
}

void Camera::syncSettings()
{
    QDoubleSpinBox *dsb = nullptr;
    QSpinBox *sb = nullptr;
    QCheckBox *cb = nullptr;
    QGroupBox *gb = nullptr;
    QRadioButton *rb = nullptr;
    QComboBox *cbox = nullptr;
    QLineEdit *le = nullptr;

    QString key;
    QVariant value;

    if ( (dsb = qobject_cast<QDoubleSpinBox*>(sender())))
    {
        key = dsb->objectName();
        value = dsb->value();

    }
    else if ( (sb = qobject_cast<QSpinBox*>(sender())))
    {
        key = sb->objectName();
        value = sb->value();
    }
    else if ( (cb = qobject_cast<QCheckBox*>(sender())))
    {
        key = cb->objectName();
        value = cb->isChecked();
    }
    else if ( (gb = qobject_cast<QGroupBox*>(sender())))
    {
        key = gb->objectName();
        value = gb->isChecked();
    }
    else if ( (rb = qobject_cast<QRadioButton*>(sender())))
    {
        key = rb->objectName();
        // Discard false requests
        if (rb->isChecked() == false)
        {
            settings().remove(key);
            return;
        }
        value = true;
    }
    else if ( (cbox = qobject_cast<QComboBox*>(sender())))
    {
        key = cbox->objectName();
        value = cbox->currentText();
    }
    else if ( (le = qobject_cast<QLineEdit*>(sender())))
    {
        key = le->objectName();
        value = le->text();
    }


    if (!m_standAlone)
    {
        settings()[key] = value;
        m_GlobalSettings[key] = value;
        // Save immediately
        Options::self()->setProperty(key.toLatin1(), value);
        m_DebounceTimer.start();
    }
}

void Camera::connectSyncSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        connect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Camera::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        connect(oneWidget, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Camera::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        connect(oneWidget, QOverload<int>::of(&QSpinBox::valueChanged), this, &Camera::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        connect(oneWidget, &QCheckBox::toggled, this, &Camera::syncSettings);

    // All Checkable Groupboxes
    for (auto &oneWidget : findChildren<QGroupBox*>())
        if (oneWidget->isCheckable())
            connect(oneWidget, &QGroupBox::toggled, this, &Camera::syncSettings);

    // All Radio Buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        connect(oneWidget, &QRadioButton::toggled, this, &Camera::syncSettings);

    // All Line Edits
    for (auto &oneWidget : findChildren<QLineEdit*>())
    {
        if (oneWidget->objectName() == "qt_spinbox_lineedit" || oneWidget->isReadOnly())
            continue;
        connect(oneWidget, &QLineEdit::textChanged, this, &Camera::syncSettings);
    }

    // Train combo box should NOT be synced.
    disconnect(opticalTrainCombo, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Camera::syncSettings);
}

void Camera::disconnectSyncSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        disconnect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Camera::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        disconnect(oneWidget, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Camera::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        disconnect(oneWidget, QOverload<int>::of(&QSpinBox::valueChanged), this, &Camera::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        disconnect(oneWidget, &QCheckBox::toggled, this, &Camera::syncSettings);

    // All Checkable Groupboxes
    for (auto &oneWidget : findChildren<QGroupBox*>())
        if (oneWidget->isCheckable())
            disconnect(oneWidget, &QGroupBox::toggled, this, &Camera::syncSettings);

    // All Radio Buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        disconnect(oneWidget, &QRadioButton::toggled, this, &Camera::syncSettings);

    // All Line Edits
    for (auto &oneWidget : findChildren<QLineEdit*>())
    {
        if (oneWidget->objectName() == "qt_spinbox_lineedit")
            continue;
        disconnect(oneWidget, &QLineEdit::textChanged, this, &Camera::syncSettings);
    }
}

QVariantMap Camera::getAllSettings() const
{
    QVariantMap settings;

    // All QLineEdits
    // N.B. This must be always first since other Widgets can be casted to QLineEdit like QSpinBox but not vice-versa.
    for (auto &oneWidget : findChildren<QLineEdit*>())
    {
        auto name = oneWidget->objectName();
        if (name == "qt_spinbox_lineedit")
            continue;
        settings.insert(name, oneWidget->text());
    }

    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->currentText());

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->value());

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->value());

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    // All Checkable Groupboxes
    for (auto &oneWidget : findChildren<QGroupBox*>())
        if (oneWidget->isCheckable())
            settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    // All Radio Buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    return settings;
}

void Camera::setAllSettings(const QVariantMap &settings, const QVariantMap * standAloneSettings)
{
    // Disconnect settings that we don't end up calling syncSettings while
    // performing the changes.
    disconnectSyncSettings();

    for (auto &name : settings.keys())
    {
        // Combo
        auto comboBox = findChild<QComboBox*>(name);
        if (comboBox)
        {
            syncControl(settings, name, comboBox);
            continue;
        }

        // Double spinbox
        auto doubleSpinBox = findChild<QDoubleSpinBox*>(name);
        if (doubleSpinBox)
        {
            syncControl(settings, name, doubleSpinBox);
            continue;
        }

        // spinbox
        auto spinBox = findChild<QSpinBox*>(name);
        if (spinBox)
        {
            syncControl(settings, name, spinBox);
            continue;
        }

        // checkbox
        auto checkbox = findChild<QCheckBox*>(name);
        if (checkbox)
        {
            syncControl(settings, name, checkbox);
            continue;
        }

        // Checkable Groupboxes
        auto groupbox = findChild<QGroupBox*>(name);
        if (groupbox && groupbox->isCheckable())
        {
            syncControl(settings, name, groupbox);
            continue;
        }

        // Radio button
        auto radioButton = findChild<QRadioButton*>(name);
        if (radioButton)
        {
            syncControl(settings, name, radioButton);
            continue;
        }

        // Line Edit
        auto lineEdit = findChild<QLineEdit*>(name);
        if (lineEdit)
        {
            syncControl(settings, name, lineEdit);
            continue;
        }
    }

    // Sync to options
    for (auto &key : settings.keys())
    {
        auto value = settings[key];
        // Save immediately
        Options::self()->setProperty(key.toLatin1(), value);

        m_settings[key] = value;
        m_GlobalSettings[key] = value;
    }

    if (standAloneSettings && standAloneSettings->size() > 0)
    {
        for (const auto &k : standAloneSettings->keys())
            m_settings[k] = (*standAloneSettings)[k];
    }

    emit settingsUpdated(getAllSettings());

    // Save to optical train specific settings as well
    if (!m_standAlone)
    {
        const int id = OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText());
        OpticalTrainSettings::Instance()->setOpticalTrainID(id);
        OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::Capture, m_settings);
        Options::setCaptureTrainID(id);
    }
    // Restablish connections
    connectSyncSettings();
}

bool Camera::syncControl(const QVariantMap &settings, const QString &key, QWidget * widget)
{
    QSpinBox *pSB = nullptr;
    QDoubleSpinBox *pDSB = nullptr;
    QCheckBox *pCB = nullptr;
    QComboBox *pComboBox = nullptr;
    QSplitter *pSplitter = nullptr;
    QRadioButton *pRadioButton = nullptr;
    QLineEdit *pLineEdit = nullptr;
    bool ok = true;

    if ((pSB = qobject_cast<QSpinBox *>(widget)))
    {
        const int value = settings[key].toInt(&ok);
        if (ok)
        {
            pSB->setValue(value);
            return true;
        }
    }
    else if ((pDSB = qobject_cast<QDoubleSpinBox *>(widget)))
    {
        const double value = settings[key].toDouble(&ok);
        if (ok)
        {
            pDSB->setValue(value);
            // Special case for gain
            if (pDSB == captureGainN)
            {
                if (captureGainN->value() != GainSpinSpecialValue)
                    setGain(captureGainN->value());
                else
                    setGain(-1);
            }
            else if (pDSB == captureOffsetN)
            {
                if (captureOffsetN->value() != OffsetSpinSpecialValue)
                    setOffset(captureOffsetN->value());
                else
                    setOffset(-1);
            }
            return true;
        }
    }
    else if ((pCB = qobject_cast<QCheckBox *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value != pCB->isChecked())
            pCB->setChecked(value);
        return true;
    }
    else if ((pRadioButton = qobject_cast<QRadioButton *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value)
            pRadioButton->setChecked(true);
        return true;
    }
    // ONLY FOR STRINGS, not INDEX
    else if ((pComboBox = qobject_cast<QComboBox *>(widget)))
    {
        const QString value = settings[key].toString();
        pComboBox->setCurrentText(value);
        return true;
    }
    else if ((pSplitter = qobject_cast<QSplitter *>(widget)))
    {
        const auto value = QByteArray::fromBase64(settings[key].toString().toUtf8());
        pSplitter->restoreState(value);
        return true;
    }
    else if ((pLineEdit = qobject_cast<QLineEdit *>(widget)))
    {
        const auto value = settings[key].toString();
        pLineEdit->setText(value);
        // Special case
        if (pLineEdit == fileRemoteDirT)
            generatePreviewFilename();
        return true;
    }

    return false;
}

void Camera::setupOpticalTrainManager()
{
    connect(OpticalTrainManager::Instance(), &OpticalTrainManager::updated, this, [this]()
    {
        const int current = opticalTrainCombo->currentIndex();
        initOpticalTrain();
        if (current >= 0 && current < opticalTrainCombo->count())
            selectOpticalTrain(opticalTrainCombo->itemText(current));
    });
    connect(trainB, &QPushButton::clicked, this, [this]()
    {
        OpticalTrainManager::Instance()->openEditor(opticalTrainCombo->currentText());
    });
    connect(opticalTrainCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        const int id = OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(index));
        // remember only the train ID from the first Capture tab
        if (!m_standAlone && cameraId() == 0)
        {
            ProfileSettings::Instance()->setOneSetting(ProfileSettings::CaptureOpticalTrain, id);
        }
        refreshOpticalTrain(id);
        emit trainChanged();
    });
}

void Camera::initOpticalTrain()
{
    opticalTrainCombo->blockSignals(true);
    opticalTrainCombo->clear();
    opticalTrainCombo->addItems(OpticalTrainManager::Instance()->getTrainNames());
    trainB->setEnabled(true);
    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::CaptureOpticalTrain);
    if (m_standAlone || trainID.isValid())
    {
        auto id = m_standAlone ? Options::captureTrainID() : trainID.toUInt();
        Options::setCaptureTrainID(id);

        // If train not found, select the first one available.
        if (OpticalTrainManager::Instance()->exists(id) == false)
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << "Optical train doesn't exist for id" << id;
            id = OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(0));
        }

        refreshOpticalTrain(id);
    }
    opticalTrainCombo->blockSignals(false);
}

void Camera::refreshOpticalTrain(const int id)
{
    auto name = OpticalTrainManager::Instance()->name(id);

    opticalTrainCombo->setCurrentText(name);
    if (!m_standAlone)
        process()->refreshOpticalTrain(name);

    // Load train settings
    // This needs to be done near the start of this function as methods further down
    // cause settings to be updated, which in turn interferes with the persistence and
    // setup of settings in OpticalTrainSettings
    OpticalTrainSettings::Instance()->setOpticalTrainID(id);
    auto settings = OpticalTrainSettings::Instance()->getOneSetting(OpticalTrainSettings::Capture);
    if (settings.isValid())
    {
        auto map = settings.toJsonObject().toVariantMap();
        if (map != m_settings)
        {
            QVariantMap standAloneSettings = copyStandAloneSettings(m_settings);
            m_settings.clear();
            setAllSettings(map, &standAloneSettings);
        }
    }
    else
    {
        setSettings(m_GlobalSettings);
    }
}

void Camera::selectOpticalTrain(QString name)
{
    opticalTrainCombo->setCurrentText(name);
}

void Camera::storeTrainKey(const QString &key, const QStringList &list)
{
    if (!m_settings.contains(key) || m_settings[key].toStringList() != list)
    {
        m_settings[key] = list;
        m_DebounceTimer.start();
    }
}

void Camera::storeTrainKeyString(const QString &key, const QString &str)
{
    if (!m_settings.contains(key) || m_settings[key].toString() != str)
    {
        m_settings[key] = str;
        m_DebounceTimer.start();
    }
}

void Camera::setupFilterManager()
{
    // Clear connections if there was an existing filter manager
    if (filterManager())
        filterManager()->disconnect(this);

    // Create new global filter manager or refresh its filter wheel
    Manager::Instance()->createFilterManager(devices()->filterWheel());

    // Set the filter manager for this filter wheel.
    Manager::Instance()->getFilterManager(devices()->filterWheel()->getDeviceName(), m_FilterManager);

    devices()->setFilterManager(m_FilterManager);

    connect(filterManager().get(), &FilterManager::updated, this, [this]()
    {
        emit filterManagerUpdated(devices()->filterWheel());
    });

    // display capture status changes
    connect(filterManager().get(), &FilterManager::newStatus, this, &Camera::newFilterStatus);

    connect(filterManagerB, &QPushButton::clicked, this, [this]()
    {
        filterManager()->refreshFilterModel();
        filterManager()->show();
        filterManager()->raise();
    });

    connect(filterManager().get(), &FilterManager::failed, this, [this]()
    {
        if (activeJob())
        {
            appendLogText(i18n("Filter operation failed."));
            abort();
        }
    });

    // filter changes
    connect(filterManager().get(), &FilterManager::newStatus, this, &Camera::setFilterStatus);

    connect(filterManager().get(), &FilterManager::labelsChanged, this, [this]()
    {
        FilterPosCombo->clear();
        FilterPosCombo->addItems(filterManager()->getFilterLabels());
        FilterPosCombo->setCurrentIndex(filterManager()->getFilterPosition() - 1);
        updateCurrentFilterPosition();
        storeTrainKey(KEY_FILTERS, filterManager()->getFilterLabels());
    });

    connect(filterManager().get(), &FilterManager::positionChanged, this, [this]()
    {
        FilterPosCombo->setCurrentIndex(filterManager()->getFilterPosition() - 1);
        updateCurrentFilterPosition();
    });
}

void Camera::clearFilterManager()
{
    // Clear connections if there was an existing filter manager
    if (m_FilterManager)
        m_FilterManager->disconnect(this);

    // clear the filter manager for this camera
    m_FilterManager.clear();
    devices()->clearFilterManager();
}

void Camera::refreshFilterSettings()
{
    FilterPosCombo->clear();

    if (!devices()->filterWheel())
    {
        FilterPosLabel->setEnabled(false);
        FilterPosCombo->setEnabled(false);
        filterEditB->setEnabled(false);
        filterManagerB->setEnabled(false);

        clearFilterManager();
        return;
    }

    FilterPosLabel->setEnabled(true);
    FilterPosCombo->setEnabled(true);
    filterEditB->setEnabled(true);
    filterManagerB->setEnabled(true);

    setupFilterManager();

    const auto labels = process()->filterLabels();
    FilterPosCombo->addItems(labels);

    // Save ISO List in train settings if different
    storeTrainKey(KEY_FILTERS, labels);

    updateCurrentFilterPosition();

    filterEditB->setEnabled(state()->getCurrentFilterPosition() > 0);
    filterManagerB->setEnabled(state()->getCurrentFilterPosition() > 0);

    FilterPosCombo->setCurrentIndex(state()->getCurrentFilterPosition() - 1);
}

void Camera::updateCurrentFilterPosition()
{
    const QString currentFilterText = FilterPosCombo->itemText(m_FilterManager->getFilterPosition() - 1);
    state()->setCurrentFilterPosition(m_FilterManager->getFilterPosition(),
                                      currentFilterText,
                                      m_FilterManager->getFilterLock(currentFilterText));

}

void Camera::setFilterWheel(QString name)
{
    // Should not happen
    if (m_standAlone)
        return;

    if (devices()->filterWheel() && devices()->filterWheel()->getDeviceName() == name)
    {
        refreshFilterSettings();
        return;
    }

    auto isConnected = devices()->filterWheel() && devices()->filterWheel()->isConnected();
    FilterPosLabel->setEnabled(isConnected);
    FilterPosCombo->setEnabled(isConnected);
    filterManagerB->setEnabled(isConnected);

    refreshFilterSettings();

    if (devices()->filterWheel())
        emit settingsUpdated(getAllSettings());
}

void Camera::editFilterName()
{
    if (m_standAlone)
    {
        QStringList labels;
        for (int index = 0; index < FilterPosCombo->count(); index++)
            labels << FilterPosCombo->itemText(index);
        QStringList newLabels;
        if (editFilterNameInternal(labels, newLabels))
        {
            FilterPosCombo->clear();
            FilterPosCombo->addItems(newLabels);
        }
    }
    else
    {
        if (devices()->filterWheel() == nullptr || state()->getCurrentFilterPosition() < 1)
            return;

        QStringList labels = filterManager()->getFilterLabels();
        QStringList newLabels;
        if (editFilterNameInternal(labels, newLabels))
            filterManager()->setFilterNames(newLabels);
    }
}

bool Camera::editFilterNameInternal(const QStringList &labels, QStringList &newLabels)
{
    QDialog filterDialog;

    QFormLayout *formLayout = new QFormLayout(&filterDialog);
    QVector<QLineEdit *> newLabelEdits;

    for (uint8_t i = 0; i < labels.count(); i++)
    {
        QLabel *existingLabel = new QLabel(QString("%1. <b>%2</b>").arg(i + 1).arg(labels[i]), &filterDialog);
        QLineEdit *newLabel = new QLineEdit(labels[i], &filterDialog);
        newLabelEdits.append(newLabel);
        formLayout->addRow(existingLabel, newLabel);
    }

    QString title = m_standAlone ?
                    "Edit Filter Names" : devices()->filterWheel()->getDeviceName();
    filterDialog.setWindowTitle(title);
    filterDialog.setLayout(formLayout);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &filterDialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &filterDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &filterDialog, &QDialog::reject);
    filterDialog.layout()->addWidget(buttonBox);

    if (filterDialog.exec() == QDialog::Accepted)
    {
        QStringList results;
        for (uint8_t i = 0; i < labels.count(); i++)
            results << newLabelEdits[i]->text();
        newLabels = results;
        return true;
    }
    return false;
}

void Camera::updateCaptureFormats()
{
    if (!activeCamera()) return;

    // list of capture types
    QStringList frameTypes = process()->frameTypes();
    // current selection
    QString currentType = captureTypeS->currentText();

    captureTypeS->blockSignals(true);
    captureTypeS->clear();

    if (frameTypes.isEmpty())
        captureTypeS->setEnabled(false);
    else
    {
        captureTypeS->setEnabled(true);
        captureTypeS->addItems(frameTypes);

        if (currentType == "")
        {
            // if no capture type is selected, take the value from the active chip
            captureTypeS->setCurrentIndex(devices()->getActiveChip()->getFrameType());
            currentType = captureTypeS->currentText();
        }
        else
            captureTypeS->setCurrentText(currentType);
    }
    captureTypeS->blockSignals(false);

    const bool isVideo = currentType == CAPTURE_TYPE_VIDEO;

    // Capture Format
    captureFormatS->blockSignals(true);
    captureFormatS->clear();
    const auto list = isVideo ? activeCamera()->getVideoFormats() : activeCamera()->getCaptureFormats();
    captureFormatS->addItems(list);
    storeTrainKey(KEY_FORMATS, list);
    captureFormatS->setCurrentText(activeCamera()->getCaptureFormat());
    captureFormatS->blockSignals(false);

    // Encoding format
    captureEncodingS->blockSignals(true);
    captureEncodingS->clear();
    captureEncodingS->addItems(isVideo ? activeCamera()->getStreamEncodings() : activeCamera()->getEncodingFormats());
    captureEncodingS->setCurrentText(isVideo ? activeCamera()->getStreamEncoding() : activeCamera()->getEncodingFormat());
    captureEncodingS->blockSignals(false);
}

void Camera::updateHFRCheckAlgo()
{
    // Threshold % is not relevant for FIXED HFR do disable the field
    const bool threshold = (m_LimitsUI->hFRCheckAlgorithm->currentIndex() != HFR_CHECK_FIXED);
    m_LimitsUI->hFRThresholdPercentage->setEnabled(threshold);
    m_LimitsUI->limitFocusHFRThresholdLabel->setEnabled(threshold);
    m_LimitsUI->limitFocusHFRPercentLabel->setEnabled(threshold);
    state()->updateHFRThreshold();
}

void Camera::clearAutoFocusHFR()
{
    if (Options::hFRCheckAlgorithm() == HFR_CHECK_FIXED)
        return;

    m_LimitsUI->hFRDeviation->setValue(0);
    //firstAutoFocus = true;
}

void Camera::checkFrameType(int index)
{
    updateCaptureFormats();

    calibrationB->setEnabled(index != FRAME_LIGHT);
    generateDarkFlatsB->setEnabled(index != FRAME_LIGHT);
    const bool isVideo = captureTypeS->currentText() == CAPTURE_TYPE_VIDEO;
    exposureOptions->setCurrentIndex(isVideo ? 1 : 0);
    exposureOptionsLabel->setToolTip(isVideo ? i18n("Duration of the video sequence") : i18n("Number of images to capture"));
    exposureOptionsLabel->setText(isVideo ? i18n("Duration:") : i18n("Count:"));
    exposureLabel->setToolTip(isVideo ? i18n("Exposure time in seconds of a single video frame") :
                              i18n("Exposure time in seconds for individual images"));

    // enforce the upload mode for videos
    if (isVideo)
        selectUploadMode(ISD::Camera::UPLOAD_REMOTE);
    else
        checkUploadMode(fileUploadModeS->currentIndex());
}

void Camera::updateVideoDurationUnit()
{
    if (videoDurationUnitCB->currentIndex() == 0)
    {
        // switching from frames to seconds
        videoDurationSB->setValue(videoDurationSB->value() * captureExposureN->value());
        videoDurationSB->setDecimals(2);
    }
    else
    {
        // switching from seconds to frames
        videoDurationSB->setValue(videoDurationSB->value() / captureExposureN->value());
        videoDurationSB->setDecimals(0);
    }
}

void Camera::saveFITSDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(Manager::Instance(), i18nc("@title:window", "FITS Save Directory"),
                  m_dirPath.toLocalFile());
    if (dir.isEmpty())
        return;

    fileDirectoryT->setText(QDir::toNativeSeparators(dir));
}

void Camera::checkUploadMode(int index)
{
    fileRemoteDirT->setEnabled(index != ISD::Camera::UPLOAD_CLIENT);

    const bool isVideo = captureTypeS->currentText() == CAPTURE_TYPE_VIDEO;

    // Access the underlying model of the combo box
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(fileUploadModeS->model());
    if (model)
    {
        // restrict selection to local (on the INDI server) for videos
        model->item(0)->setFlags(isVideo ? (model->item(0)->flags() & ~Qt::ItemIsEnabled) :
                                 (model->item(0)->flags() | Qt::ItemIsEnabled));
        model->item(2)->setFlags(isVideo ? (model->item(2)->flags() & ~Qt::ItemIsEnabled) :
                                 (model->item(2)->flags() | Qt::ItemIsEnabled));
    }

    generatePreviewFilename();
}

void Camera::selectUploadMode(int index)
{
    fileUploadModeS->setCurrentIndex(index);
    checkUploadMode(index);
}

bool Camera::checkUploadPaths(FilenamePreviewType filenamePreview, const QSharedPointer<SequenceJob> &job)
{
    // only relevant if we do not generate file name previews
    if (filenamePreview != FILENAME_NOT_PREVIEW)
        return true;

    if (fileUploadModeS->currentIndex() != ISD::Camera::UPLOAD_CLIENT && job->getRemoteDirectory().toString().isEmpty())
    {
        KSNotification::error(i18n("You must set local or remote directory for Remotely & Both modes."));
        return false;
    }

    if (fileUploadModeS->currentIndex() != ISD::Camera::UPLOAD_REMOTE && fileDirectoryT->text().isEmpty())
    {
        KSNotification::error(i18n("You must set local directory for Locally & Both modes."));
        return false;
    }
    // everything OK
    return true;
}

void Camera::generatePreviewFilename()
{
    if (state()->isCaptureRunning() == false)
    {
        placeholderFormatT->setToolTip(previewFilename( fileUploadModeS->currentIndex() == ISD::Camera::UPLOAD_REMOTE ?
                                       FILENAME_REMOTE_PREVIEW : FILENAME_LOCAL_PREVIEW ));
        emit newLocalPreview(placeholderFormatT->toolTip());

        if (fileUploadModeS->currentIndex() != ISD::Camera::UPLOAD_CLIENT)
            fileRemoteDirT->setToolTip(previewFilename( FILENAME_REMOTE_PREVIEW ));
    }
}

QString Camera::previewFilename(FilenamePreviewType previewType)
{
    QString previewText;
    QString m_format;
    auto separator = QDir::separator();

    if (previewType == FILENAME_LOCAL_PREVIEW)
    {
        if(!fileDirectoryT->text().endsWith(separator) && !placeholderFormatT->text().startsWith(separator))
            placeholderFormatT->setText(separator + placeholderFormatT->text());
        m_format = fileDirectoryT->text() + placeholderFormatT->text() + formatSuffixN->prefix() +
                   formatSuffixN->cleanText();
    }
    else if (previewType == FILENAME_REMOTE_PREVIEW)
        m_format = fileRemoteDirT->text().isEmpty() ? fileDirectoryT->text() : fileRemoteDirT->text();

    //Guard against an empty format to avoid the empty directory warning pop-up in addjob
    if (m_format.isEmpty())
        return previewText;
    // Tags %d & %p disable for now for simplicity
    //    else if (state()->sequenceURL().toLocalFile().isEmpty() && (m_format.contains("%d") || m_format.contains("%p")
    //             || m_format.contains("%f")))
    else if (state()->sequenceURL().toLocalFile().isEmpty() && m_format.contains("%f"))
        previewText = ("Save the sequence file to show filename preview");
    else
    {
        // create temporarily a sequence job
        QSharedPointer<SequenceJob> m_job = createJob(SequenceJob::JOBTYPE_PREVIEW, previewType);
        if (m_job.isNull())
            return previewText;

        QString previewSeq;
        if (state()->sequenceURL().toLocalFile().isEmpty())
        {
            if (m_format.startsWith(separator))
                previewSeq = m_format.left(m_format.lastIndexOf(separator));
        }
        else
            previewSeq = state()->sequenceURL().toLocalFile();
        auto m_placeholderPath = PlaceholderPath(previewSeq);

        QString extension;
        if (captureEncodingS->currentText() == "FITS")
            extension = ".fits";
        else if (captureEncodingS->currentText() == "XISF")
            extension = ".xisf";
        else
            extension = ".[NATIVE]";
        previewText = m_placeholderPath.generateSequenceFilename(*m_job, previewType == FILENAME_LOCAL_PREVIEW, true, 1,
                      extension, "", false);
        previewText = QDir::toNativeSeparators(previewText);
        // we do not use it any more
        m_job->deleteLater();
    }

    // Must change directory separate to UNIX style for remote
    if (previewType == FILENAME_REMOTE_PREVIEW)
        previewText.replace(separator, "/");

    return previewText;
}

void Camera::openExposureCalculatorDialog()
{
    QCINFO << "Instantiating an Exposure Calculator";

    // Learn how to read these from indi
    double preferredSkyQuality = 20.5;

    auto scope = currentScope();
    double focalRatio = scope["focal_ratio"].toDouble(-1);

    auto reducedFocalLength = currentReducer() * scope["focal_length"].toDouble(-1);
    auto aperture = currentAperture();
    auto reducedFocalRatio = (focalRatio > 0 || aperture == 0) ? focalRatio : reducedFocalLength / aperture;

    if (devices()->getActiveCamera() != nullptr)
    {
        QCINFO << "set ExposureCalculator preferred camera to active camera id: "
               << devices()->getActiveCamera()->getDeviceName();
    }

    QPointer<ExposureCalculatorDialog> anExposureCalculatorDialog(new ExposureCalculatorDialog(KStars::Instance(),
            preferredSkyQuality,
            reducedFocalRatio,
            devices()->getActiveCamera()->getDeviceName()));
    anExposureCalculatorDialog->setAttribute(Qt::WA_DeleteOnClose);
    anExposureCalculatorDialog->show();
}

void Camera::handleScriptsManager()
{
    QMap<ScriptTypes, QString> old_scripts = m_scriptsManager->getScripts();

    if (m_scriptsManager->exec() != QDialog::Accepted)
        // reset to old value
        m_scriptsManager->setScripts(old_scripts);
}

void Camera::showObserverDialog()
{
    QList<OAL::Observer *> m_observerList;
    KStars::Instance()->data()->userdb()->GetAllObservers(m_observerList);
    QStringList observers;
    for (auto &o : m_observerList)
        observers << QString("%1 %2").arg(o->name(), o->surname());

    QDialog observersDialog(this);
    observersDialog.setWindowTitle(i18nc("@title:window", "Select Current Observer"));

    QLabel label(i18n("Current Observer:"));

    QComboBox observerCombo(&observersDialog);
    observerCombo.addItems(observers);
    observerCombo.setCurrentText(getObserverName());
    observerCombo.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QPushButton manageObserver(&observersDialog);
    manageObserver.setFixedSize(QSize(32, 32));
    manageObserver.setIcon(QIcon::fromTheme("document-edit"));
    manageObserver.setAttribute(Qt::WA_LayoutUsesWidgetRect);
    manageObserver.setToolTip(i18n("Manage Observers"));
    connect(&manageObserver, &QPushButton::clicked, this, [&]()
    {
        ObserverAdd add;
        add.exec();

        QList<OAL::Observer *> m_observerList_updated; // Renamed to avoid conflict
        KStars::Instance()->data()->userdb()->GetAllObservers(m_observerList_updated);
        QStringList observers_updated; // Renamed to avoid conflict
        for (auto &o : m_observerList_updated)
            observers_updated << QString("%1 %2").arg(o->name(), o->surname());

        observerCombo.clear();
        observerCombo.addItems(observers_updated);
        observerCombo.setCurrentText(getObserverName());

    });

    QHBoxLayout * layout = new QHBoxLayout;
    layout->addWidget(&label);
    layout->addWidget(&observerCombo);
    layout->addWidget(&manageObserver);

    observersDialog.setLayout(layout);

    observersDialog.exec();
    setObserverName(observerCombo.currentText());
}

void Camera::onStandAloneShow(QShowEvent * event)
{
    OpticalTrainSettings::Instance()->setOpticalTrainID(Options::captureTrainID());
    auto oneSetting = OpticalTrainSettings::Instance()->getOneSetting(OpticalTrainSettings::Capture);
    setSettings(oneSetting.toJsonObject().toVariantMap());

    Q_UNUSED(event);
    // QSharedPointer<FilterManager> fm; // This was unused

    captureGainN->setValue(GainSpinSpecialValue);
    captureOffsetN->setValue(OffsetSpinSpecialValue);

    m_standAloneUseCcdGain = true;
    m_standAloneUseCcdOffset = true;
    if (m_settings.contains(KEY_GAIN_KWD) && m_settings[KEY_GAIN_KWD].toString() == "CCD_CONTROLS")
        m_standAloneUseCcdGain = false;
    if (m_settings.contains(KEY_OFFSET_KWD) && m_settings[KEY_OFFSET_KWD].toString() == "CCD_CONTROLS")
        m_standAloneUseCcdOffset = false;


    // Capture Gain
    connect(captureGainN, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        if (captureGainN->value() != GainSpinSpecialValue)
            setGain(captureGainN->value());
        else
            setGain(-1);
    });

    // Capture Offset
    connect(captureOffsetN, &QDoubleSpinBox::editingFinished, this, [this]()
    {
        if (captureOffsetN->value() != OffsetSpinSpecialValue)
            setOffset(captureOffsetN->value());
        else
            setOffset(-1);
    });
}

void Camera::setStandAloneGain(double value)
{
    QMap<QString, QMap<QString, QVariant> > propertyMap = m_customPropertiesDialog->getCustomProperties();

    if (m_standAloneUseCcdGain)
    {
        if (value >= 0)
        {
            QMap<QString, QVariant> ccdGain;
            ccdGain["GAIN"] = value;
            propertyMap["CCD_GAIN"] = ccdGain;
        }
        else
        {
            propertyMap["CCD_GAIN"].remove("GAIN");
            if (propertyMap["CCD_GAIN"].size() == 0)
                propertyMap.remove("CCD_GAIN");
        }
    }
    else
    {
        if (value >= 0)
        {
            QMap<QString, QVariant> ccdGain = propertyMap["CCD_CONTROLS"];
            ccdGain["Gain"] = value;
            propertyMap["CCD_CONTROLS"] = ccdGain;
        }
        else
        {
            propertyMap["CCD_CONTROLS"].remove("Gain");
            if (propertyMap["CCD_CONTROLS"].size() == 0)
                propertyMap.remove("CCD_CONTROLS");
        }
    }

    m_customPropertiesDialog->setCustomProperties(propertyMap);
}

void Camera::setStandAloneOffset(double value)
{
    QMap<QString, QMap<QString, QVariant> > propertyMap = m_customPropertiesDialog->getCustomProperties();

    if (m_standAloneUseCcdOffset)
    {
        if (value >= 0)
        {
            QMap<QString, QVariant> ccdOffset;
            ccdOffset["OFFSET"] = value;
            propertyMap["CCD_OFFSET"] = ccdOffset;
        }
        else
        {
            propertyMap["CCD_OFFSET"].remove("OFFSET");
            if (propertyMap["CCD_OFFSET"].size() == 0)
                propertyMap.remove("CCD_OFFSET");
        }
    }
    else
    {
        if (value >= 0)
        {
            QMap<QString, QVariant> ccdOffset = propertyMap["CCD_CONTROLS"];
            ccdOffset["Offset"] = value;
            propertyMap["CCD_CONTROLS"] = ccdOffset;
        }
        else
        {
            propertyMap["CCD_CONTROLS"].remove("Offset");
            if (propertyMap["CCD_CONTROLS"].size() == 0)
                propertyMap.remove("CCD_CONTROLS");
        }
    }

    m_customPropertiesDialog->setCustomProperties(propertyMap);
}

QJsonObject Camera::currentScope()
{
    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::CaptureOpticalTrain);
    if (activeCamera() && trainID.isValid())
    {
        auto id = trainID.toUInt();
        auto name = OpticalTrainManager::Instance()->name(id);
        return OpticalTrainManager::Instance()->getScope(name);
    }
    // return empty JSON object
    return QJsonObject();
}

double Camera::currentReducer()
{
    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::CaptureOpticalTrain);
    if (activeCamera() && trainID.isValid())
    {
        auto id = trainID.toUInt();
        auto name = OpticalTrainManager::Instance()->name(id);
        return OpticalTrainManager::Instance()->getReducer(name);
    }
    // no reducer available
    return 1.0;
}

double Camera::currentAperture()
{
    auto scope = currentScope();

    double focalLength = scope["focal_length"].toDouble(-1);
    double aperture = scope["aperture"].toDouble(-1);
    double focalRatio = scope["focal_ratio"].toDouble(-1);

    // DSLR Lens Aperture
    if (aperture < 0 && focalRatio > 0)
        aperture = focalLength * focalRatio;

    return aperture;
}

} // namespace Ekos
