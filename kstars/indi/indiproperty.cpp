/*  INDI Property
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.


 */

 #include "indiproperty.h"
 #include "indigroup.h"
 #include "indidevice.h"
 #include "devicemanager.h"
 #include "indimenu.h"
 #include "indistd.h"
 #include "Options.h"
 #include "kstars.h"
 #include "dialogs/timedialog.h"
 #include "skymap.h"

 #include <base64.h>

 #include <kmenu.h>
 #include <klineedit.h>
 #include <kled.h>
 #include <klocale.h>
 #include <kcombobox.h>
 #include <kpushbutton.h>
 #include <knuminput.h>
 #include <kdebug.h>
 #include <kmessagebox.h>

 #include <QButtonGroup>
 #include <QCheckBox>
 #include <QLabel>
 #include <QTimer>
 #include <QFile>
 #include <QDataStream>
 #include <QFrame>
 #include <QHBoxLayout>
 #include <QVBoxLayout>
 #include <QAbstractButton>
 #include <QAction>

 #include <unistd.h>
 #include <stdlib.h>
 #include <assert.h>

/*******************************************************************
** INDI Property: contains widgets, labels, and their status
*******************************************************************/
INDI_P::INDI_P(INDI_G *parentGroup, const QString &inName)
{
    name = inName;

    pg = parentGroup;

    //  el.setAutoDelete(true);

    stdID 	  = -1;

    indistd 	  = new INDIStdProperty(this, pg->dp->parent->ksw, pg->dp->stdDev);

    PHBox           = new QHBoxLayout();
    PHBox->setMargin(0);
    PHBox->setSpacing(KDialog::spacingHint());
    PVBox           = new QVBoxLayout();
    PVBox->setMargin(0);
    PVBox->setSpacing(KDialog::spacingHint());
    light           = NULL;
    label_w         = NULL;
    set_w           = NULL;
    groupB          = NULL;
}

/* INDI property desstructor, makes sure everything is "gone" right */
INDI_P::~INDI_P()
{
    while ( ! el.isEmpty() ) delete el.takeFirst();
    pg->propertyLayout->removeItem(PHBox);
    delete (light);
    delete (label_w);
    delete (set_w);
    delete (PHBox);
    delete (indistd);
    delete (groupB);
}

bool INDI_P::isOn(const QString &component)
{

    INDI_E *lp;

    lp = findElement(component);


    if (!lp)
        return false;

    if (lp->check_w && lp->check_w->isChecked())
        return true;

    if (lp->push_w && lp->state == PS_ON)
        return true;

    return false;
}

INDI_E * INDI_P::findElement(const QString &elementName)
{
    for ( int i=0; i < el.size(); ++i )
        if (el[i]->name == elementName || el[i]->label == elementName)
            return el[i];

    return NULL;
}

void INDI_P::drawLt(PState lstate)
{


    /* set state light */
    switch (lstate)
    {
    case PS_IDLE:
        light->setColor(Qt::gray);
        break;

    case PS_OK:
        light->setColor(Qt::green);
        emit okState();
        disconnect( this, SIGNAL(okState()), 0, 0 );
        break;

    case PS_BUSY:
        light->setColor(Qt::yellow);
        break;

    case PS_ALERT:
        light->setColor(Qt::red);
        break;

    default:
        break;

    }

}

