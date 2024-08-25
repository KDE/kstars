/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "focusmodule.h"
#include "focus.h"

#include "Options.h"
#include "auxiliary/ksmessagebox.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "kstarsdata.h"
#include "kspaths.h"

#include "ekos_focus_debug.h"

#define TAB_BUTTON_SIZE 20

namespace Ekos
{

FocusModule::FocusModule()
{
    setupUi(this);

    // Adding the "New Tab" tab
    QWidget *newTab = new QWidget;
    QPushButton *addButton = new QPushButton;
    addButton->setIcon(QIcon::fromTheme("list-add"));
    addButton->setFixedSize(TAB_BUTTON_SIZE, TAB_BUTTON_SIZE);
    addButton->setToolTip(i18n("<p>Add additional focuser</p><p><b>WARNING</b>: This feature is experimental!</p>"));
    connect(addButton, &QPushButton::clicked, this, [this]()
    {
        FocusModule::addFocuser();
    });

    focusTabs->addTab(newTab, "");
    focusTabs->tabBar()->setTabButton(0, QTabBar::RightSide, addButton);

    // Create an autofocus CSV file, dated at startup time
    m_FocusLogFileName = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("focuslogs/autofocus-" +
                         QDateTime::currentDateTime().toString("yyyy-MM-ddThh-mm-ss") + ".txt");
    m_FocusLogFile.setFileName(m_FocusLogFileName);

    // Create main focuser
    addFocuser();
}

FocusModule::~FocusModule()
{
    m_FocusLogFile.close();
}

QSharedPointer<Focus> &FocusModule::focuser(int i)
{
    if (i < m_Focusers.count())
        return m_Focusers[i];
    else
    {
        qCWarning(KSTARS_EKOS_FOCUS) << "Unknown focuser ID:" << i;
        return m_Focusers[0];
    }

}

QSharedPointer<Focus> FocusModule::mainFocuser()
{
    if (m_Focusers.size() <= 0)
    {
        QSharedPointer<Focus> newFocuser;
        newFocuser.reset(new Focus(0));
        m_Focusers.append(newFocuser);
    }
    return m_Focusers[0];
}

void FocusModule::checkFocus(double requiredHFR, const QString &trainname)
{
    bool found = false;
    // publish to all known focusers using the same optical train (should be only one)
    for (auto focuser : m_Focusers)
        if (trainname == "" || focuser->opticalTrain() == trainname)
        {
            focuser->checkFocus(requiredHFR);
            found = true;
        }

    if (!found)
    {
        QSharedPointer newFocuser = addFocuser(trainname);
        newFocuser->checkFocus(requiredHFR);
    }
}

void FocusModule::runAutoFocus(const AutofocusReason autofocusReason, const QString &reasonInfo, const QString &trainname)
{
    bool found = false;
    // publish to all known focusers using the same optical train (should be only one)
    for (auto focuser : m_Focusers)
        if (trainname == "" || focuser->opticalTrain() == trainname)
        {
            focuser->runAutoFocus(autofocusReason, reasonInfo);
            found = true;
        }

    if (!found)
    {
        QSharedPointer newFocuser = addFocuser(trainname);
        newFocuser->runAutoFocus(autofocusReason, reasonInfo);
    }
}

void FocusModule::resetFocuser(const QString &trainname)
{
    bool found = false;
    // publish to all known focusers using the same optical train (should be only one)
    for (auto focuser : m_Focusers)
        if (trainname == "" || focuser->opticalTrain() == trainname)
        {
            focuser->resetFocuser();
            found = true;
        }

    if (!found)
    {
        QSharedPointer newFocuser = addFocuser(trainname);
        newFocuser->resetFocuser();
    }
}

void FocusModule::abort(const QString &trainname)
{
    bool found = false;
    // publish to all known focusers using the same optical train (should be only one)
    for (auto focuser : m_Focusers)
        if (trainname == "" || focuser->opticalTrain() == trainname)
        {
            focuser->abort();
            found = true;
        }

    if (!found)
    {
        QSharedPointer newFocuser = addFocuser(trainname);
        newFocuser->abort();
    }
}

void FocusModule::adaptiveFocus(const QString &trainname)
{
    bool found = false;
    // publish to all known focusers using the same optical train (should be only one)
    for (auto focuser : m_Focusers)
        if (trainname == "" || focuser->opticalTrain() == trainname)
        {
            focuser->adaptiveFocus();
            found = true;
        }

    if (!found)
    {
        QSharedPointer newFocuser = addFocuser(trainname);
        newFocuser->adaptiveFocus();
    }
}

void FocusModule::meridianFlipStarted(const QString &trainname)
{
    bool found = false;
    // publish to all known focusers using the same optical train (should be only one)
    for (auto focuser : m_Focusers)
        if (trainname == "" || focuser->opticalTrain() == trainname)
        {
            focuser->meridianFlipStarted();
            found = true;
        }

    if (!found)
    {
        QSharedPointer newFocuser = addFocuser(trainname);
        newFocuser->meridianFlipStarted();
    }
}

void FocusModule::setMountStatus(ISD::Mount::Status newState)
{
    // publish to all known focusers using the same optical train (should be only one)
    for (auto focuser : m_Focusers)
        focuser->setMountStatus(newState);
}

void FocusModule::setMountCoords(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &ha)
{
    // publish to all known focusers using the same optical train (should be only one)
    for (auto focuser : m_Focusers)
        focuser->setMountCoords(position, pierSide, ha);
}

bool FocusModule::addTemperatureSource(const QSharedPointer<ISD::GenericDevice> &device)
{
    if (device.isNull())
        return false;

    for (auto &oneSource : m_TemperatureSources)
    {
        if (oneSource->getDeviceName() == device->getDeviceName())
            return false;
    }

    m_TemperatureSources.append(device);

    // publish new list of temperature sources to all focusers
    for (auto focuser : m_Focusers)
        focuser->updateTemperatureSources(m_TemperatureSources);

    return true;
}

void FocusModule::syncCameraInfo(const char* devicename)
{
    // publish the change to all focusers
    for (auto focuser : m_Focusers)
        if (focuser->camera() == devicename)
            focuser->syncCameraInfo();
}

void FocusModule::clearLog()
{
    m_LogText.clear();
    emit newLog(QString());
}

void FocusModule::appendLogText(const QString &logtext)
{
    m_LogText.insert(0, i18nc("log entry; %1 is the date, %2 is the text", "%1 %2",
                              KStarsData::Instance()->lt().toString("yyyy-MM-ddThh:mm:ss"), logtext));

    qCInfo(KSTARS_EKOS_FOCUS) << logtext;

    emit newLog(logtext);
}

void FocusModule::appendFocusLogText(const QString &lines)
{
    if (Options::focusLogging())
    {

        if (!m_FocusLogFile.exists())
        {
            // Create focus-specific log file and write the header record
            QDir dir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
            dir.mkpath("focuslogs");
            m_FocusLogEnabled = m_FocusLogFile.open(QIODevice::WriteOnly | QIODevice::Text);
            if (m_FocusLogEnabled)
            {
                QTextStream header(&m_FocusLogFile);
                header << "date, time, position, temperature, filter, HFR, altitude\n";
                header.flush();
            }
            else
                qCWarning(KSTARS_EKOS_FOCUS) << "Failed to open focus log file: " << m_FocusLogFileName;
        }

        if (m_FocusLogEnabled)
        {
            QTextStream out(&m_FocusLogFile);
            out << QDateTime::currentDateTime().toString("yyyy-MM-dd, hh:mm:ss, ") << lines;
            out.flush();
        }
    }

}

void FocusModule::removeDevice(const QSharedPointer<ISD::GenericDevice> &deviceRemoved)
{
    // Check in Temperature Sources.
    for (auto &oneSource : m_TemperatureSources)
        if (oneSource->getDeviceName() == deviceRemoved->getDeviceName())
            m_TemperatureSources.removeAll(oneSource);

    // publish the change to all focusers
    for (auto focuser : m_Focusers)
        focuser->removeDevice(deviceRemoved);
}


void FocusModule::initFocuser(QSharedPointer<Focus> newFocuser)
{
    connect(newFocuser.get(), &Focus::focuserChanged, this, &FocusModule::updateFocuser);
    connect(newFocuser.get(), &Focus::suspendGuiding, this, &FocusModule::suspendGuiding);
    connect(newFocuser.get(), &Focus::resumeGuiding, this, &FocusModule::resumeGuiding);
    connect(newFocuser.get(), &Focus::resumeGuiding, this, &FocusModule::resumeGuiding);
    connect(newFocuser.get(), &Focus::newStatus, this, &FocusModule::newStatus);
    connect(newFocuser.get(), &Focus::focusAdaptiveComplete, this, &FocusModule::focusAdaptiveComplete);
    connect(newFocuser.get(), &Focus::newHFR, this, &FocusModule::newHFR);
    connect(newFocuser.get(), &Focus::newFocusTemperatureDelta, this, &FocusModule::newFocusTemperatureDelta);
    connect(newFocuser.get(), &Focus::inSequenceAF, this, &FocusModule::inSequenceAF);
    connect(newFocuser.get(), &Focus::newLog, this, &FocusModule::appendLogText);
    connect(newFocuser.get(), &Focus::newFocusLog, this, &FocusModule::appendFocusLogText);
}

QSharedPointer<Focus> FocusModule::addFocuser(const QString &trainname)
{
    QSharedPointer<Focus> newFocuser;
    newFocuser.reset(new Focus(m_Focusers.count()));

    // create the new tab and bring it to front
    const int tabIndex = focusTabs->insertTab(std::max(0, focusTabs->count() - 1), newFocuser.get(), "new Focuser");
    focusTabs->setCurrentIndex(tabIndex);
    // make the tab closeable if it's not the first one
    if (tabIndex > 0)
    {
        QPushButton *closeButton = new QPushButton();
        closeButton->setIcon(QIcon::fromTheme("window-close"));
        closeButton->setFixedSize(TAB_BUTTON_SIZE, TAB_BUTTON_SIZE);
        focusTabs->tabBar()->setTabButton(tabIndex, QTabBar::RightSide, closeButton);
        connect(closeButton, &QPushButton::clicked, this, [this, tabIndex]()
        {
            checkCloseFocuserTab(tabIndex);
        });
    }

    const QString train = findUnusedOpticalTrain();

    m_Focusers.append(newFocuser);
    // select an unused train
    if (train != "")
        newFocuser->opticalTrainCombo->setCurrentText(train);

    // set the weather sources
    newFocuser->updateTemperatureSources(m_TemperatureSources);
    // set the optical train
    if (trainname != "" && newFocuser->opticalTrainCombo->findText(trainname))
        newFocuser->opticalTrainCombo->setCurrentText(trainname);

    // update the tab text
    updateFocuser(tabIndex, true);
    initFocuser(newFocuser);

    return newFocuser;
}

void FocusModule::updateFocuser(int tabID, bool isValid)
{
    if (isValid)
    {
        if (tabID < focusTabs->count() && tabID < m_Focusers.count() && !m_Focusers[tabID].isNull())
        {
            const QString name = m_Focusers[tabID]->m_Focuser != nullptr ?
                                 m_Focusers[tabID]->m_Focuser->getDeviceName() :
                                 "no focuser";
            focusTabs->setTabText(tabID, name);
        }
        else
            qCWarning(KSTARS_EKOS_FOCUS) << "Unknown focuser ID:" << tabID;
    }
    else
        focusTabs->setTabText(focusTabs->currentIndex(), "no focuser");
}

void FocusModule::closeFocuserTab(int tabIndex)
{
    focusTabs->removeTab(tabIndex);
    focuser(tabIndex).clear();
    m_Focusers.removeAt(tabIndex);
    // select the next one on the left
    focusTabs->setCurrentIndex(std::max(0, tabIndex - 1));
}

void FocusModule::checkCloseFocuserTab(int tabIndex)
{
    if (m_Focusers[tabIndex]->isBusy())
    {
        // if accept has been clicked, abort and close the tab
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, &tabIndex]()
        {
            KSMessageBox::Instance()->disconnect(this);
            m_Focusers[tabIndex]->abort();
            closeFocuserTab(tabIndex);
        });
        // if cancel has been clicked, do not close the tab
        connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [this]()
        {
            KSMessageBox::Instance()->disconnect(this);
        });

        KSMessageBox::Instance()->warningContinueCancel(i18n("Camera %1 is busy. Abort to close?",
                m_Focusers[tabIndex]->m_Focuser->getDeviceName()), i18n("Stop capturing"), 30, false, i18n("Abort"));
    }
    else
    {
        closeFocuserTab(tabIndex);

    }
}

const QString FocusModule::findUnusedOpticalTrain()
{
    const auto names = OpticalTrainManager::Instance()->getTrainNames();
    QSet<QString> trainnames = QSet<QString>(names.begin(), names.end());
    foreach(auto focuser, m_Focusers)
        trainnames.remove(focuser->opticalTrain());

    if (trainnames.isEmpty())
        return "";
    else
        return trainnames.values().first();
}

}
