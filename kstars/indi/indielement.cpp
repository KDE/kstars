/*
    SPDX-FileCopyrightText: 2003 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    2004-01-15	INDI element is the most basic unit of the INDI KStars client.
*/

#include "indielement.h"

#include "indiproperty.h"
#include "indigroup.h"
#include "indidevice.h"

#include "kstars.h"
#include "ksnotification.h"

#include <indicom.h>

#include <KSqueezedTextLabel>
#include <KLocalizedString>
#include <KLed>
#include <KMessageBox>

#include <QButtonGroup>
#include <QCheckBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>

extern const char *libindi_strings_context;

/*******************************************************************
** INDI Element
*******************************************************************/
INDI_E::INDI_E(INDI_P *gProp, INDI::Property dProp) : QWidget(gProp), guiProp(gProp), dataProp(dProp)
{
    EHBox = new QHBoxLayout;
    EHBox->setObjectName("Element Horizontal Layout");
    EHBox->setContentsMargins(0, 0, 0, 0);
}

void INDI_E::buildSwitch(QButtonGroup *groupB, ISwitch *sw)
{
    name  = sw->name;
    label = i18nc(libindi_strings_context, sw->label);

    if (label == "(I18N_EMPTY_MESSAGE)")
        label = sw->label;

    sp = sw;

    if (label.isEmpty())
        label = i18nc(libindi_strings_context, sw->name);

    if (label == "(I18N_EMPTY_MESSAGE)")
        label = sw->name;

    if (groupB == nullptr)
        return;

    switch (guiProp->getGUIType())
    {
        case PG_BUTTONS:
            push_w = new QPushButton(label, this);
            push_w->setStyleSheet(":checked {background-color: darkGreen}");
            push_w->setCheckable(true);
            groupB->addButton(push_w);

            syncSwitch();

            guiProp->addWidget(push_w);

            push_w->show();

            if (dataProp.getPermission() == IP_RO)
                push_w->setEnabled(sw->s == ISS_ON);

            break;

        case PG_RADIO:
            check_w = new QCheckBox(label, this);
            groupB->addButton(check_w);

            syncSwitch();

            guiProp->addWidget(check_w);

            check_w->show();

            if (dataProp.getPermission() == IP_RO)
                check_w->setEnabled(sw->s == ISS_ON);

            break;

        default:
            break;
    }
}

void INDI_E::buildMenuItem(ISwitch *sw)
{
    buildSwitch(nullptr, sw);
}

void INDI_E::buildText(IText *itp)
{
    name = itp->name;
    if (itp->label[0])
        label = i18nc(libindi_strings_context, itp->label);

    if (label == "(I18N_EMPTY_MESSAGE)")
        label = itp->label;

    tp = itp;

    if (label.isEmpty())
        label = i18nc(libindi_strings_context, itp->name);

    if (label == "(I18N_EMPTY_MESSAGE)")
        label = itp->name;

    setupElementLabel();

    if (tp->text[0])
        text = i18nc(libindi_strings_context, tp->text);

    switch (dataProp.getPermission())
    {
        case IP_RW:
            setupElementRead(ELEMENT_READ_WIDTH  * KStars::Instance()->devicePixelRatio());
            setupElementWrite(ELEMENT_WRITE_WIDTH  * KStars::Instance()->devicePixelRatio());

            break;

        case IP_RO:
            setupElementRead(ELEMENT_FULL_WIDTH * KStars::Instance()->devicePixelRatio());
            break;

        case IP_WO:
            setupElementWrite(ELEMENT_FULL_WIDTH * KStars::Instance()->devicePixelRatio());
            break;
    }

    guiProp->addLayout(EHBox);
}

void INDI_E::setupElementLabel()
{
    QPalette palette;

    label_w = new KSqueezedTextLabel(this);
    label_w->setMinimumWidth(ELEMENT_LABEL_WIDTH * KStars::Instance()->devicePixelRatio());
    label_w->setMaximumWidth(ELEMENT_LABEL_WIDTH * KStars::Instance()->devicePixelRatio());
    label_w->setMargin(2);

    palette.setColor(label_w->backgroundRole(), QColor(224, 232, 238));
    label_w->setPalette(palette);
    label_w->setTextFormat(Qt::RichText);
    label_w->setAlignment(Qt::AlignCenter);
    label_w->setWordWrap(true);

    label_w->setStyleSheet("border: 1px solid grey; border-radius: 2px");

    if (label.length() > MAX_LABEL_LENGTH)
    {
        QFont tempFont(label_w->font());
        tempFont.setPointSize(tempFont.pointSize() - MED_INDI_FONT);
        label_w->setFont(tempFont);
    }

    label_w->setText(label);

    EHBox->addWidget(label_w);
}

