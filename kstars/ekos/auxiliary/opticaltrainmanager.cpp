/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opticaltrainmanager.h"
#include <kstars_debug.h>

#include "ksnotification.h"
#include "kstarsdata.h"
#include "kstars.h"
#include "indi/indilistener.h"
#include "ekos/auxiliary/profilesettings.h"
#include "oal/equipmentwriter.h"

#include <QTimer>
#include <QSqlTableModel>
#include <QSqlDatabase>
#include <QSqlRecord>

#include <basedevice.h>

#include <algorithm>

namespace Ekos
{

OpticalTrainManager *OpticalTrainManager::m_Instance = nullptr;

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
OpticalTrainManager *OpticalTrainManager::Instance()
{
    if (m_Instance == nullptr)
        m_Instance = new OpticalTrainManager();

    return m_Instance;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::release()
{
    delete(m_Instance);
    m_Instance = nullptr;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
OpticalTrainManager::OpticalTrainManager() : QDialog(KStars::Instance())
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    setupUi(this);

    connect(this, &QDialog::finished, this, [this]()
    {
        emit configurationRequested(false);
    });

    // Mount Combo
    connect(mountComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this]()
    {
        updateOpticalTrainValue(mountComboBox, "mount");
    });

    // DustCap Combo
    connect(dustCapComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this]()
    {
        updateOpticalTrainValue(dustCapComboBox, "dustcap");
    });

    // Light Box
    connect(lightBoxComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this]()
    {
        updateOpticalTrainValue(lightBoxComboBox, "lightbox");
    });

    // Scope / Lens
    connect(scopeComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this]()
    {
        updateOpticalTrainValue(scopeComboBox, "scope");
    });

    // Reducer
    connect(reducerSpinBox, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
            [this](double value)
    {
        updateOpticalTrainValue(value, "reducer");
    });

    // Rotator
    connect(rotatorComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this]()
    {
        updateOpticalTrainValue(rotatorComboBox, "rotator");
    });

    // Focuser
    connect(focusComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this]()
    {
        updateOpticalTrainValue(focusComboBox, "focuser");
    });

    // Filter Wheel
    connect(filterComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this]()
    {
        updateOpticalTrainValue(filterComboBox, "filterwheel");
    });

    // Camera
    connect(cameraComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this]()
    {
        updateOpticalTrainValue(cameraComboBox, "camera");
    });

    // Guider
    connect(guiderComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this]()
    {
        updateOpticalTrainValue(guiderComboBox, "guider");
    });

    connect(addB, &QPushButton::clicked, this, [this]()
    {
        addOpticalTrain(m_TrainNames.count(), i18n("New Train"));
        m_OpticalTrainsModel->select();
        refreshModel();
        trainNamesList->setCurrentRow(trainNamesList->count() - 1);
        selectOpticalTrain(trainNamesList->currentItem());
    });

    connect(removeB, &QPushButton::clicked, this, [this]()
    {
        if (trainNamesList->currentItem() != nullptr)
        {
            removeOpticalTrain(trainNamesList->currentItem()->text());
            removeB->setEnabled(false);
        }
    });

    connect(resetB, &QPushButton::clicked, this, &OpticalTrainManager::reset);

    connect(opticalElementsB, &QPushButton::clicked, this, [this]()
    {
        QScopedPointer<EquipmentWriter> writer(new EquipmentWriter());
        writer->loadEquipment();
        writer->exec();
        refreshOpticalElements();
    });

    connect(trainNamesList, &QListWidget::itemClicked, this, [this](QListWidgetItem * item)
    {
        selectOpticalTrain(item);
    });
    connect(trainNamesList, &QListWidget::itemChanged, this, [this](QListWidgetItem * item)
    {
        renameCurrentOpticalTrain(item->text());
    });
    connect(trainNamesList, &QListWidget::currentRowChanged, this, [this](int row)
    {
        if (row >= 0)
            selectOpticalTrain(trainNamesList->currentItem());
    });

    m_CheckMissingDevicesTimer.setInterval(5000);
    m_CheckMissingDevicesTimer.setSingleShot(true);
    connect(&m_CheckMissingDevicesTimer, &QTimer::timeout, this, &OpticalTrainManager::checkMissingDevices);

    m_DelegateTimer.setInterval(1000);
    m_DelegateTimer.setSingleShot(true);
    connect(&m_DelegateTimer, &QTimer::timeout, this, [this]()
    {
        if (m_Profile)
            setProfile(m_Profile);
    });

    initModel();
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::initModel()
{
    auto userdb = QSqlDatabase::database(KStarsData::Instance()->userdb()->connectionName());
    m_OpticalTrainsModel = new QSqlTableModel(this, userdb);
    connect(m_OpticalTrainsModel, &QSqlTableModel::dataChanged, this, [this]()
    {
        m_OpticalTrains.clear();
        for (int i = 0; i < m_OpticalTrainsModel->rowCount(); ++i)
        {
            QVariantMap recordMap;
            QSqlRecord record = m_OpticalTrainsModel->record(i);
            for (int j = 0; j < record.count(); j++)
                recordMap[record.fieldName(j)] = record.value(j);

            m_OpticalTrains.append(recordMap);
        }

        m_TrainNames.clear();
        for (auto &oneTrain : m_OpticalTrains)
            m_TrainNames << oneTrain["name"].toString();

        trainNamesList->clear();
        trainNamesList->addItems(m_TrainNames);
        trainNamesList->setEditTriggers(QAbstractItemView::AllEditTriggers);
        emit updated();
    });
}

