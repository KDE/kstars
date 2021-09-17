/*
    SPDX-FileCopyrightText: 2003 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "indiproperty.h"

#include "clientmanager.h"
#include "indidevice.h"
#include "indielement.h"
#include "indigroup.h"
#include "kstars.h"
#include "Options.h"
#include "skymap.h"
#include "dialogs/timedialog.h"

#include <indicom.h>
#include <indiproperty.h>

#include <KLed>
#include <KSqueezedTextLabel>

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

extern const char *libindi_strings_context;

/*******************************************************************
** INDI Property: contains widgets, labels, and their status
*******************************************************************/
INDI_P::INDI_P(INDI_G *ipg, INDI::Property prop) : QWidget(ipg), pg(ipg), dataProp(prop)
{
    name = QString(prop.getName());

    PHBox = new QHBoxLayout(this);
    PHBox->setObjectName("Property Horizontal Layout");
    PHBox->setContentsMargins(0, 0, 0, 0);
    PVBox = new QVBoxLayout;
    PVBox->setContentsMargins(0, 0, 0, 0);
    PVBox->setObjectName("Property Vertical Layout");

    initGUI();
}

void INDI_P::updateStateLED()
{
    /* set state light */
    switch (dataProp.getState())
    {
        case IPS_IDLE:
            ledStatus->setColor(Qt::gray);
            break;

        case IPS_OK:
            ledStatus->setColor(Qt::green);
            break;

        case IPS_BUSY:
            ledStatus->setColor(Qt::yellow);
            break;

        case IPS_ALERT:
            ledStatus->setColor(Qt::red);
            break;
    }
}

/* build widgets for property pp using info in root.
 */
void INDI_P::initGUI()
{
    QString label = i18nc(libindi_strings_context, dataProp.getLabel());

    if (label == "(I18N_EMPTY_MESSAGE)")
        label = dataProp.getLabel();

    /* add to GUI group */
    ledStatus = new KLed(this);
    ledStatus->setMaximumSize(16, 16);
    ledStatus->setLook(KLed::Sunken);

    updateStateLED();

    /* Create a horizontally layout widget around light and label */
    QWidget *labelWidget = new QWidget(this);
    QHBoxLayout *labelLayout =  new QHBoxLayout(labelWidget);
    labelLayout->setContentsMargins(0, 0, 0, 0);

    /* #1 First widget is the LED status indicator */
    labelLayout->addWidget(ledStatus);

    if (label.isEmpty())
    {
        label = i18nc(libindi_strings_context, name.toUtf8());
        if (label == "(I18N_EMPTY_MESSAGE)")
            label = name.toUtf8();

        labelW = new KSqueezedTextLabel(label, this);
    }
    else
        labelW = new KSqueezedTextLabel(label, this);

    //labelW->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    labelW->setFrameShape(QFrame::Box);
    labelW->setFrameShadow(QFrame::Sunken);
    labelW->setMargin(2);
    labelW->setFixedWidth(PROPERTY_LABEL_WIDTH * KStars::Instance()->devicePixelRatio());
    labelW->setTextFormat(Qt::RichText);
    labelW->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    labelW->setWordWrap(true);

    labelLayout->addWidget(labelW);
    PHBox->addWidget(labelWidget, 0, Qt::AlignTop | Qt::AlignLeft);

    ledStatus->show();
    labelW->show();

    // #3 Add the Vertical layout which may contain several elements
    PHBox->addLayout(PVBox);

    switch (dataProp.getType())
    {
        case INDI_SWITCH:
            if (dataProp.getSwitch()->getRule() == ISR_NOFMANY)
                guiType = PG_RADIO;
            else if (dataProp.getSwitch()->count() > 4)
                guiType = PG_MENU;
            else
                guiType = PG_BUTTONS;

            if (guiType == PG_MENU)
                buildMenuGUI();
            else
                buildSwitchGUI();
            break;

        case INDI_TEXT:
            buildTextGUI();
            break;

        case INDI_NUMBER:
            buildNumberGUI();
            break;

        case INDI_LIGHT:
            buildLightGUI();
            break;

        case INDI_BLOB:
            buildBLOBGUI();
            break;

        default:
            break;
    }
}

