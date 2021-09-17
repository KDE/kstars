/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq (mutlaqja AT ikarustech DOT com)

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <zlib.h>
#include <indicom.h>
#include <base64.h>
#include <basedevice.h>

#include <QFrame>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QLayout>
#include <QButtonGroup>
#include <QSocketNotifier>
#include <QDateTime>
#include <QSplitter>
#include <QLineEdit>
#include <QDebug>
#include <QComboBox>
#include <QStatusBar>
#include <QMenu>
#include <QTabWidget>
#include <QTextEdit>

#include <KLed>
#include <KLocalizedString>
#include <KMessageBox>

#include "kstars.h"
#include "skymap.h"
#include "Options.h"
#include "skyobjects/skyobject.h"
#include "dialogs/timedialog.h"
#include "geolocation.h"

#include "indiproperty.h"
#include "indidevice.h"
#include "indigroup.h"
#include "indielement.h"

#include <indi_debug.h>

const char *libindi_strings_context = "string from libindi, used in the config dialog";

INDI_D::INDI_D(QWidget *parent, INDI::BaseDevice *in_dv, ClientManager *in_cm) : QWidget(parent)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    m_BaseDevice = in_dv;
    m_ClientManager = in_cm;

    m_Name = m_BaseDevice->getDeviceName();

    QHBoxLayout *layout = new QHBoxLayout(this);

    deviceVBox = new QSplitter(Qt::Vertical, this);

    groupContainer = new QTabWidget(this);

    msgST_w = new QTextEdit(this);
    msgST_w->setReadOnly(true);
    msgST_w->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContentsOnFirstShow);

    deviceVBox->addWidget(groupContainer);
    deviceVBox->addWidget(msgST_w);
    deviceVBox->setStretchFactor(0, 2);

    layout->addWidget(deviceVBox);
}

bool INDI_D::buildProperty(INDI::Property prop)
{
    if (!prop.isValid())
        return false;

    QString groupName(prop.getGroupName());

    if (prop.getDeviceName() != m_Name)
        return false;

    INDI_G *pg = getGroup(groupName);

    if (pg == nullptr)
    {
        pg = new INDI_G(this, groupName);
        groupsList.append(pg);
        groupContainer->addTab(pg, i18nc(libindi_strings_context, groupName.toUtf8()));
    }

    return pg->addProperty(prop);
}

#if 0
bool INDI_D::removeProperty(INDI::Property prop)
{
    if (prop == nullptr)
        return false;

    QString groupName(prop->getGroupName());

    if (strcmp(prop->getDeviceName(), m_BaseDevice->getDeviceName()))
    {
        // qDebug() << "Ignoring property " << prop->getName() << " for device " << prop->getgetDeviceName() << " because our device is "
        //     << dv->getDeviceName() << endl;
        return false;
    }

    // qDebug() << "Received new property " << prop->getName() << " for our device " << dv->getDeviceName() << endl;

    INDI_G *pg = getGroup(groupName);

    if (pg == nullptr)
        return false;

    bool removeResult = pg->removeProperty(prop->getName());

    if (pg->size() == 0 && removeResult)
    {
        //qDebug() << "Removing tab for group " << pg->getName() << " with an index of " << groupsList.indexOf(pg) << endl;
        groupContainer->removeTab(groupsList.indexOf(pg));
        groupsList.removeOne(pg);
        delete (pg);
    }

    return removeResult;
}
#endif

bool INDI_D::removeProperty(const QString &device, const QString &name)
{
    if (device != m_Name)
        return false;

    for (auto &oneGroup : groupsList)
    {
        for (auto &oneProperty : oneGroup->getProperties())
        {
            if (name == oneProperty->getName())
            {
                bool rc = oneGroup->removeProperty(name);
                if (oneGroup->size() == 0)
                {
                    int index = groupsList.indexOf(oneGroup);
                    groupContainer->removeTab(index);
                    delete groupsList.takeAt(index);
                }
                return rc;
            }
        }
    }

    return false;
}