void INDI_P::newText()
{

    foreach(INDI_E *lp, el)
    {
        /* If PG_SCALE */
        if (lp->spin_w)
            lp->targetValue = lp->spin_w->value();
        /* PG_NUMERIC or PG_TEXT */
        else
        {
            switch (perm)
            {
            case PP_RW:
                // FIXME is this problematic??
                if (lp->write_w->text().isEmpty())
                    lp->text = lp->read_w->text();
                else
                    lp->text = lp->write_w->text();
                break;

            case PP_RO:
                break;

            case PP_WO:
                // Ignore if it's empty
                if (lp->write_w->text().isEmpty())
                    return;

                lp->text = lp->write_w->text();
                break;
            }

            /*if (guitype == PG_NUMERIC)
            {
                f_scansexa(lp->text.toAscii(), &(lp->targetValue));
                if ((lp->targetValue > lp->max || lp->targetValue < lp->min))
                {
                    KMessageBox::error(0, i18n("Invalid range for element %1. Valid range is from %2 to %3", lp->label, lp->min, lp->max));
                    return;
                }
            }*/
        }
    }

    state = PS_BUSY;

    drawLt(state);

    /* perform any std functions */
    indistd->newText();

    if (guitype == PG_TEXT)
        pg->dp->deviceManager->sendNewText(this);
    else if (guitype == PG_NUMERIC)
        pg->dp->deviceManager->sendNewNumber(this);

}

void INDI_P::newAbstractButton(QAbstractButton *button)
{
    foreach(INDI_E *lp, el)
    {
        if (lp->push_w->text() == button->text())
        {
            newSwitch(lp);
            break;
        }
    }
}

void INDI_P::newComboBoxItem(const QString &item)
{
    foreach(INDI_E *lp, el)
    {
        if (lp->label == item)
        {
            newSwitch(lp);
            break;
        }
    }
}

void INDI_P::newSwitch(INDI_E *lp)
{
    QFont buttonFont;

    assert(lp != NULL);

    switch (guitype)
    {
    case PG_MENU:
        if (lp->state == PS_ON)
            return;

        foreach( INDI_E *elm, el)
        elm->state = PS_OFF;

        lp->state = PS_ON;
        break;

    case PG_BUTTONS:

        foreach( INDI_E *elm, el)
        {
            if (elm == lp)
                continue;

            elm->push_w->setDown(false);
            buttonFont = elm->push_w->font();
            buttonFont.setBold(false);
            elm->push_w->setFont(buttonFont);
            elm->state = PS_OFF;
        }

        lp->push_w->setDown(true);
        buttonFont = lp->push_w->font();
        buttonFont.setBold(true);
        lp->push_w->setFont(buttonFont);
        lp->state = PS_ON;

        break;

    case PG_RADIO:
        lp->state = lp->state == PS_ON ? PS_OFF : PS_ON;
        lp->check_w->setChecked(lp->state == PS_ON);
        break;

    default:
        break;

    }
    state = PS_BUSY;

    drawLt(state);

    if (indistd->newSwitch(lp))
        return;

    pg->dp->deviceManager->sendNewSwitch (this, lp);
}

/* Display file dialog to select and upload a file to the client
   Loop through blobs and send each accordingly
 */
void INDI_P::newBlob()
{
    QFile fp;
    QString filename;
    QString format;
    QDataStream binaryStream;
    int data64_size=0, pos=0;
    unsigned char *data_file;
    unsigned char *data64;
    bool sending (false);
    bool valid (true);

    for (int i=0; i < el.size(); i++)
    {
        filename = el[i]->write_w->text();
        if (filename.isEmpty())
        {
            valid = false;
            continue;
        }

        fp.setFileName(filename);

        if ( (pos = filename.lastIndexOf(".")) != -1)
            format = filename.mid (pos, filename.length());

        if (!fp.open(QIODevice::ReadOnly))
        {
            KMessageBox::error(0, i18n("Cannot open file %1 for reading", filename));
            valid = false;
            continue;
        }

        binaryStream.setDevice(&fp);

        data_file = new unsigned char[fp.size()];
        if (data_file == NULL)
        {
            KMessageBox::error(0, i18n("Not enough memory to load %1", filename));
            fp.close();
            valid = false;
            continue;
        }

        binaryStream.readRawData((char*)data_file, fp.size());

        data64 = new unsigned char[4*fp.size()/3+4];
        if (data64 == NULL)
        {
            KMessageBox::error(0, i18n("Not enough memory to convert file %1 to base64", filename));
            fp.close();
            valid = false;
            continue;
        }

        data64_size = to64frombits (data64, data_file, fp.size());

        delete [] data_file;

        if (sending == false)
        {
            sending = true;
//            pg->dp->deviceManager->startBlob (pg->dp->name, name, QString(timestamp()));
        }

        pg->dp->deviceManager->sendOneBlob(el[i]->name, data64_size, format, data64);

        fp.close();
        delete [] data64;
    }

    /* Nothing been made, no changes */
    if (!sending && !valid)
        return;
    else if (sending)
        pg->dp->deviceManager->finishBlob();

    if (valid)
        state = PS_BUSY;
    else
        state = PS_ALERT;

    drawLt(state);
}