void INDI_E::syncSwitch()
{
    if (sp == nullptr)
        return;

    QFont buttonFont;

    switch (guiProp->getGUIType())
    {
        case PG_BUTTONS:
            if (sp->s == ISS_ON)
            {
                push_w->setChecked(true);
                //push_w->setDown(true);
                buttonFont = push_w->font();
                buttonFont.setBold(true);
                push_w->setFont(buttonFont);

                if (dataProp.getPermission() == IP_RO)
                    push_w->setEnabled(true);
            }
            else
            {
                push_w->setChecked(false);
                //push_w->setDown(false);
                buttonFont = push_w->font();
                buttonFont.setBold(false);
                push_w->setFont(buttonFont);

                if (dataProp.getPermission() == IP_RO)
                    push_w->setEnabled(false);
            }
            break;

        case PG_RADIO:
            if (sp->s == ISS_ON)
            {
                check_w->setChecked(true);
                if (dataProp.getPermission() == IP_RO)
                    check_w->setEnabled(true);
            }
            else
            {
                check_w->setChecked(false);
                if (dataProp.getPermission() == IP_RO)
                    check_w->setEnabled(false);
            }
            break;

        default:
            break;
    }
}

void INDI_E::syncText()
{
    if (tp == nullptr)
        return;

    if (dataProp.getPermission() != IP_WO)
    {
        if (tp->text[0])
            read_w->setText(i18nc(libindi_strings_context, tp->text));
        else
            read_w->setText(tp->text);
    }
    // If write-only
    else
    {
        write_w->setText(tp->text);
    }
}

void INDI_E::syncNumber()
{
    char iNumber[MAXINDIFORMAT];
    if (np == nullptr || read_w == nullptr)
        return;

    numberFormat(iNumber, np->format, np->value);

    text = iNumber;

    read_w->setText(text);

    if (spin_w)
    {
        if (np->min != spin_w->minimum())
            setMin();
        if (np->max != spin_w->maximum())
            setMax();
    }
}

void INDI_E::updateTP()
{
    if (tp == nullptr)
        return;

    IUSaveText(tp, write_w->text().toHtmlEscaped().toLatin1().constData());
}

void INDI_E::updateNP()
{
    if (np == nullptr)
        return;

    if (write_w != nullptr)
    {
        if (write_w->text().isEmpty())
            return;

        f_scansexa(write_w->text().replace(',', '.').toLatin1().constData(), &(np->value));
        return;
    }

    if (spin_w != nullptr)
        np->value = spin_w->value();
}

void INDI_E::setText(const QString &newText)
{
    if (tp == nullptr)
        return;

    switch (dataProp.getPermission())
    {
        case IP_RO:
            read_w->setText(newText);
            break;

        case IP_WO:
        case IP_RW:
            text = newText;
            IUSaveText(tp, newText.toLatin1().constData());
            read_w->setText(newText);
            write_w->setText(newText);
            break;
    }
}

void INDI_E::setValue(double value)
{
    if (spin_w == nullptr || np == nullptr)
        return;
    // ensure that min <= value <= max
    if (value < np->min || value > np->max)
        return;

    spin_w->setValue(value);
    spinChanged(value);
}

void INDI_E::buildBLOB(IBLOB *ibp)
{
    name  = ibp->name;
    label = i18nc(libindi_strings_context, ibp->label);

    if (label == "(I18N_EMPTY_MESSAGE)")
        label = ibp->label;

    bp = ibp;

    if (label.isEmpty())
        label = i18nc(libindi_strings_context, ibp->name);

    if (label == "(I18N_EMPTY_MESSAGE)")
        label = ibp->name;

    setupElementLabel();

    text = i18n("INDI DATA STREAM");

    switch (dataProp.getPermission())
    {
        case IP_RW:
            setupElementRead(ELEMENT_READ_WIDTH * KStars::Instance()->devicePixelRatio());
            setupElementWrite(ELEMENT_WRITE_WIDTH * KStars::Instance()->devicePixelRatio());
            setupBrowseButton();
            break;

        case IP_RO:
            setupElementRead(ELEMENT_FULL_WIDTH * KStars::Instance()->devicePixelRatio());
            break;

        case IP_WO:
            setupElementWrite(ELEMENT_FULL_WIDTH * KStars::Instance()->devicePixelRatio());
            setupBrowseButton();
            break;
    }

    guiProp->addLayout(EHBox);
}