void OpticalTrainManager::syncDevices()
{
    syncDelegatesToDevices();
    if (m_Profile)
    {
        refreshModel();
        emit updated();
    }
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::refreshModel()
{
    KStars::Instance()->data()->userdb()->GetOpticalTrains(m_Profile->id, m_OpticalTrains);
    m_TrainNames.clear();
    for (auto &oneTrain : m_OpticalTrains)
        m_TrainNames << oneTrain["name"].toString();

    trainNamesList->clear();
    trainNamesList->addItems(m_TrainNames);
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::syncActiveDevices()
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        auto train = oneTrain["name"].toString();
        QSharedPointer<ISD::GenericDevice> device;

        if (getGenericDevice(train, Mount, device))
            syncActiveProperties(oneTrain, device);
        if (getGenericDevice(train, Camera, device))
            syncActiveProperties(oneTrain, device);
        if (getGenericDevice(train, GuideVia, device))
            syncActiveProperties(oneTrain, device);
        if (getGenericDevice(train, Focuser, device))
            syncActiveProperties(oneTrain, device);
        if (getGenericDevice(train, FilterWheel, device))
            syncActiveProperties(oneTrain, device);
        if (getGenericDevice(train, Rotator, device))
            syncActiveProperties(oneTrain, device);
        if (getGenericDevice(train, DustCap, device))
            syncActiveProperties(oneTrain, device);
        if (getGenericDevice(train, LightBox, device))
            syncActiveProperties(oneTrain, device);
    }
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::syncActiveProperties(const QVariantMap &train, const QSharedPointer<ISD::GenericDevice> &device)
{
    auto tvp = device->getProperty("ACTIVE_DEVICES");
    if (!tvp)
        return;

    auto name = train["name"].toString();

    for (auto &it : *tvp.getText())
    {
        QList<QSharedPointer<ISD::GenericDevice>> devs;
        QString elementText = it.getText();
        if (it.isNameMatch("ACTIVE_TELESCOPE"))
        {
            auto activeDevice = train["mount"].toString();
            if (activeDevice == "--")
                elementText.clear();
            else if (activeDevice != elementText)
            {
                QSharedPointer<ISD::GenericDevice> genericDevice;
                if (getGenericDevice(name, Mount, genericDevice))
                    devs.append(genericDevice);
            }
        }
        else if (it.isNameMatch("ACTIVE_DOME"))
        {
            devs = INDIListener::devicesByInterface(INDI::BaseDevice::DOME_INTERFACE);
        }
        else if (it.isNameMatch("ACTIVE_GPS"))
        {
            devs = INDIListener::devicesByInterface(INDI::BaseDevice::GPS_INTERFACE);
        }
        else if (it.isNameMatch("ACTIVE_ROTATOR"))
        {
            auto activeDevice = train["rotator"].toString();
            if (activeDevice == "--")
                elementText.clear();
            else if (activeDevice != elementText)
            {
                QSharedPointer<ISD::GenericDevice> genericDevice;
                if (getGenericDevice(name, Rotator, genericDevice))
                    devs.append(genericDevice);
            }
        }
        else if (it.isNameMatch("ACTIVE_FOCUSER"))
        {
            auto activeDevice = train["focuser"].toString();
            if (activeDevice == "--")
                elementText.clear();
            else if (activeDevice != elementText)
            {
                QSharedPointer<ISD::GenericDevice> genericDevice;
                if (getGenericDevice(name, Focuser, genericDevice))
                    devs.append(genericDevice);
            }
        }
        else if (it.isNameMatch("ACTIVE_FILTER"))
        {
            auto activeDevice = train["filterwheel"].toString();
            if (activeDevice == "--")
                elementText.clear();
            else if (activeDevice != elementText)
            {
                QSharedPointer<ISD::GenericDevice> genericDevice;
                if (getGenericDevice(name, FilterWheel, genericDevice))
                    devs.append(genericDevice);
            }
        }

        if (!devs.empty())
        {
            if (it.getText() != devs.first()->getDeviceName())
            {
                it.setText(devs.first()->getDeviceName().toLatin1().constData());
                device->sendNewProperty(tvp.getText());
            }
        }
        // Clear element if required
        else if (elementText.isEmpty() && !QString(it.getText()).isEmpty())
        {
            it.setText("");
            device->sendNewProperty(tvp.getText());
        }
    }

}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::setProfile(const QSharedPointer<ProfileInfo> &profile)
{
    m_DelegateTimer.stop();

    // Load optical train model
    if (m_Profile != profile)
    {
        m_Profile = profile;
        refreshModel();
    }

    // Are we still updating delegates? If yes, return.
    if (syncDelegatesToDevices())
    {
        m_CheckMissingDevicesTimer.start();

        // Start delegate timer to ensure no more changes are pending.
        m_DelegateTimer.start();

        syncActiveDevices();
    }
    else
    {
        checkOpticalTrains();
    }
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::checkOpticalTrains()
{
    if (m_OpticalTrains.empty())
    {
        generateOpticalTrains();
        refreshModel();
        if (!m_OpticalTrains.empty())
        {
            auto primaryTrainID = m_OpticalTrains[0]["id"].toUInt();
            ProfileSettings::Instance()->setOneSetting(ProfileSettings::PrimaryOpticalTrain, primaryTrainID);
            ProfileSettings::Instance()->setOneSetting(ProfileSettings::CaptureOpticalTrain, primaryTrainID);
            ProfileSettings::Instance()->setOneSetting(ProfileSettings::FocusOpticalTrain, primaryTrainID);
            ProfileSettings::Instance()->setOneSetting(ProfileSettings::MountOpticalTrain, primaryTrainID);
            ProfileSettings::Instance()->setOneSetting(ProfileSettings::AlignOpticalTrain, primaryTrainID);
            ProfileSettings::Instance()->setOneSetting(ProfileSettings::DarkLibraryOpticalTrain, primaryTrainID);
            if (m_OpticalTrains.count() > 1)
                ProfileSettings::Instance()->setOneSetting(ProfileSettings::GuideOpticalTrain, m_OpticalTrains[1]["id"].toInt());
            else
                ProfileSettings::Instance()->setOneSetting(ProfileSettings::GuideOpticalTrain, primaryTrainID);
        }

        emit updated();
        show();
        raise();
        emit configurationRequested(true);
    }
    else
    {
        m_CheckMissingDevicesTimer.start();
        emit updated();
    }
}
////////////////////////////////////////////////////////////////////////////
/// This method tries to guess possible optical train configuration
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::generateOpticalTrains()
{
    // We should have primary train
    addOpticalTrain(0, i18n("Primary"));
    // Check if need secondary train
    if (cameraComboBox->count() > 2)
        addOpticalTrain(1, i18n("Secondary"));
    // Check if need tertiary train
    if (cameraComboBox->count() > 3)
        addOpticalTrain(2, i18n("Tertiary"));
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
QString OpticalTrainManager::addOpticalTrain(uint8_t index, const QString &name)
{
    QVariantMap train;
    train["profile"] = m_Profile->id;
    train["name"] = uniqueTrainName(name);

    train["mount"] = mountComboBox->itemText(mountComboBox->count() - 1);
    train["dustcap"] = dustCapComboBox->itemText(dustCapComboBox->count() - 1);
    train["lightbox"] = lightBoxComboBox->itemText(lightBoxComboBox->count() - 1);
    train["reducer"] = 1.0;
    train["rotator"] = rotatorComboBox->itemText(rotatorComboBox->count() - 1);
    train["focuser"] = focusComboBox->itemText(focusComboBox->count() - 1);
    train["filterwheel"] = filterComboBox->itemText(filterComboBox->count() - 1);
    train["guider"] = guiderComboBox->itemText(guiderComboBox->count() - 1);

    QJsonObject opticalElement;
    if (KStars::Instance()->data()->userdb()->getLastOpticalElement(opticalElement))
        train["scope"] = opticalElement["name"].toString();

    train["camera"] = "--";
    // Primary train
    if (index == 0 && cameraComboBox->count() > 1)
        train["camera"] = cameraComboBox->itemText(1);
    // Any other trains
    else if (index > 0)
    {
        // For 2nd train and beyond, we get the N camera appropiate for this train if one exist.
        // We add + 1 because first element in combobox is "--"
        auto cameraIndex = index + 1;
        if (cameraComboBox->count() >= cameraIndex)
            train["camera"] = cameraComboBox->itemText(cameraIndex);
    }

    KStarsData::Instance()->userdb()->AddOpticalTrain(train);
    return train["name"].toString();
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::addOpticalTrain(const QJsonObject &value)
{
    auto newTrain = value.toVariantMap();
    newTrain["profile"] = m_Profile->id;
    KStarsData::Instance()->userdb()->AddOpticalTrain(newTrain);

    refreshTrains();
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
bool OpticalTrainManager::setOpticalTrainValue(const QString &name, const QString &field, const QVariant &value)
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
        {
            // If value did not change, just return true
            if (oneTrain[field] == value)
                return true;

            // Update field and database.
            oneTrain[field] = value;
            KStarsData::Instance()->userdb()->UpdateOpticalTrain(oneTrain, oneTrain["id"].toInt());
            syncActiveDevices();
            emit updated();
            return true;
        }
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::renameCurrentOpticalTrain(const QString &name)
{
    if (m_CurrentOpticalTrain != nullptr && (*m_CurrentOpticalTrain)["name"] != name)
    {
        auto pos = trainNamesList->currentRow();
        // ensure train name uniqueness
        auto unique = uniqueTrainName(name);
        // update the train database entry
        setOpticalTrainValue((*m_CurrentOpticalTrain)["name"].toString(), "name", unique);
        // propagate the unique name to the current selection
        trainNamesList->currentItem()->setText(unique);
        // refresh the trains
        refreshTrains();
        // refresh selection
        selectOpticalTrain(unique);
        trainNamesList->setCurrentRow(pos);
    }
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
bool OpticalTrainManager::setOpticalTrain(const QJsonObject &train)
{
    auto oneOpticalTrain = getOpticalTrain(train["id"].toInt());
    if (!oneOpticalTrain.empty())
    {
        KStarsData::Instance()->userdb()->UpdateOpticalTrain(train.toVariantMap(), oneOpticalTrain["id"].toInt());
        refreshTrains();
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
bool OpticalTrainManager::removeOpticalTrain(const QString &name)
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
        {
            auto id = oneTrain["id"].toInt();
            KStarsData::Instance()->userdb()->DeleteOpticalTrain(id);
            KStarsData::Instance()->userdb()->DeleteOpticalTrainSettings(id);
            refreshTrains();
            selectOpticalTrain(nullptr);
            return true;
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
bool OpticalTrainManager::syncDelegatesToDevices()
{
    auto changed = false;

    // Mounts
    auto mounts = INDIListener::devicesByInterface(INDI::BaseDevice::TELESCOPE_INTERFACE);
    QStringList values;
    for (auto &oneMount : mounts)
        values << oneMount->getDeviceName();
    changed |= !values.empty() && values != m_MountNames;
    m_MountNames = values;
    auto currentMount = mountComboBox->currentText();
    mountComboBox->clear();
    mountComboBox->addItems(QStringList() << "--" << values);
    mountComboBox->setCurrentText(currentMount);

    // Dust Caps
    values.clear();
    auto dustcaps = INDIListener::devicesByInterface(INDI::BaseDevice::DUSTCAP_INTERFACE);
    for (auto &oneCap : dustcaps)
        values << oneCap->getDeviceName();
    changed |= !values.empty() && values != m_DustCapNames;
    m_DustCapNames = values;
    auto currentCap = dustCapComboBox->currentText();
    dustCapComboBox->clear();
    dustCapComboBox->addItems(QStringList() << "--" << values);
    dustCapComboBox->setCurrentText(currentCap);

    // Light Boxes
    values.clear();
    auto lightboxes = INDIListener::devicesByInterface(INDI::BaseDevice::LIGHTBOX_INTERFACE);
    for (auto &oneBox : lightboxes)
        values << oneBox->getDeviceName();
    changed |= !values.empty() && values != m_LightBoxNames;
    auto currentLightBox = lightBoxComboBox->currentText();
    m_LightBoxNames = values;
    lightBoxComboBox->clear();
    lightBoxComboBox->addItems(QStringList() << "--" << values);
    lightBoxComboBox->setCurrentText(currentLightBox);

    // Scopes
    values = KStars::Instance()->data()->userdb()->getOpticalElementNames();
    changed |= !values.empty() && values != m_ScopeNames;
    m_ScopeNames = values;
    auto currentScope = scopeComboBox->currentText();
    scopeComboBox->clear();
    scopeComboBox->addItems(QStringList() << "--" << values);
    scopeComboBox->setCurrentText(currentScope);

    // Rotators
    values.clear();
    auto rotators = INDIListener::devicesByInterface(INDI::BaseDevice::ROTATOR_INTERFACE);
    for (auto &oneRotator : rotators)
        values << oneRotator->getDeviceName();
    changed |= !values.empty() && values != m_RotatorNames;
    m_RotatorNames = values;
    auto currentRotator = rotatorComboBox->currentText();
    rotatorComboBox->clear();
    rotatorComboBox->addItems(QStringList() << "--" << values);
    rotatorComboBox->setCurrentText(currentRotator);

    // Focusers
    values.clear();
    auto focusers = INDIListener::devicesByInterface(INDI::BaseDevice::FOCUSER_INTERFACE);
    for (auto &oneFocuser : focusers)
        values << oneFocuser->getDeviceName();
    changed |= !values.empty() && values != m_FocuserNames;
    m_FocuserNames = values;
    auto currentFocuser = focusComboBox->currentText();
    focusComboBox->clear();
    focusComboBox->addItems(QStringList() << "--" << values);
    focusComboBox->setCurrentText(currentFocuser);

    // Filter Wheels
    values.clear();
    auto filterwheels = INDIListener::devicesByInterface(INDI::BaseDevice::FILTER_INTERFACE);
    for (auto &oneFilterWheel : filterwheels)
        values << oneFilterWheel->getDeviceName();
    changed |= !values.empty() && values != m_FilterWheelNames;
    m_FilterWheelNames = values;
    auto currentFilter = filterComboBox->currentText();
    filterComboBox->clear();
    filterComboBox->addItems(QStringList() << "--" << values);
    filterComboBox->setCurrentText(currentFilter);

    // Cameras
    values.clear();
    auto cameras = INDIListener::devicesByInterface(INDI::BaseDevice::CCD_INTERFACE);
    for (auto &oneCamera : cameras)
        values << oneCamera->getDeviceName();
    changed |= !values.empty() && values != m_CameraNames;
    m_CameraNames = values;
    auto currentCamera = cameraComboBox->currentText();
    cameraComboBox->clear();
    cameraComboBox->addItems(QStringList() << "--" << values);
    cameraComboBox->setCurrentText(currentCamera);

    // Guiders
    values.clear();
    auto guiders = INDIListener::devicesByInterface(INDI::BaseDevice::GUIDER_INTERFACE);
    for (auto &oneGuider : guiders)
        values << oneGuider->getDeviceName();
    changed |= !values.empty() && values != m_GuiderNames;
    m_GuiderNames = values;
    auto currentGuider = guiderComboBox->currentText();
    guiderComboBox->clear();
    guiderComboBox->addItems(QStringList() << "--" << values);
    guiderComboBox->setCurrentText(currentGuider);

    return changed;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
QString OpticalTrainManager::uniqueTrainName(QString name)
{
    QString result = name;
    int nr = 1;
    while (m_TrainNames.contains(result))
        result = QString("%1 (%2)").arg(name).arg(nr++);

    return result;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
bool OpticalTrainManager::selectOpticalTrain(QListWidgetItem *item)
{
    if (item != nullptr && selectOpticalTrain(item->text()))
    {
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
QString OpticalTrainManager::findTrainContainingDevice(const QString &name, Role role)
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        auto train = oneTrain["name"].toString();

        switch (role)
        {
            case Mount:
                if (oneTrain["mount"].toString() == name)
                    return train;
                break;
            case Camera:
                if (oneTrain["camera"].toString() == name)
                    return train;
                break;
            case Rotator:
                if (oneTrain["rotator"].toString() == name)
                    return train;
                break;
            case GuideVia:
                if (oneTrain["guider"].toString() == name)
                    return train;
                break;
            case DustCap:
                if (oneTrain["dustcap"].toString() == name)
                    return train;
                break;
            case Scope:
                if (oneTrain["scope"].toString() == name)
                    return train;
                break;
            case FilterWheel:
                if (oneTrain["filterwheel"].toString() == name)
                    return train;
                break;
            case Focuser:
                if (oneTrain["focuser"].toString() == name)
                    return train;
                break;
            case Reducer:
                if (oneTrain["reducer"].toString() == name)
                    return train;
                break;
            case LightBox:
                if (oneTrain["lightbox"].toString() == name)
                    return train;
                break;
        }

    }

    return QString();
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
bool OpticalTrainManager::selectOpticalTrain(const QString &name)
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
        {
            m_Persistent = false;
            m_CurrentOpticalTrain = &oneTrain;
            mountComboBox->setCurrentText(oneTrain["mount"].toString());
            dustCapComboBox->setCurrentText(oneTrain["dustcap"].toString());
            lightBoxComboBox->setCurrentText(oneTrain["lightbox"].toString());
            scopeComboBox->setCurrentText(oneTrain["scope"].toString());
            reducerSpinBox->setValue(oneTrain["reducer"].toDouble());
            rotatorComboBox->setCurrentText(oneTrain["rotator"].toString());
            focusComboBox->setCurrentText(oneTrain["focuser"].toString());
            filterComboBox->setCurrentText(oneTrain["filterwheel"].toString());
            cameraComboBox->setCurrentText(oneTrain["camera"].toString());
            guiderComboBox->setCurrentText(oneTrain["guider"].toString());
            removeB->setEnabled(m_OpticalTrains.length() > 1);
            trainConfigBox->setEnabled(true);
            m_Persistent = true;
            return true;
        }
    }

    // none found
    m_Persistent = false;
    m_CurrentOpticalTrain = nullptr;
    mountComboBox->setCurrentText("--");
    dustCapComboBox->setCurrentText("--");
    lightBoxComboBox->setCurrentText("--");
    scopeComboBox->setCurrentText("--");
    reducerSpinBox->setValue(1.0);
    rotatorComboBox->setCurrentText("--");
    focusComboBox->setCurrentText("--");
    filterComboBox->setCurrentText("--");
    cameraComboBox->setCurrentText("--");
    guiderComboBox->setCurrentText("--");
    removeB->setEnabled(false);
    trainConfigBox->setEnabled(false);
    m_Persistent = true;
    return false;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::openEditor(const QString &name)
{
    selectOpticalTrain(name);
    QList<QListWidgetItem*> matches = trainNamesList->findItems(name, Qt::MatchExactly);
    if (matches.count() > 0)
        trainNamesList->setCurrentItem(matches.first());
    emit configurationRequested(true);
    show();
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
bool OpticalTrainManager::getGenericDevice(const QString &train, Role role, QSharedPointer<ISD::GenericDevice> &generic)
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == train)
        {
            switch (role)
            {
                case Mount:
                    return INDIListener::findDevice(oneTrain["mount"].toString(), generic);
                case Camera:
                    return INDIListener::findDevice(oneTrain["camera"].toString(), generic);
                case Rotator:
                    return INDIListener::findDevice(oneTrain["rotator"].toString(), generic);
                case GuideVia:
                    return INDIListener::findDevice(oneTrain["guider"].toString(), generic);
                case DustCap:
                    return INDIListener::findDevice(oneTrain["dustcap"].toString(), generic);
                case FilterWheel:
                    return INDIListener::findDevice(oneTrain["filterwheel"].toString(), generic);
                case Focuser:
                    return INDIListener::findDevice(oneTrain["focuser"].toString(), generic);
                case LightBox:
                    return INDIListener::findDevice(oneTrain["lightbox"].toString(), generic);
                default:
                    break;
            }
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
ISD::Mount *OpticalTrainManager::getMount(const QString &name)
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
        {
            QSharedPointer<ISD::GenericDevice> generic;
            if (INDIListener::findDevice(oneTrain["mount"].toString(), generic))
                return generic->getMount();
        }
    }

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
ISD::DustCap *OpticalTrainManager::getDustCap(const QString &name)
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
        {
            QSharedPointer<ISD::GenericDevice> generic;
            if (INDIListener::findDevice(oneTrain["dustcap"].toString(), generic))
                return generic->getDustCap();
        }
    }

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
ISD::LightBox *OpticalTrainManager::getLightBox(const QString &name)
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
        {
            QSharedPointer<ISD::GenericDevice> generic;
            if (INDIListener::findDevice(oneTrain["lightbox"].toString(), generic))
                return generic->getLightBox();
        }
    }

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
QJsonObject OpticalTrainManager::getScope(const QString &name)
{
    QJsonObject oneOpticalElement;
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
        {
            if (KStars::Instance()->data()->userdb()->getOpticalElementByName(oneTrain["scope"].toString(), oneOpticalElement))
                return oneOpticalElement;
        }
    }

    return oneOpticalElement;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
double OpticalTrainManager::getReducer(const QString &name)
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
            return oneTrain["reducer"].toDouble();
    }

    return 1;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
ISD::Rotator *OpticalTrainManager::getRotator(const QString &name)
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
        {
            QSharedPointer<ISD::GenericDevice> generic;
            if (INDIListener::findDevice(oneTrain["rotator"].toString(), generic))
                return generic->getRotator();
        }
    }

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
ISD::Focuser *OpticalTrainManager::getFocuser(const QString &name)
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
        {
            QSharedPointer<ISD::GenericDevice> generic;
            if (INDIListener::findDevice(oneTrain["focuser"].toString(), generic))
                return generic->getFocuser();
        }
    }

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
ISD::FilterWheel *OpticalTrainManager::getFilterWheel(const QString &name)
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
        {
            QSharedPointer<ISD::GenericDevice> generic;
            if (INDIListener::findDevice(oneTrain["filterwheel"].toString(), generic))
                return generic->getFilterWheel();
        }
    }

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
ISD::Camera *OpticalTrainManager::getCamera(const QString &name)
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
        {
            QSharedPointer<ISD::GenericDevice> generic;
            if (INDIListener::findDevice(oneTrain["camera"].toString(), generic))
                return generic->getCamera();
        }
    }

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
ISD::Guider *OpticalTrainManager::getGuider(const QString &name)
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
        {
            QSharedPointer<ISD::GenericDevice> generic;
            if (INDIListener::findDevice(oneTrain["guider"].toString(), generic))
                return generic->getGuider();
        }
    }

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
ISD::AdaptiveOptics *OpticalTrainManager::getAdaptiveOptics(const QString &name)
{
    // FIXME not implmeneted yet.
    // Need to add to database later
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
        {
            QSharedPointer<ISD::GenericDevice> generic;
            if (INDIListener::findDevice(oneTrain["adaptiveoptics"].toString(), generic))
                return generic->getAdaptiveOptics();
        }
    }

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
const QVariantMap OpticalTrainManager::getOpticalTrain(uint8_t id) const
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["id"].toInt() == id)
            return oneTrain;
    }

    return QVariantMap();
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
bool OpticalTrainManager::exists(uint8_t id) const
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["id"].toInt() == id)
            return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
