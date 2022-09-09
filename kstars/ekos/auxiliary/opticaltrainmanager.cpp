/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opticaltrainmanager.h"
#include <kstars_debug.h>

#include "kstarsdata.h"
#include "kstars.h"
#include "indi/indilistener.h"
#include "ekos/auxiliary/profilesettings.h"
#include "ekos/auxiliary/tabledelegate.h"
#include "oal/equipmentwriter.h"
#include "ekos/manager.h"

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
OpticalTrainManager::OpticalTrainManager() : QDialog(Ekos::Manager::Instance())
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    setupUi(this);

    // Delegates

    // Mount Combo
    m_MountDelegate = new ComboDelegate(trainView);
    trainView->setItemDelegateForColumn(Mount, m_MountDelegate);

    // DustCap Combo
    m_DustCapDelegate = new ComboDelegate(trainView);
    trainView->setItemDelegateForColumn(DustCap, m_DustCapDelegate);

    // Light Box
    m_LightBoxDelegate = new ComboDelegate(trainView);
    trainView->setItemDelegateForColumn(LightBox, m_LightBoxDelegate);

    // Scope / Lens
    m_ScopeDelegate = new ComboDelegate(trainView);
    trainView->setItemDelegateForColumn(Scope, m_ScopeDelegate);

    // Reducer
    m_ReducerDelegate = new DoubleDelegate(trainView, 0.1, 1, 0.1);
    trainView->setItemDelegateForColumn(Reducer, m_ReducerDelegate);

    // Rotator
    m_RotatorDelegate = new ComboDelegate(trainView);
    trainView->setItemDelegateForColumn(Rotator, m_RotatorDelegate);

    // Focuser
    m_FocuserDelegate = new ComboDelegate(trainView);
    trainView->setItemDelegateForColumn(Focuser, m_FocuserDelegate);

    // Filter Wheel
    m_FilterWheelDelegate = new ComboDelegate(trainView);
    trainView->setItemDelegateForColumn(FilterWheel, m_FilterWheelDelegate);

    // Camera
    m_CameraDelegate = new ComboDelegate(trainView);
    trainView->setItemDelegateForColumn(Camera, m_CameraDelegate);

    // Guider
    m_GuiderDelegate = new ComboDelegate(trainView);
    trainView->setItemDelegateForColumn(Guider, m_GuiderDelegate);

    connect(addB, &QPushButton::clicked, this, [this]()
    {
        addOpticalTrain(false, i18n("New Train"));
        m_OpticalTrainsModel->select();

        refreshModel();
    });

    connect(removeB, &QPushButton::clicked, this, [this]()
    {
        int row = trainView->currentIndex().row();
        m_OpticalTrainsModel->removeRow(row);
        m_OpticalTrainsModel->submitAll();

        refreshModel();

        removeB->setEnabled(false);
    });

    connect(trainView, &QTableView::clicked, this, [this](const QModelIndex & index)
    {
        removeB->setEnabled(index.row() > 0);
    });

    connect(opticalElementsB, &QPushButton::clicked, this, [this]()
    {
        QScopedPointer<EquipmentWriter> writer(new EquipmentWriter());
        writer->loadEquipment();
        writer->exec();
        refreshOpticalElements();
    });

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
    connect(m_OpticalTrainsModel, &QSqlTableModel::dataChanged, this, &OpticalTrainManager::refreshTrains);
    trainView->setModel(m_OpticalTrainsModel);
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
    m_OpticalTrainsModel->setTable("opticaltrains");
    m_OpticalTrainsModel->setFilter(QString("profile=%1").arg(m_Profile->id));
    m_OpticalTrainsModel->select();
    m_OpticalTrainsModel->setEditStrategy(QSqlTableModel::OnFieldChange);

    m_OpticalTrainsModel->setHeaderData(Name, Qt::Horizontal, i18n("Name"));
    m_OpticalTrainsModel->setHeaderData(Mount, Qt::Horizontal, i18n("Mount"));

    m_OpticalTrainsModel->setHeaderData(DustCap, Qt::Horizontal, i18n("Telescope cover"), Qt::ToolTipRole);
    m_OpticalTrainsModel->setHeaderData(DustCap, Qt::Horizontal, i18n("Dust Cap"));

    m_OpticalTrainsModel->setHeaderData(LightBox, Qt::Horizontal, i18n("Flat field light source"), Qt::ToolTipRole);
    m_OpticalTrainsModel->setHeaderData(LightBox, Qt::Horizontal, i18n("Light Box"));

    m_OpticalTrainsModel->setHeaderData(Scope, Qt::Horizontal, i18n("Telescope or Lens"), Qt::ToolTipRole);
    m_OpticalTrainsModel->setHeaderData(Scope, Qt::Horizontal, i18n("Scope/Lens"));

    m_OpticalTrainsModel->setHeaderData(Reducer, Qt::Horizontal, i18n("Reducer"));
    m_OpticalTrainsModel->setHeaderData(Rotator, Qt::Horizontal, i18n("Rotator"));
    m_OpticalTrainsModel->setHeaderData(Focuser, Qt::Horizontal, i18n("Focuser"));
    m_OpticalTrainsModel->setHeaderData(FilterWheel, Qt::Horizontal, i18n("Filter Wheel"));
    m_OpticalTrainsModel->setHeaderData(Camera, Qt::Horizontal, i18n("Camera"));

    m_OpticalTrainsModel->setHeaderData(Guider, Qt::Horizontal, i18n("Guider"));
    m_OpticalTrainsModel->setHeaderData(Guider, Qt::Horizontal, i18n("Device receiving guiding correction pulses"),
                                        Qt::ToolTipRole);

    trainView->hideColumn(ID);
    trainView->hideColumn(Profile);

    KStars::Instance()->data()->userdb()->GetOpticalTrains(m_Profile->id, m_OpticalTrains);
    m_TrainNames.clear();
    for (auto &oneTrain : m_OpticalTrains)
        m_TrainNames << oneTrain["name"].toString();
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::setProfile(const QSharedPointer<ProfileInfo> &profile)
{
    m_Profile = profile;
    syncDelegatesToDevices();
    refreshModel();

    if (m_OpticalTrains.empty())
    {
        generateOpticalTrains();
        refreshModel();
        if (!m_OpticalTrains.empty())
        {
            auto primaryTrainName = m_OpticalTrains[0]["name"].toString();
            ProfileSettings::Instance()->setOneSetting(ProfileSettings::PrimaryOpticalTrain, primaryTrainName);
            ProfileSettings::Instance()->setOneSetting(ProfileSettings::CaptureOpticalTrain, primaryTrainName);
            ProfileSettings::Instance()->setOneSetting(ProfileSettings::FocusOpticalTrain, primaryTrainName);
            ProfileSettings::Instance()->setOneSetting(ProfileSettings::MountOpticalTrain, primaryTrainName);
            ProfileSettings::Instance()->setOneSetting(ProfileSettings::AlignOpticalTrain, primaryTrainName);
            if (m_OpticalTrains.count() > 1)
                ProfileSettings::Instance()->setOneSetting(ProfileSettings::GuideOpticalTrain, m_OpticalTrains[1]["name"].toString());
            else
                ProfileSettings::Instance()->setOneSetting(ProfileSettings::GuideOpticalTrain, primaryTrainName);
        }

        emit updated();
        show();
        raise();
        emit configurationRequested();
    }
    else
        emit updated();
}
////////////////////////////////////////////////////////////////////////////
/// This method tries to guess possible optical train configuration
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::generateOpticalTrains()
{
    // We should have primary train
    addOpticalTrain(true, i18n("Primary"));
    // Check if need secondary train
    if (m_CameraDelegate->values().count() > 1)
        addOpticalTrain(false, i18n("Secondary"));
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::addOpticalTrain(bool main, const QString &name)
{
    QVariantMap train;
    train["profile"] = m_Profile->id;
    train["name"] = name;

    auto mounts = m_MountDelegate->values();
    train["mount"] = mounts.count() > 1 ? mounts[1] : mounts[0];

    auto dustcaps = m_DustCapDelegate->values();
    train["dustcap"] = dustcaps.count() > 1 ? dustcaps[1] : dustcaps[0];

    auto lightboxes = m_LightBoxDelegate->values();
    train["lightbox"] = lightboxes.count() > 1 ? lightboxes[1] : lightboxes[0];

    QJsonObject opticalElement;
    if (KStars::Instance()->data()->userdb()->getLastOpticalElement(opticalElement))
        train["scope"] = opticalElement["name"].toString();

    train["reducer"] = 1.0;

    auto rotators = m_RotatorDelegate->values();
    train["rotator"] = rotators.count() > 1 ? rotators[1] : rotators[0];

    auto focusers = m_FocuserDelegate->values();
    train["focuser"] = focusers.count() > 1 ? focusers[1] : focusers[0];

    auto filterwheels = m_FilterWheelDelegate->values();
    train["filterwheel"] = filterwheels.count() > 1 ? filterwheels[1] : filterwheels[0];

    auto cameras = m_CameraDelegate->values();
    if (main)
        train["camera"] = cameras.count() > 1 ? cameras[1] : cameras[0];
    else
        train["camera"] = cameras.count() > 2 ? cameras[2] : cameras[0];

    auto guiders = m_GuiderDelegate->values();
    train["guider"] = guiders.count() > 1 ? guiders[1] : guiders[0];

    KStarsData::Instance()->userdb()->AddOpticalTrain(train);

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
    auto oneOpticalTrain = getOpticalTrain(name);
    if (!oneOpticalTrain.empty())
    {
        oneOpticalTrain[field] = value;
        KStarsData::Instance()->userdb()->UpdateOpticalTrain(oneOpticalTrain, oneOpticalTrain["id"].toInt());
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
bool OpticalTrainManager::setOpticalTrain(const QJsonObject &train)
{
    auto oneOpticalTrain = getOpticalTrain(train["name"].toString());
    if (!oneOpticalTrain.empty())
    {
        KStarsData::Instance()->userdb()->UpdateOpticalTrain(oneOpticalTrain, oneOpticalTrain["id"].toInt());
        refreshTrains();
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
bool OpticalTrainManager::removeOpticalTrain(uint32_t id)
{
    auto exists = std::any_of(m_OpticalTrains.begin(), m_OpticalTrains.end(), [id](auto & oneTrain)
    {
        return oneTrain["id"].toInt() == id;
    });

    if (exists)
    {
        KStarsData::Instance()->userdb()->DeleteOpticalTrain(id);
        KStarsData::Instance()->userdb()->DeleteOpticalTrainSettings(id);
        refreshTrains();
        return true;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
void OpticalTrainManager::syncDelegatesToDevices()
{
    // Mounts
    auto mounts = INDIListener::devicesByInterface(INDI::BaseDevice::TELESCOPE_INTERFACE);
    QStringList values;
    for (auto &oneMount : mounts)
        values << oneMount->getDeviceName();
    m_MountDelegate->setValues(values);

    // Dust Caps
    values.clear();
    auto dustcaps = INDIListener::devicesByInterface(INDI::BaseDevice::DUSTCAP_INTERFACE);
    for (auto &oneCap : dustcaps)
        values << oneCap->getDeviceName();
    m_DustCapDelegate->setValues(values);

    // Light Boxes
    values.clear();
    auto lightboxes = INDIListener::devicesByInterface(INDI::BaseDevice::LIGHTBOX_INTERFACE);
    for (auto &oneBox : lightboxes)
        values << oneBox->getDeviceName();
    m_LightBoxDelegate->setValues(values);

    // Scopes
    m_ScopeDelegate->setValues(KStars::Instance()->data()->userdb()->getOpticalElementNames());

    // Rotators
    values.clear();
    auto rotators = INDIListener::devicesByInterface(INDI::BaseDevice::ROTATOR_INTERFACE);
    for (auto &oneRotator : rotators)
        values << oneRotator->getDeviceName();
    m_RotatorDelegate->setValues(values);

    // Focusers
    values.clear();
    auto focusers = INDIListener::devicesByInterface(INDI::BaseDevice::FOCUSER_INTERFACE);
    for (auto &oneFocuser : focusers)
        values << oneFocuser->getDeviceName();
    m_FocuserDelegate->setValues(values);

    // Filter Wheels
    values.clear();
    auto filterwheels = INDIListener::devicesByInterface(INDI::BaseDevice::FILTER_INTERFACE);
    for (auto &oneFilterWheel : filterwheels)
        values << oneFilterWheel->getDeviceName();
    m_FilterWheelDelegate->setValues(values);

    // Cameras
    values.clear();
    auto cameras = INDIListener::devicesByInterface(INDI::BaseDevice::CCD_INTERFACE);
    for (auto &oneCamera : cameras)
        values << oneCamera->getDeviceName();
    m_CameraDelegate->setValues(values);

    // Guiders
    values.clear();
    auto guiders = INDIListener::devicesByInterface(INDI::BaseDevice::GUIDER_INTERFACE);
    for (auto &oneGuider : guiders)
        values << oneGuider->getDeviceName();
    m_GuiderDelegate->setValues(values);
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
    m_ScopeDelegate->setValues(KStars::Instance()->data()->userdb()->getOpticalElementNames());
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
int OpticalTrainManager::id(const QString &name)
{
    for (auto &oneTrain : m_OpticalTrains)
    {
        if (oneTrain["name"].toString() == name)
            return oneTrain["id"].toUInt();
    }

    return -1;
}

}
