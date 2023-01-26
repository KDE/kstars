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
        addOpticalTrain(false, i18n("New Train"));
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

    m_CheckMissingDevicesTimer.setInterval(2000);
    m_CheckMissingDevicesTimer.setSingleShot(true);
    connect(&m_CheckMissingDevicesTimer, &QTimer::timeout, this, &OpticalTrainManager::checkMissingDevices);
    initModel();
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::initModel()
{
    QSqlDatabase userdb = QSqlDatabase::cloneDatabase(KStarsData::Instance()->userdb()->GetDatabase(), "opticaltrains_db");
    userdb.open();
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
void OpticalTrainManager::setProfile(const QSharedPointer<ProfileInfo> &profile)
{
    // Are we still updating delegates? If yes, return.
    if (syncDelegatesToDevices())
    {
        m_CheckMissingDevicesTimer.start();
        return;
    }

    // Once we're done, let's continue with train generation.
    if (m_Profile != profile)
    {
        m_Profile = profile;
        refreshModel();
    }

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
    addOpticalTrain(true, i18n("Primary"));
    // Check if need secondary train
    if (m_CameraNames.count() > 2)
        addOpticalTrain(false, i18n("Secondary"));
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
QString OpticalTrainManager::addOpticalTrain(bool main, const QString &name)
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

    if (main)
        train["camera"] = cameraComboBox->itemText(cameraComboBox->count() > 1 ? 1 : 0);
    else
        train["camera"] = cameraComboBox->itemText(cameraComboBox->count() > 2 ? 2 : 0);

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
    mountComboBox->clear();
    mountComboBox->addItems(QStringList() << "--" << values);

    // Dust Caps
    values.clear();
    auto dustcaps = INDIListener::devicesByInterface(INDI::BaseDevice::DUSTCAP_INTERFACE);
    for (auto &oneCap : dustcaps)
        values << oneCap->getDeviceName();
    changed |= !values.empty() && values != m_DustCapNames;
    m_DustCapNames = values;
    dustCapComboBox->clear();
    dustCapComboBox->addItems(QStringList() << "--" << values);

    // Light Boxes
    values.clear();
    auto lightboxes = INDIListener::devicesByInterface(INDI::BaseDevice::LIGHTBOX_INTERFACE);
    for (auto &oneBox : lightboxes)
        values << oneBox->getDeviceName();
    changed |= !values.empty() && values != m_LightBoxNames;
    m_LightBoxNames = values;
    lightBoxComboBox->clear();
    lightBoxComboBox->addItems(QStringList() << "--" << values);

    // Scopes
    values = KStars::Instance()->data()->userdb()->getOpticalElementNames();
    changed |= !values.empty() && values != m_ScopeNames;
    m_ScopeNames = values;
    scopeComboBox->clear();
    scopeComboBox->addItems(QStringList() << "--" << values);

    // Rotators
    values.clear();
    auto rotators = INDIListener::devicesByInterface(INDI::BaseDevice::ROTATOR_INTERFACE);
    for (auto &oneRotator : rotators)
        values << oneRotator->getDeviceName();
    changed |= !values.empty() && values != m_RotatorNames;
    m_RotatorNames = values;
    rotatorComboBox->clear();
    rotatorComboBox->addItems(QStringList() << "--" << values);

    // Focusers
    values.clear();
    auto focusers = INDIListener::devicesByInterface(INDI::BaseDevice::FOCUSER_INTERFACE);
    for (auto &oneFocuser : focusers)
        values << oneFocuser->getDeviceName();
    changed |= !values.empty() && values != m_FocuserNames;
    m_FocuserNames = values;
    focusComboBox->clear();
    focusComboBox->addItems(QStringList() << "--" << values);

    // Filter Wheels
    values.clear();
    auto filterwheels = INDIListener::devicesByInterface(INDI::BaseDevice::FILTER_INTERFACE);
    for (auto &oneFilterWheel : filterwheels)
        values << oneFilterWheel->getDeviceName();
    changed |= !values.empty() && values != m_FilterWheelNames;
    m_FilterWheelNames = values;
    filterComboBox->clear();
    filterComboBox->addItems(QStringList() << "--" << values);

    // Cameras
    values.clear();
    auto cameras = INDIListener::devicesByInterface(INDI::BaseDevice::CCD_INTERFACE);
    for (auto &oneCamera : cameras)
        values << oneCamera->getDeviceName();
    changed |= !values.empty() && values != m_CameraNames;
    m_CameraNames = values;
    cameraComboBox->clear();
    cameraComboBox->addItems(QStringList() << "--" << values);

    // Guiders
    values.clear();
    auto guiders = INDIListener::devicesByInterface(INDI::BaseDevice::GUIDER_INTERFACE);
    for (auto &oneGuider : guiders)
        values << oneGuider->getDeviceName();
    changed |= !values.empty() && values != m_GuiderNames;
    m_GuiderNames = values;
    guiderComboBox->clear();
    guiderComboBox->addItems(QStringList() << "--" << values);

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
            removeB->setEnabled(true);
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

    // After list is updated, need to refresh scopeCombo box.
    auto currentOpticalElement = scopeComboBox->currentText();
    scopeComboBox->clear();
    scopeComboBox->addItems(QStringList() << "--" << m_ScopeNames);
    scopeComboBox->setCurrentText(currentOpticalElement);
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
        setOpticalTrainValue(trainNamesList->currentItem()->text(), element, QString("%0.2d").arg(value));

}

}