/* build widgets for property pp using info in root.
 */
void INDI_P::addGUI (XMLEle *root)
{
    XMLAtt *prompt;
    QString errmsg;

    /* add to GUI group */
    light = new KLed (pg->propertyContainer);
    light->setMaximumSize(16,16);
    light->setLook(KLed::Sunken);
    //light->setShape(KLed::Rectangular);
    drawLt(state);

    /* #1 First widegt is the LED status indicator */
    PHBox->addWidget(light);

    /* #2 add label for prompt */
    prompt = findAtt(root, "label", errmsg);

    if (!prompt)
        label = name;
    else
        label = valuXMLAtt(prompt);

    // use property name if label is empty
    if (label.isEmpty())
    {
        label = name;
        label_w = new QLabel(label, pg->propertyContainer);
    }
    else
        label_w = new QLabel(label, pg->propertyContainer);

    label_w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    label_w->setFrameShape( QFrame::StyledPanel );
    label_w->setMinimumWidth(PROPERTY_LABEL_WIDTH);
    label_w->setMaximumWidth(PROPERTY_LABEL_WIDTH);
    label_w->setTextFormat( Qt::RichText );
    label_w->setAlignment(Qt::AlignVCenter | Qt::AlignLeft );
    label_w->setWordWrap(true);

    PHBox->addWidget(label_w);

    light->show();
    label_w->show();

    /* #3 Add the Vertical layout thay may contain several elements */
    PHBox->addLayout(PVBox);
}

int INDI_P::buildTextGUI(XMLEle *root, QString & errmsg)
{
    INDI_E *lp;
    XMLEle *text;
    XMLAtt *ap;
    QString textName, textLabel;

    for (text = nextXMLEle (root, 1); text != NULL; text = nextXMLEle (root, 0))
    {
        if (strcmp (tagXMLEle(text), "defText"))
            continue;

        ap = findXMLAtt(text, "name");
        if (!ap)
        {
            errmsg = QString("Error: unable to find attribute 'name' for property %1").arg(name);
            return DeviceManager::INDI_PROPERTY_INVALID;
        }

        textName = valuXMLAtt(ap);
        textName.truncate(MAXINDINAME);

        ap = findXMLAtt(text, "label");

        if (!ap)
        {
            errmsg = QString("Error: unable to find attribute 'label' for property %1").arg(name);
            return (-1);
        }

        textLabel = valuXMLAtt(ap);


        if (textLabel.isEmpty())
            textLabel = textName;

        textLabel.truncate(MAXINDINAME);

        lp = new INDI_E(this, textName, textLabel);

        lp->buildTextGUI(QString(pcdataXMLEle(text)));

        el.append(lp);

    }

    if (perm == PP_RO)
        return 0;

    // INDI STD, but we use our own controls
    if (name == "TIME_UTC")
    {
        setupSetButton("Time");
        QObject::connect(set_w, SIGNAL(clicked()), indistd, SLOT(newTime()));
    }
    else
    {
        setupSetButton("Set");
        QObject::connect(set_w, SIGNAL(clicked()), this, SLOT(newText()));
    }

    return 0;

}