void INDI_E::buildNumber(INumber *inp)
{
    bool scale = false;
    char iNumber[MAXINDIFORMAT];

    name  = inp->name;
    label = i18nc(libindi_strings_context, inp->label);

    if (label == "(I18N_EMPTY_MESSAGE)")
        label = inp->label;

    np = inp;

    if (label.isEmpty())
        label = i18nc(libindi_strings_context, inp->name);

    if (label == "(I18N_EMPTY_MESSAGE)")
        label = inp->name;

    numberFormat(iNumber, np->format, np->value);
    text = iNumber;

    setupElementLabel();

    if (np->step != 0 && (np->max - np->min) / np->step <= 100)
        scale = true;

    switch (dataProp.getPermission())
    {
        case IP_RW:
            setupElementRead(ELEMENT_READ_WIDTH * KStars::Instance()->devicePixelRatio());
            if (scale)
                setupElementScale(ELEMENT_WRITE_WIDTH * KStars::Instance()->devicePixelRatio());
            else
                setupElementWrite(ELEMENT_WRITE_WIDTH * KStars::Instance()->devicePixelRatio());

            guiProp->addLayout(EHBox);
            break;

        case IP_RO:
            setupElementRead(ELEMENT_READ_WIDTH * KStars::Instance()->devicePixelRatio());
            guiProp->addLayout(EHBox);
            break;

        case IP_WO:
            if (scale)
                setupElementScale(ELEMENT_FULL_WIDTH * KStars::Instance()->devicePixelRatio());
            else
                setupElementWrite(ELEMENT_FULL_WIDTH * KStars::Instance()->devicePixelRatio());

            guiProp->addLayout(EHBox);

            break;
    }
}

void INDI_E::buildLight(ILight *ilp)
{
    name  = ilp->name;
    label = i18nc(libindi_strings_context, ilp->label);

    if (label == "(I18N_EMPTY_MESSAGE)")
        label = ilp->label;

    lp = ilp;

    if (label.isEmpty())
        label = i18nc(libindi_strings_context, ilp->name);

    if (label == "(I18N_EMPTY_MESSAGE)")
        label = ilp->name;

    led_w = new KLed(this);
    led_w->setMaximumSize(16, 16);
    led_w->setLook(KLed::Sunken);

    syncLight();

    EHBox->addWidget(led_w);

    setupElementLabel();

    guiProp->addLayout(EHBox);
}

void INDI_E::syncLight()
{
    if (lp == nullptr)
        return;

    switch (lp->s)
    {
        case IPS_IDLE:
            led_w->setColor(Qt::gray);
            break;

        case IPS_OK:
            led_w->setColor(Qt::green);
            break;

        case IPS_BUSY:
            led_w->setColor(Qt::yellow);
            break;

        case IPS_ALERT:
            led_w->setColor(Qt::red);
            break;
    }
}

void INDI_E::setupElementScale(int length)
{
    if (np == nullptr)
        return;

    int steps = static_cast<int>((np->max - np->min) / np->step);
    spin_w    = new QDoubleSpinBox(this);
    spin_w->setRange(np->min, np->max);
    spin_w->setSingleStep(np->step);
    spin_w->setValue(np->value);
    spin_w->setDecimals(3);

    slider_w = new QSlider(Qt::Horizontal, this);
    slider_w->setRange(0, steps);
    slider_w->setPageStep(1);
    slider_w->setValue(static_cast<int>((np->value - np->min) / np->step));

    connect(spin_w, SIGNAL(valueChanged(double)), this, SLOT(spinChanged(double)));
    connect(slider_w, SIGNAL(sliderMoved(int)), this, SLOT(sliderChanged(int)));

    if (length == ELEMENT_FULL_WIDTH  * KStars::Instance()->devicePixelRatio())
        spin_w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    else
        spin_w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    spin_w->setMinimumWidth(static_cast<int>(length * 0.45));
    slider_w->setMinimumWidth(static_cast<int>(length * 0.55));

    EHBox->addWidget(slider_w);
    EHBox->addWidget(spin_w);
}