void INDI_P::buildSwitchGUI()
{
    auto svp = dataProp.getSwitch();

    if (!svp)
        return;

    groupB = new QButtonGroup(this);

    if (guiType == PG_BUTTONS)
    {
        if (svp->getRule() == ISR_1OFMANY)
            groupB->setExclusive(true);
        else
            groupB->setExclusive(false);
    }
    else if (guiType == PG_RADIO)
        groupB->setExclusive(false);

    if (svp->p != IP_RO)
        QObject::connect(groupB, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(newSwitch(QAbstractButton*)));

    for (auto &it : *svp)
    {
        auto lp = new INDI_E(this, dataProp);
        lp->buildSwitch(groupB, &it);
        elementList.append(lp);
    }

    horSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    PHBox->addItem(horSpacer);
}

void INDI_P::buildTextGUI()
{
    auto tvp = dataProp.getText();

    if (!tvp)
        return;

    for (auto &it : *tvp)
    {
        auto lp = new INDI_E(this, dataProp);
        lp->buildText(&it);
        elementList.append(lp);
    }

    horSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    PHBox->addItem(horSpacer);

    if (tvp->getPermission() == IP_RO)
        return;

    // INDI STD, but we use our own controls
    if (name == "TIME_UTC")
        setupSetButton(i18n("Time"));
    else
        setupSetButton(i18n("Set"));
}

void INDI_P::buildNumberGUI()
{
    auto nvp = dataProp.getNumber();

    if (!nvp)
        return;

    for (auto &it : *nvp)
    {
        auto lp = new INDI_E(this, dataProp);
        lp->buildNumber(&it);
        elementList.append(lp);
    }

    horSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    PHBox->addItem(horSpacer);

    if (nvp->getPermission() == IP_RO)
        return;

    setupSetButton(i18n("Set"));
}

void INDI_P::buildLightGUI()
{
    auto lvp = dataProp.getLight();

    if (!lvp)
        return;

    for (auto &it : *lvp)
    {
        auto ep = new INDI_E(this, dataProp);
        ep->buildLight(&it);
        elementList.append(ep);
    }

    horSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    PHBox->addItem(horSpacer);
}

void INDI_P::buildBLOBGUI()
{
    auto bvp = dataProp.getBLOB();

    if (!bvp)
        return;

    for (auto &it : *bvp)
    {
        auto lp = new INDI_E(this, dataProp);
        lp->buildBLOB(&it);
        elementList.append(lp);
    }

    horSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    PHBox->addItem(horSpacer);

    enableBLOBC = new QCheckBox();
    enableBLOBC->setIcon(QIcon::fromTheme("network-modem"));
    enableBLOBC->setChecked(true);
    enableBLOBC->setToolTip(i18n("Enable binary data transfer from this property to KStars and vice-versa."));

    PHBox->addWidget(enableBLOBC);

    connect(enableBLOBC, SIGNAL(stateChanged(int)), this, SLOT(setBLOBOption(int)));

    if (dataProp.getPermission() != IP_RO)
        setupSetButton(i18n("Upload"));
}

void INDI_P::setBLOBOption(int state)
{
    pg->getDevice()->getClientManager()->setBLOBEnabled(state == Qt::Checked, dataProp.getDeviceName(), dataProp.getName());
}

void INDI_P::newSwitch(QAbstractButton *button)
{
    auto svp = dataProp.getSwitch();
    QString buttonText = button->text();

    if (!svp)
        return;

    buttonText.remove('&');

    for (auto &el : elementList)
    {
        if (el->getLabel() == buttonText)
        {
            newSwitch(el->getName());
            return;
        }
    }
}

