/*  INDI Property
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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
INDI_P::INDI_P(INDI_G *ipg, INDI::Property *prop)
{
    pg       = ipg;
    dataProp = prop;

    name = QString(prop->getName());

    PHBox.reset(new QHBoxLayout());
    PHBox->setMargin(0);
    PVBox = new QVBoxLayout();
    PVBox->setMargin(0);

    initGUI();
}

INDI_P::~INDI_P()
{
    qDeleteAll(elementList);
    elementList.clear();
}

void INDI_P::updateStateLED()
{
    /* set state light */
    switch (dataProp->getState())
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

        default:
            break;
    }
}

/* build widgets for property pp using info in root.
 */
void INDI_P::initGUI()
{
    QString label = i18nc(libindi_strings_context, dataProp->getLabel());

    if (label == "(I18N_EMPTY_MESSAGE)")
        label = dataProp->getLabel();

    /* add to GUI group */
    ledStatus.reset(new KLed(pg->getContainer()));
    ledStatus->setMaximumSize(16, 16);
    ledStatus->setLook(KLed::Sunken);

    updateStateLED();

    /* #1 First widegt is the LED status indicator */
    PHBox->addWidget(ledStatus.get());

    if (label.isEmpty())
    {
        label = i18nc(libindi_strings_context, name.toUtf8());
        if (label == "(I18N_EMPTY_MESSAGE)")
            label = name.toUtf8();

        labelW.reset(new KSqueezedTextLabel(label, pg->getContainer()));
    }
    else
        labelW.reset(new KSqueezedTextLabel(label, pg->getContainer()));

    //labelW->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    labelW->setFrameShape(QFrame::StyledPanel);
    labelW->setFixedWidth(PROPERTY_LABEL_WIDTH * KStars::Instance()->devicePixelRatio());
    labelW->setTextFormat(Qt::RichText);
    labelW->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    labelW->setWordWrap(true);

    PHBox->addWidget(labelW.get());

    ledStatus->show();
    labelW->show();

    // #3 Add the Vertical layout which may contain several elements
    PHBox->addLayout(PVBox);

    switch (dataProp->getType())
    {
        case INDI_SWITCH:
            if (dataProp->getSwitch()->r == ISR_NOFMANY)
                guiType = PG_RADIO;
            else if (dataProp->getSwitch()->nsp > 4)
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
    INDI_E *lp                 = nullptr;
    ISwitchVectorProperty *svp = dataProp->getSwitch();

    if (svp == nullptr)
        return;

    groupB.reset(new QButtonGroup());

    if (guiType == PG_BUTTONS)
    {
        if (svp->r == ISR_1OFMANY)
            groupB->setExclusive(true);
        else
            groupB->setExclusive(false);
    }
    else if (guiType == PG_RADIO)
        groupB->setExclusive(false);

    if (svp->p != IP_RO)
        QObject::connect(groupB.get(), SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(newSwitch(QAbstractButton*)));

    for (int i = 0; i < svp->nsp; i++)
    {
        ISwitch *sp = &(svp->sp[i]);
        lp          = new INDI_E(this, dataProp);

        lp->buildSwitch(groupB.get(), sp);

        elementList.append(lp);
    }

    horSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    PHBox->addItem(horSpacer);
}

void INDI_P::buildTextGUI()
{
    INDI_E *lp               = nullptr;
    ITextVectorProperty *tvp = dataProp->getText();

    if (tvp == nullptr)
        return;

    for (int i = 0; i < tvp->ntp; i++)
    {
        IText *tp = &(tvp->tp[i]);
        lp        = new INDI_E(this, dataProp);

        lp->buildText(tp);

        elementList.append(lp);
    }

    horSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    PHBox->addItem(horSpacer);

    if (tvp->p == IP_RO)
        return;

    // INDI STD, but we use our own controls
    if (name == "TIME_UTC")
        setupSetButton(i18n("Time"));
    else
        setupSetButton(i18n("Set"));
}

void INDI_P::buildNumberGUI()
{
    INDI_E *lp                 = nullptr;
    INumberVectorProperty *nvp = dataProp->getNumber();

    if (nvp == nullptr)
        return;

    for (int i = 0; i < nvp->nnp; i++)
    {
        INumber *np = &(nvp->np[i]);
        lp          = new INDI_E(this, dataProp);

        lp->buildNumber(np);

        elementList.append(lp);
    }

    horSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    PHBox->addItem(horSpacer);

    if (nvp->p == IP_RO)
        return;

    setupSetButton(i18n("Set"));
}

void INDI_P::buildLightGUI()
{
    INDI_E *ep                = nullptr;
    ILightVectorProperty *lvp = dataProp->getLight();

    if (lvp == nullptr)
        return;

    for (int i = 0; i < lvp->nlp; i++)
    {
        ILight *lp = &(lvp->lp[i]);
        ep         = new INDI_E(this, dataProp);

        ep->buildLight(lp);

        elementList.append(ep);
    }

    horSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    PHBox->addItem(horSpacer);
}

void INDI_P::buildBLOBGUI()
{
    INDI_E *lp               = nullptr;
    IBLOBVectorProperty *bvp = dataProp->getBLOB();

    if (bvp == nullptr)
        return;

    for (int i = 0; i < bvp->nbp; i++)
    {
        IBLOB *bp = &(bvp->bp[i]);
        lp        = new INDI_E(this, dataProp);

        lp->buildBLOB(bp);

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

    if (dataProp->getPermission() != IP_RO)
        setupSetButton(i18n("Upload"));
}

void INDI_P::setBLOBOption(int state)
{
    if (state == Qt::Checked)
        pg->getDevice()->getClientManager()->setBLOBMode(B_ALSO, dataProp->getDeviceName(), dataProp->getName());
    else
        pg->getDevice()->getClientManager()->setBLOBMode(B_NEVER, dataProp->getDeviceName(), dataProp->getName());
}

void INDI_P::newSwitch(QAbstractButton *button)
{
    ISwitchVectorProperty *svp = dataProp->getSwitch();
    QString buttonText         = button->text();

    if (svp == nullptr)
        return;

    buttonText.remove('&');

    foreach (INDI_E *el, elementList)
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
    ISwitchVectorProperty *svp = dataProp->getSwitch();

    if (svp == nullptr)
        return;

    if (menuC.get() != nullptr)
    {
        menuC->setCurrentIndex(IUFindOnSwitchIndex(svp));
    }
}

void INDI_P::newSwitch(int index)
{
    ISwitchVectorProperty *svp = dataProp->getSwitch();

    if (svp == nullptr)
        return;

    if (index >= svp->nsp)
        return;

    ISwitch *sp = &(svp->sp[index]);

    IUResetSwitch(svp);
    sp->s = ISS_ON;

    sendSwitch();
}

void INDI_P::newSwitch(const QString &name)
{
    ISwitchVectorProperty *svp = dataProp->getSwitch();

    if (svp == nullptr)
        return;

    ISwitch *sp = IUFindSwitch(svp, name.toLatin1().constData());

    if (sp == nullptr)
        return;

    if (svp->r == ISR_1OFMANY)
    {
        IUResetSwitch(svp);
        sp->s = ISS_ON;
    }
    else
    {
        if (svp->r == ISR_ATMOST1)
        {
            ISState prev_state = sp->s;
            IUResetSwitch(svp);
            sp->s = prev_state;
        }

        sp->s = (sp->s == ISS_ON) ? ISS_OFF : ISS_ON;
    }

    sendSwitch();
}

void INDI_P::sendSwitch()
{
    ISwitchVectorProperty *svp = dataProp->getSwitch();

    if (svp == nullptr)
        return;

    svp->s = IPS_BUSY;

    foreach (INDI_E *el, elementList)
        el->syncSwitch();

    updateStateLED();

    // Send it to server
    pg->getDevice()->getClientManager()->sendNewSwitch(svp);
}

void INDI_P::sendText()
{
    ITextVectorProperty *tvp   = nullptr;
    INumberVectorProperty *nvp = nullptr;

    switch (dataProp->getType())
    {
        case INDI_TEXT:
            tvp = dataProp->getText();
            if (tvp == nullptr)
                return;

            tvp->s = IPS_BUSY;

            foreach (INDI_E *el, elementList)
                el->updateTP();

            pg->getDevice()->getClientManager()->sendNewText(tvp);

            break;

        case INDI_NUMBER:
            nvp = dataProp->getNumber();
            if (nvp == nullptr)
                return;

            nvp->s = IPS_BUSY;

            foreach (INDI_E *el, elementList)
                el->updateNP();

            pg->getDevice()->getClientManager()->sendNewNumber(nvp);

        default:
            break;
    }

    updateStateLED();
}

void INDI_P::buildMenuGUI()
{
    QStringList menuOptions;
    QString oneOption;
    int onItem                 = -1;
    INDI_E *lp                 = nullptr;
    ISwitchVectorProperty *svp = dataProp->getSwitch();

    if (svp == nullptr)
        return;

    menuC.reset(new QComboBox(pg->getContainer()));

    if (svp->p == IP_RO)
        connect(menuC.get(), SIGNAL(activated(int)), this, SLOT(resetSwitch()));
    else
        connect(menuC.get(), SIGNAL(activated(int)), this, SLOT(newSwitch(int)));

    for (int i = 0; i < svp->nsp; i++)
    {
        ISwitch *tp = &(svp->sp[i]);

        if (tp->s == ISS_ON)
            onItem = i;

        lp = new INDI_E(this, dataProp);

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

    PHBox->addWidget(menuC.get());
    PHBox->addItem(horSpacer);
}

void INDI_P::setupSetButton(const QString &caption)
{
    setB.reset(new QPushButton(caption, pg->getContainer()));
    setB->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setB->setMinimumWidth(MIN_SET_WIDTH * KStars::Instance()->devicePixelRatio());
    setB->setMaximumWidth(MAX_SET_WIDTH * KStars::Instance()->devicePixelRatio());

    connect(setB.get(), SIGNAL(clicked()), this, SLOT(processSetButton()));

    PHBox->addWidget(setB.get());
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
    ISwitchVectorProperty *svp = dataProp->getSwitch();

    if (svp == nullptr)
        return;

    int currentIndex = IUFindOnSwitchIndex(svp);
    menuC->setCurrentIndex(currentIndex);
}

void INDI_P::processSetButton()
{
    switch (dataProp->getType())
    {
        case INDI_TEXT:
            if (!strcmp(dataProp->getName(), "TIME_UTC"))
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
    IBLOBVectorProperty *bvp = dataProp->getBLOB();

    if (bvp == nullptr)
        return;

    bvp->s = IPS_BUSY;

    pg->getDevice()->getClientManager()->startBlob(bvp->device, bvp->name, timestamp());

    for (int i = 0; i < elementList.count(); i++)
    {
        IBLOB *bp = &(bvp->bp[i]);
#if (INDI_VERSION_MINOR >= 4 && INDI_VERSION_RELEASE >= 2)
        pg->getDevice()->getClientManager()->sendOneBlob(bp);
#else
        pg->getDevice()->getClientManager()->sendOneBlob(bp->name, bp->size, bp->format, bp->blob);
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
    INDI_E *timeEle;
    INDI_E *offsetEle;

    timeEle   = getElement("UTC");
    offsetEle = getElement("OFFSET");
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

INDI_E *INDI_P::getElement(const QString &elementName)
{
    foreach (INDI_E *ep, elementList)
    {
        if (ep->getName() == elementName)
            return ep;
    }

    return nullptr;
}