const QVariantMap OpticalTrainManager::getOpticalTrain(const QString &name) const
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
            return oneTrain;
    }

    return QVariantMap();
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::refreshTrains()
{
    refreshModel();
    emit updated();
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::refreshOpticalElements()
{
    m_ScopeNames = KStars::Instance()->data()->userdb()->getOpticalElementNames();
    syncDelegatesToDevices();
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
int OpticalTrainManager::id(const QString &name) const
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
            return oneTrain["id"].toUInt();
    }

    return -1;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
QString OpticalTrainManager::name(int id) const
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["id"].toInt() == id)
            return oneTrain["name"].toString();
    }

    return QString();
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::checkMissingDevices()
{
    // Double check the sanity of the train. If devices are added or missing, then we need to show it to alert the user.
    auto devices = getMissingDevices();
    if (!devices.empty())
    {
        if (devices.count() == 1)
        {
            KSNotification::event(QLatin1String("IndiServerMessage"),
                                  i18n("Missing device detected (%1). Please reconfigure the optical trains before proceeding any further.",
                                       devices.first()),
                                  KSNotification::General, KSNotification::Warn);
        }
        else
        {
            KSNotification::event(QLatin1String("IndiServerMessage"),
                                  i18n("Missing devices detected (%1). Please reconfigure the optical trains before proceeding any further.",
                                       devices.join(", ")),
                                  KSNotification::General, KSNotification::Warn);
        }
        show();
        raise();
        emit configurationRequested(true);
    }
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
QStringList OpticalTrainManager::getMissingDevices() const
{
    auto missing = QStringList();
    for (auto &oneTrain : m_OpticalTrains)
    {
        auto mount = oneTrain["mount"].toString();
        if (mount != "--" && m_MountNames.contains(mount) == false)
            missing << mount;

        auto camera = oneTrain["camera"].toString();
        if (camera != "--" && m_CameraNames.contains(camera) == false)
            missing << camera;

        auto dustcap = oneTrain["dustcap"].toString();
        if (dustcap != "--" && m_DustCapNames.contains(dustcap) == false)
            missing << dustcap;

        auto lightbox = oneTrain["lightbox"].toString();
        if (lightbox != "--" && m_LightBoxNames.contains(lightbox) == false)
            missing << lightbox;

        auto focuser = oneTrain["focuser"].toString();
        if (focuser != "--" && m_FocuserNames.contains(focuser) == false)
            missing << focuser;

        auto filterwheel = oneTrain["filterwheel"].toString();
        if (filterwheel != "--" && m_FilterWheelNames.contains(filterwheel) == false)
            missing << filterwheel;

        auto guider = oneTrain["guider"].toString();
        if (guider != "--" && m_GuiderNames.contains(guider) == false)
            missing << guider;

    }

    return missing;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void Ekos::OpticalTrainManager::updateOpticalTrainValue(QComboBox *cb, const QString &element)
{
    if (trainNamesList->currentItem() != nullptr && m_Persistent == true)
        setOpticalTrainValue(trainNamesList->currentItem()->text(), element, cb->currentText());
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::updateOpticalTrainValue(double value, const QString &element)
{
    if (trainNamesList->currentItem() != nullptr && m_Persistent == true)
        setOpticalTrainValue(trainNamesList->currentItem()->text(), element, value);

}

////////////////////////////////////////////////////////////////////////////
/// Reset optical train to default values.
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::reset()
{
    if (m_CurrentOpticalTrain != nullptr)
    {
        auto id = m_CurrentOpticalTrain->value("id");
        auto name = m_CurrentOpticalTrain->value("name");
        int row = trainNamesList->currentRow();
        m_CurrentOpticalTrain->clear();

        m_CurrentOpticalTrain->insert("id", id);
        m_CurrentOpticalTrain->insert("name", name);
        m_CurrentOpticalTrain->insert("mount", "--");
        m_CurrentOpticalTrain->insert("camera", "--");
        m_CurrentOpticalTrain->insert("rotator", "--");
        m_CurrentOpticalTrain->insert("guider", "--");
        m_CurrentOpticalTrain->insert("dustcap", "--");
        m_CurrentOpticalTrain->insert("scope", "--");
        m_CurrentOpticalTrain->insert("filterwheel", "--");
        m_CurrentOpticalTrain->insert("focuser", "--");
        m_CurrentOpticalTrain->insert("reducer", 1);
        m_CurrentOpticalTrain->insert("lightbox", "--");

        KStarsData::Instance()->userdb()->UpdateOpticalTrain(*m_CurrentOpticalTrain, id.toInt());
        refreshTrains();
        selectOpticalTrain(name.toString());
        trainNamesList->setCurrentRow(row);
    }
}

}