void INDI_E::spinChanged(double value)
{
    int slider_value = static_cast<int>((value - np->min) / np->step);
    slider_w->setValue(slider_value);
}

void INDI_E::sliderChanged(int value)
{
    double spin_value = (value * np->step) + np->min;
    spin_w->setValue(spin_value);
}

void INDI_E::setMin()
{
    if (spin_w)
    {
        spin_w->setMinimum(np->min);
        spin_w->setValue(np->value);
    }
    if (slider_w)
    {
        slider_w->setMaximum(static_cast<int>((np->max - np->min) / np->step));
        slider_w->setMinimum(0);
        slider_w->setPageStep(1);
        slider_w->setValue(static_cast<int>((np->value - np->min) / np->step));
    }
}

void INDI_E::setMax()
{
    if (spin_w)
    {
        spin_w->setMaximum(np->max);
        spin_w->setValue(np->value);
    }
    if (slider_w)
    {
        slider_w->setMaximum(static_cast<int>((np->max - np->min) / np->step));
        slider_w->setMinimum(0);
        slider_w->setPageStep(1);
        slider_w->setValue(static_cast<int>((np->value - np->min) / np->step));
    }
}

void INDI_E::setupElementWrite(int length)
{
    write_w = new QLineEdit(this);
    write_w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    write_w->setMinimumWidth(length);
    write_w->setMaximumWidth(length);

    write_w->setText(text);

    QObject::connect(write_w, SIGNAL(returnPressed()), guiProp, SLOT(sendText()));
    EHBox->addWidget(write_w);
}

void INDI_E::setupElementRead(int length)
{
    read_w = new QLineEdit(this);
    read_w->setMinimumWidth(length);
    read_w->setFocusPolicy(Qt::NoFocus);
    read_w->setCursorPosition(0);
    read_w->setAlignment(Qt::AlignCenter);
    read_w->setReadOnly(true);
    read_w->setText(text);

    EHBox->addWidget(read_w);
}

void INDI_E::setupBrowseButton()
{
    browse_w = new QPushButton(this);
    browse_w->setIcon(QIcon::fromTheme("document-open"));
    browse_w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    browse_w->setMinimumWidth(MIN_SET_WIDTH  * KStars::Instance()->devicePixelRatio());
    browse_w->setMaximumWidth(MAX_SET_WIDTH  * KStars::Instance()->devicePixelRatio());

    EHBox->addWidget(browse_w);
    QObject::connect(browse_w, SIGNAL(clicked()), this, SLOT(browseBlob()));
}

void INDI_E::browseBlob()
{
    QFile fp;
    QString filename;
    QString format;
    int pos = 0;
    QUrl currentURL;

    currentURL = QFileDialog::getOpenFileUrl();

    // if user presses cancel
    if (currentURL.isEmpty())
        return;

    if (currentURL.isValid())
        write_w->setText(currentURL.toLocalFile());

    fp.setFileName(currentURL.toLocalFile());

    if ((pos = filename.lastIndexOf(".")) != -1)
        format = filename.mid(pos, filename.length());

    //qDebug() << "Filename is " << fp.fileName() << endl;

    if (!fp.open(QIODevice::ReadOnly))
    {
        KSNotification::error( i18n("Cannot open file %1 for reading", filename));
        return;
    }

    bp->bloblen = bp->size = fp.size();

    bp->blob = static_cast<uint8_t *>(realloc(bp->blob, bp->size));
    if (bp->blob == nullptr)
    {
        KSNotification::error( i18n("Not enough memory for file %1", filename));
        fp.close();
        return;
    }

    memcpy(bp->blob, fp.readAll().constData(), bp->size);

    blobDirty = true;
}

QString INDI_E::getWriteField()
{
    if (write_w)
        return write_w->text();
    else
        return nullptr;
}

QString INDI_E::getReadField()
{
    if (read_w)
        return read_w->text();
    else
        return nullptr;
}