int INDI_P::buildNumberGUI  (XMLEle *root, QString & errmsg)
{
    char format[32];
    double min=0, max=0, step=0;
    XMLEle *number;
    XMLAtt *ap;
    INDI_E *lp;
    QString numberName, numberLabel;

    for (number = nextXMLEle (root, 1); number != NULL; number = nextXMLEle (root, 0))
    {
        if (strcmp (tagXMLEle(number), "defNumber"))
            continue;

        ap = findXMLAtt(number, "name");

        if (!ap)
        {
            errmsg = QString("Error: unable to find attribute 'name' for property %1").arg(name);
            return DeviceManager::INDI_PROPERTY_INVALID;
        }

        numberName = valuXMLAtt(ap);
        numberName.truncate(MAXINDINAME);

        ap = findXMLAtt(number, "label");

        if (!ap)
        {
            errmsg = QString("Error: unable to find attribute 'label' for property %1").arg(name);
            return DeviceManager::INDI_PROPERTY_INVALID;
        }

        numberLabel = valuXMLAtt(ap);

        if (numberLabel.isEmpty())
            numberLabel = numberName;

        numberLabel.truncate(MAXINDINAME);

        lp = new INDI_E(this, numberName, numberLabel);

        ap = findXMLAtt (number, "min");
        if (ap)
            min = atof(valuXMLAtt(ap));
        ap = findXMLAtt (number, "max");
        if (ap)
            max = atof(valuXMLAtt(ap));
        ap = findXMLAtt (number, "step");
        if (ap)
            step = atof(valuXMLAtt(ap));
        ap = findXMLAtt (number, "format");
        if (ap)
            strcpy(format,valuXMLAtt(ap));

        lp->initNumberValues(min, max, step, format);

        lp->buildNumberGUI(atof(pcdataXMLEle(number)));

        el.append(lp);

    }

    if (perm == PP_RO)
        return 0;


    if (name == "GEOGRAPHIC_COORD")
    {
        setupSetButton("Update");
        QObject::connect(set_w, SIGNAL(clicked()), indistd->stdDev, SLOT(updateLocation()));
    }
    else
    {

        if (name == "CCD_EXPOSURE")
            setupSetButton(i18n("Capture Image"));
        else
            setupSetButton(i18nc("Set a value", "Set"));

        QObject::connect(set_w, SIGNAL(clicked()), this, SLOT(newText()));
    }

    return (0);
}


void INDI_P::setupSetButton(const QString &caption)
{
    set_w = new QPushButton(caption, pg->propertyContainer);
    set_w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    set_w->setMinimumWidth( MIN_SET_WIDTH );
    set_w->setMaximumWidth( MAX_SET_WIDTH );

    PHBox->addWidget(set_w);
}

int INDI_P::buildMenuGUI(XMLEle *root, QString & errmsg)
{
    XMLEle *sep = NULL;
    XMLAtt *ap;
    INDI_E *lp;
    QString switchName, switchLabel;
    QStringList menuOptions;
    int i=0, onItem=-1;

    guitype = PG_MENU;

    // build pulldown menu first
    // create each switch.
    // N.B. can only be one in On state.
    for (sep = nextXMLEle (root, 1), i=0; sep != NULL; sep = nextXMLEle (root, 0), ++i)
    {
        /* look for switch tage */
        if (strcmp (tagXMLEle(sep), "defSwitch"))
            continue;

        /* find name  */
        ap = findAtt (sep, "name", errmsg);
        if (!ap)
            return DeviceManager::INDI_PROPERTY_INVALID;

        switchName = valuXMLAtt(ap);
        switchName.truncate(MAXINDINAME);

        /* find label */
        ap = findAtt (sep, "label", errmsg);
        if (!ap)
	   return DeviceManager::INDI_PROPERTY_INVALID;

        switchLabel = valuXMLAtt(ap);

        if (switchLabel.isEmpty())
            switchLabel = switchName;

        switchLabel.truncate(MAXINDINAME);

        lp = new INDI_E(this, switchName, switchLabel);

        if (pg->dp->crackSwitchState (pcdataXMLEle(sep), &(lp->state)) < 0)
        {
            errmsg = QString("INDI: <%1> unknown state %2 for %3 %4 %5").arg(tagXMLEle(root)).arg(valuXMLAtt(ap)).arg(name).arg(lp->name).arg(name);
            return DeviceManager::INDI_PROPERTY_INVALID;
        }

        menuOptions.append(switchLabel);

        if (lp->state == PS_ON)
        {
            if (onItem != -1)
            {
                errmsg = QString("INDI: <%1> %2 %3 has multiple On switches").arg(tagXMLEle(root)).arg(name).arg(lp->name);
                return DeviceManager::INDI_PROPERTY_INVALID;
            }

            onItem = i;
        }

        el.append(lp);
    }

    om_w = new KComboBox(pg->propertyContainer);
    om_w->addItems(menuOptions);
    om_w->setCurrentIndex(onItem);

    HorSpacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );

    PHBox->addWidget(om_w);
    PHBox->addItem(HorSpacer);

    QObject::connect(om_w, SIGNAL( activated ( const QString &)), this, SLOT(newComboBoxItem( const QString &)));

    return (0);
}