bool INDI_D::updateSwitchGUI(ISwitchVectorProperty *svp)
{
    INDI_P *guiProp = nullptr;
    QString propName(svp->name);

    if (m_Name != svp->device)
        return false;

    for (const auto &pg : groupsList)
    {
        if ((guiProp = pg->getProperty(propName)) != nullptr)
            break;
    }

    if (guiProp == nullptr || guiProp->isRegistered() == false)
        return false;

    guiProp->updateStateLED();

    if (guiProp->getGUIType() == PG_MENU)
        guiProp->updateMenuGUI();
    else
    {
        for (const auto &lp : guiProp->getElements())
            lp->syncSwitch();
    }

    return true;
}

bool INDI_D::updateTextGUI(ITextVectorProperty *tvp)
{
    INDI_P *guiProp = nullptr;
    QString propName(tvp->name);

    if (m_Name != tvp->device)
        return false;

    for (const auto &pg : groupsList)
    {
        INDI_P * const p = pg->getProperty(propName);
        if (p != nullptr)
        {
            guiProp = p;
            break;
        }
    }

    if (guiProp == nullptr)
        return false;

    guiProp->updateStateLED();

    for (const auto &lp : guiProp->getElements())
        lp->syncText();

    return true;
}

bool INDI_D::updateNumberGUI(INumberVectorProperty *nvp)
{
    INDI_P *guiProp = nullptr;
    QString propName(nvp->name);

    if (m_Name != nvp->device)
        return false;

    for (const auto &pg : groupsList)
    {
        INDI_P * const p = pg->getProperty(propName);
        if (p != nullptr)
        {
            guiProp = p;
            break;
        }
    }

    if (guiProp == nullptr)
        return false;

    guiProp->updateStateLED();

    for (const auto &lp : guiProp->getElements())
        lp->syncNumber();

    return true;
}

bool INDI_D::updateLightGUI(ILightVectorProperty *lvp)
{
    INDI_P *guiProp = nullptr;
    QString propName(lvp->name);

    if (m_Name != lvp->device)
        return false;

    for (const auto &pg : groupsList)
    {
        INDI_P * const p = pg->getProperty(propName);
        if (p != nullptr)
        {
            guiProp = p;
            break;
        }
    }

    if (guiProp == nullptr)
        return false;

    guiProp->updateStateLED();

    for (const auto &lp : guiProp->getElements())
        lp->syncLight();

    return true;
}

bool INDI_D::updateBLOBGUI(IBLOB *bp)
{
    INDI_P *guiProp = nullptr;
    QString propName(bp->bvp->name);

    if (m_Name != bp->bvp->device)
        return false;

    for (const auto &pg : groupsList)
    {
        INDI_P * const p = pg->getProperty(propName);
        if (p != nullptr)
        {
            guiProp = p;
            break;
        }
    }

    if (guiProp == nullptr)
        return false;

    guiProp->updateStateLED();

    return true;
}

void INDI_D::updateMessageLog(INDI::BaseDevice *idv, int messageID)
{
    if (idv != m_BaseDevice)
        return;

    QString message = QString::fromStdString(m_BaseDevice->messageQueue(messageID));
    QString formatted = message;

    // TODO the colors should be from the color scheme
    if (message.mid(21, 2) == "[E")
        formatted = QString("<span style='color:red'>%1</span>").arg(message);
    else if (message.mid(21, 2) == "[W")
        formatted = QString("<span style='color:orange'>%1</span>").arg(message);
    else if (message.mid(21, 2) != "[I")
    {
        // Debug message
        qCDebug(KSTARS_INDI) << idv->getDeviceName() << ":" << message.mid(21);
        return;
    }

    if (Options::showINDIMessages())
        KStars::Instance()->statusBar()->showMessage(i18nc("INDI message shown in status bar", "%1", message), 0);

    msgST_w->ensureCursorVisible();
    msgST_w->insertHtml(i18nc("Message shown in INDI control panel", "%1", formatted));
    msgST_w->insertPlainText("\n");
    QTextCursor c = msgST_w->textCursor();
    c.movePosition(QTextCursor::Start);
    msgST_w->setTextCursor(c);

    qCInfo(KSTARS_INDI) << idv->getDeviceName() << ": " << message.mid(21);
}

//INDI_D::~INDI_D()
//{
//    while (!groupsList.isEmpty())
//        delete groupsList.takeFirst();
//}

INDI_G *INDI_D::getGroup(const QString &groupName) const
{
    for (const auto &pg : groupsList)
    {
        if (pg->getName() == groupName)
            return pg;
    }

    return nullptr;
}

void INDI_D::clearMessageLog()
{
    msgST_w->clear();
}