void INDI_P::resetSwitch()
{
    auto svp = dataProp.getSwitch();

    if (!svp)
        return;

    if (menuC != nullptr)
    {
        menuC->setCurrentIndex(svp->findOnSwitchIndex());
    }
}

void INDI_P::newSwitch(int index)
{
    auto svp = dataProp.getSwitch();

    if (!svp)
        return;

    if (index >= svp->count() || index < 0)
        return;

    auto sp = svp->at(index);

    svp->reset();
    sp->setState(ISS_ON);

    sendSwitch();
}

void INDI_P::newSwitch(const QString &name)
{
    auto svp = dataProp.getSwitch();

    if (!svp)
        return;

    auto sp = svp->findWidgetByName(name.toLatin1().constData());

    if (!sp)
        return;

    if (svp->getRule() == ISR_1OFMANY)
    {
        svp->reset();
        sp->setState(ISS_ON);
    }
    else
    {
        if (svp->getRule() == ISR_ATMOST1)
        {
            ISState prev_state = sp->getState();
            svp->reset();
            sp->setState(prev_state);
        }

        sp->setState(sp->getState() == ISS_ON ? ISS_OFF : ISS_ON);
    }

    sendSwitch();
}

void INDI_P::sendSwitch()
{
    auto svp = dataProp.getSwitch();

    if (!svp)
        return;

    svp->setState(IPS_BUSY);

    for (auto &el : elementList)
        el->syncSwitch();

    updateStateLED();

    // Send it to server
    pg->getDevice()->getClientManager()->sendNewSwitch(svp);
}

void INDI_P::sendText()
{
    switch (dataProp.getType())
    {
        case INDI_TEXT:
        {
            auto tvp = dataProp.getText();
            if (!tvp)
                return;

            tvp->setState(IPS_BUSY);

            for (auto &el : elementList)
                el->updateTP();

            pg->getDevice()->getClientManager()->sendNewText(tvp);

            break;
        }

        case INDI_NUMBER:
        {
            auto nvp = dataProp.getNumber();
            if (!nvp)
                return;

            nvp->setState(IPS_BUSY);

            for (auto &el : elementList)
                el->updateNP();

            pg->getDevice()->getClientManager()->sendNewNumber(nvp);
            break;
        }
        default:
            break;
    }

    updateStateLED();
}

void INDI_P::buildMenuGUI()
{
    QStringList menuOptions;
    QString oneOption;
    int onItem = -1;
    auto svp = dataProp.getSwitch();

    if (!svp)
        return;

    menuC = new QComboBox(this);

    if (svp->getPermission() == IP_RO)
        connect(menuC, SIGNAL(activated(int)), this, SLOT(resetSwitch()));
    else
        connect(menuC, SIGNAL(activated(int)), this, SLOT(newSwitch(int)));

    for (int i = 0; i < svp->nsp; i++)
    {
        auto tp = svp->at(i);

        if (tp->getState() == ISS_ON)
            onItem = i;

        auto lp = new INDI_E(this, dataProp);

        lp->buildMenuItem(tp);

        oneOption = i18nc(libindi_strings_context, lp->getLabel().toUtf8());

        if (oneOption == "(I18N_EMPTY_MESSAGE)")
            oneOption = lp->getLabel().toUtf8();

        menuOptions.append(oneOption);

        elementList.append(lp);
    }

    menuC->addItems(menuOptions);
    menuC->setCurrentIndex(onItem);

    horSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    PHBox->addWidget(menuC);
    PHBox->addItem(horSpacer);
}

void INDI_P::setupSetButton(const QString &caption)
{
    setB = new QPushButton(caption, this);
    setB->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setB->setMinimumWidth(MIN_SET_WIDTH * KStars::Instance()->devicePixelRatio());
    setB->setMaximumWidth(MAX_SET_WIDTH * KStars::Instance()->devicePixelRatio());

    connect(setB, SIGNAL(clicked()), this, SLOT(processSetButton()));

    PHBox->addWidget(setB);
}