int INDI_P::buildSwitchesGUI(XMLEle *root, QString & errmsg)
{
    XMLEle *sep;
    XMLAtt *ap;
    INDI_E *lp;
    KPushButton *button(NULL);
    QCheckBox   *checkbox;
    QFont buttonFont;
    QString switchName, switchLabel;
    int j;

    groupB = new QButtonGroup(0);
    //groupB->setFrameShape(QtFrame::NoFrame);
    if (guitype == PG_BUTTONS)
        groupB->setExclusive(true);

    QObject::connect(groupB, SIGNAL(buttonClicked(QAbstractButton *)), this, SLOT(newAbstractButton(QAbstractButton *)));

    for (sep = nextXMLEle (root, 1), j=-1; sep != NULL; sep = nextXMLEle (root, 0))
    {
        /* look for switch tage */
        if (strcmp (tagXMLEle(sep), "defSwitch"))
            continue;

        /* find name  */
        ap = findAtt (sep, "name", errmsg);
        if (!ap)
            return DeviceManager::INDI_PROPERTY_INVALID;

        switchName = valuXMLAtt(ap);
        switchName.truncate(MAXINDINAME);

        /* find label */
        ap = findAtt (sep, "label", errmsg);
        if (!ap)
            return DeviceManager::INDI_PROPERTY_INVALID;

        switchLabel = valuXMLAtt(ap);

        if (switchLabel.isEmpty())
            switchLabel = switchName;

        switchLabel.truncate(MAXINDINAME);

        lp = new INDI_E(this, switchName, switchLabel);

        if (pg->dp->crackSwitchState (pcdataXMLEle(sep), &(lp->state)) < 0)
        {
            errmsg = QString("INDI: <%1> unknown state %2 for %3 %4 %5").arg(tagXMLEle(root)).arg(valuXMLAtt(ap)).arg(name).arg(name).arg(lp->name);
            return DeviceManager::INDI_PROPERTY_INVALID;
        }

        j++;

        /* build toggle */
        switch (guitype)
        {
        case PG_BUTTONS:
            button = new KPushButton(switchLabel, pg->propertyContainer);

            //groupB->insert(button, j);
            groupB->addButton(button);

            if (lp->state == PS_ON)
            {
                button->setDown(true);
                buttonFont = button->font();
                buttonFont.setBold(true);
                button->setFont(buttonFont);
            }

            lp->push_w = button;

            PHBox->addWidget(button);

            button->show();

            break;

        case PG_RADIO:
            checkbox = new QCheckBox(switchLabel, pg->propertyContainer);
            //groupB->insert(checkbox, j);
            groupB->addButton(button);

            if (lp->state == PS_ON)
                checkbox->setChecked(true);

            lp->check_w = checkbox;

            PHBox->addWidget(checkbox);

            checkbox->show();

            break;

        default:
            break;

        }

        el.append(lp);
    }

    if (j < 0)
        return DeviceManager::INDI_PROPERTY_INVALID;

    HorSpacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );

    PHBox->addItem(HorSpacer);

    return (0);
}

int INDI_P::buildLightsGUI(XMLEle *root, QString & errmsg)
{
    XMLEle *lep;
    XMLAtt *ap;
    INDI_E *lp;
    QString sname, slabel;

    for (lep = nextXMLEle (root, 1); lep != NULL; lep = nextXMLEle (root, 0))
    {
        if (strcmp (tagXMLEle(lep), "defLight"))
            continue;

        /* find name  */
        ap = findAtt (lep, "name", errmsg);
        if (!ap) return DeviceManager::INDI_PROPERTY_INVALID;

        sname = valuXMLAtt(ap);
        sname.truncate(MAXINDINAME);

        /* find label */
        ap = findAtt (lep, "label", errmsg);
        if (!ap) return DeviceManager::INDI_PROPERTY_INVALID;

        slabel = valuXMLAtt(ap);
        if (slabel.isEmpty())
            slabel = sname;

        slabel.truncate(MAXINDINAME);

        lp = new INDI_E(this, sname, slabel);

        if (pg->dp->crackLightState (pcdataXMLEle(lep), &lp->state) < 0)
        {
            errmsg = QString("INDI: <%1> unknown state %2 for %3 %4 %5").arg(tagXMLEle(root)).arg(valuXMLAtt(ap)).arg(pg->dp->name).arg(name).arg(sname);
            return DeviceManager::INDI_PROPERTY_INVALID;
        }

        lp->buildLightGUI();
        el.append(lp);
    }

    HorSpacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );

    PHBox->addItem(HorSpacer);

    return (0);
}

/* Build BLOB GUI
 * Return 0 if okay, -1 if error */
int INDI_P::buildBLOBGUI(XMLEle *root, QString & errmsg)
{
    INDI_E *lp;
    XMLEle *blob;
    XMLAtt *ap;
    QString blobName, blobLabel ;

    for (blob = nextXMLEle (root, 1); blob != NULL; blob = nextXMLEle (root, 0))
    {
        if (strcmp (tagXMLEle(blob), "defBLOB"))
            continue;

        ap = findXMLAtt(blob, "name");
        if (!ap)
        {
            errmsg = QString("Error: unable to find attribute 'name' for property %1").arg(name);
            return DeviceManager::INDI_PROPERTY_INVALID;
        }

        blobName = valuXMLAtt(ap);
        blobName.truncate(MAXINDINAME);

        ap = findXMLAtt(blob, "label");

        if (!ap)
        {
            errmsg = QString("Error: unable to find attribute 'label' for property %1").arg(name);
            return DeviceManager::INDI_PROPERTY_INVALID;
        }

        blobLabel = valuXMLAtt(ap);

        if (blobLabel.isEmpty())
            blobLabel = blobName;

        blobLabel.truncate(MAXINDINAME);

        lp = new INDI_E(this, blobName, blobLabel);

        lp->buildBLOBGUI();

        el.append(lp);

    }

    enableBLOBC = new QCheckBox();
    enableBLOBC->setIcon(KIcon("modem"));
    enableBLOBC->setChecked(true);
    enableBLOBC->setToolTip(i18n("Enable binary data transfer from this property to KStars and vice-versa."));

    PHBox->addWidget(enableBLOBC);

    connect(enableBLOBC, SIGNAL(stateChanged(int)), this, SLOT(setBLOBOption(int)));

    if (perm != PP_RO)
    {
        setupSetButton(i18n("Upload"));
        QObject::connect(set_w, SIGNAL(clicked()), this, SLOT(newBlob()));
    }



    return 0;

}

void INDI_P::activateSwitch(const QString &name)
{
    foreach (INDI_E *lp, el)
    {
        if (lp->name == name && lp->push_w)
        {
            newSwitch(lp);
            break;
        }
    }
}

void INDI_P::setBLOBOption(int state)
{
    if (pg->dp->deviceManager== NULL)
        return;

    if (state == Qt::Checked)
        pg->dp->deviceManager->enableBLOB(true, pg->dp->name, name);
    else
        pg->dp->deviceManager->enableBLOB(false, pg->dp->name, name);
}

#include "indiproperty.moc"