void INDI_P::addWidget(QWidget *w)
{
    PHBox->addWidget(w);
}

void INDI_P::addLayout(QHBoxLayout *layout)
{
    PVBox->addLayout(layout);
}

void INDI_P::updateMenuGUI()
{
    auto svp = dataProp.getSwitch();

    if (!svp)
        return;

    int currentIndex = svp->findOnSwitchIndex();
    menuC->setCurrentIndex(currentIndex);
}

void INDI_P::processSetButton()
{
    switch (dataProp.getType())
    {
        case INDI_TEXT:
            //if (!strcmp(dataProp.getName(), "TIME_UTC"))
            if (dataProp.isNameMatch("TIME_UTC"))
                newTime();
            else
                sendText();

            break;

        case INDI_NUMBER:
            sendText();
            break;

        case INDI_BLOB:
            sendBlob();
            break;

        default:
            break;
    }
}

void INDI_P::sendBlob()
{
    //int index=0;
    //bool openingTag=false;
    auto bvp = dataProp.getBLOB();

    if (!bvp)
        return;

    bvp->setState(IPS_BUSY);

    pg->getDevice()->getClientManager()->startBlob(bvp->getDeviceName(), bvp->getName(), timestamp());

    for (int i = 0; i < elementList.count(); i++)
    {
        INDI::WidgetView<IBLOB> *bp = bvp->at(i);
#if (INDI_VERSION_MINOR >= 4 && INDI_VERSION_RELEASE >= 2)
        pg->getDevice()->getClientManager()->sendOneBlob(bp);
#else
        pg->getDevice()->getClientManager()->sendOneBlob(bp->getName(), bp->getSize(), bp->getFormat(), const_cast<void *>(bp->getBlob()));
#endif
    }

    // JM: Why we need dirty here? We should be able to upload multiple time
    /*foreach(INDI_E *ep, elementList)
    {
        if (ep->getBLOBDirty() == true)
        {

            if (openingTag == false)
            {
                pg->getDevice()->getClientManager()->startBlob(bvp->device, bvp->name, timestamp());
                openingTag = true;
            }

            IBLOB *bp = &(bvp->bp[index]);
            ep->setBLOBDirty(false);

            //qDebug() << "SENDING BLOB " << bp->name << " has size of " << bp->size << " and bloblen of " << bp->bloblen << endl;
            pg->getDevice()->getClientManager()->sendOneBlob(bp->name, bp->size, bp->format, bp->blob);

        }

        index++;

    }*/

    //if (openingTag)
    pg->getDevice()->getClientManager()->finishBlob();

    updateStateLED();
}

void INDI_P::newTime()
{
    INDI_E *timeEle   = getElement("UTC");
    INDI_E *offsetEle = getElement("OFFSET");
    if (!timeEle || !offsetEle)
        return;

    TimeDialog timedialog(KStars::Instance()->data()->ut(), KStars::Instance()->data()->geo(), KStars::Instance(),
                          true);

    if (timedialog.exec() == QDialog::Accepted)
    {
        QTime newTime(timedialog.selectedTime());
        QDate newDate(timedialog.selectedDate());

        timeEle->setText(QString("%1-%2-%3T%4:%5:%6")
                         .arg(newDate.year())
                         .arg(newDate.month())
                         .arg(newDate.day())
                         .arg(newTime.hour())
                         .arg(newTime.minute())
                         .arg(newTime.second()));

        offsetEle->setText(QString().setNum(KStars::Instance()->data()->geo()->TZ(), 'g', 2));

        sendText();
    }
    else
        return;
}

INDI_E *INDI_P::getElement(const QString &elementName) const
{
    for (auto *ep : elementList)
    {
        if (ep->getName() == elementName)
            return ep;
    }

    return nullptr;
}

bool INDI_P::isRegistered() const
{
    return (dataProp && dataProp.getRegistered());
}
