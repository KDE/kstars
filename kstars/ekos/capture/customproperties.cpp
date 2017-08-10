/*  Custom Properties
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "customproperties.h"

CustomProperties::CustomProperties()
{
    setupUi(this);

    addB->setIcon(QIcon::fromTheme("go-next", QIcon(":/icons/breeze/default/go-next.svg")));
    addB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    removeB->setIcon(QIcon::fromTheme("go-previous", QIcon(":/icons/breeze/default/go-previous.svg")));
    removeB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    clearB->setIcon(QIcon::fromTheme("edit-clear", QIcon(":/icons/breeze/default/edit-clear.svg")));
    clearB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    connect(addB, SIGNAL(clicked()), this, SLOT(slotAdd()));
    connect(removeB, SIGNAL(clicked()), this, SLOT(slotRemove()));
    connect(clearB, SIGNAL(clicked()), this, SLOT(slotClear()));

    connect(buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(slotApply()));
}

void CustomProperties::setCCD(ISD::CCD *ccd)
{
    currentCCD = ccd;

    syncProperties();
}

void CustomProperties::syncProperties()
{
    availablePropertiesList->clear();
    availablePropertiesList->clear();
    QStringList props;

    for (auto &property : currentCCD->getProperties())
    {
        if (property->getType() == INDI_NUMBER && property->getPermission() != IP_RO)
        {
            if (!strcmp(property->getName(), "CCD_TEMPERATURE") || !strcmp(property->getName(), "CCD_FRAME") ||
                !strcmp(property->getName(), "CCD_EXPOSURE") || !strcmp(property->getName(), "CCD_BINNING") ||
                !strcmp(property->getName(), "GUIDER_FRAME") || !strcmp(property->getName(), "GUIDER_BINNING") ||
                !strcmp(property->getName(), "GUIDER_EXPOSURE") || !strcmp(property->getName(), "FILTER_SLOT") ||
                !strcmp(property->getName(), "TELESCOPE_TIMED_GUIDE_NS") || !strcmp(property->getName(), "TELESCOPE_TIMED_GUIDE_WE"))
                continue;

            props << property->getLabel();
        }
    }


    props.removeDuplicates();
    availablePropertiesList->addItems(props);
}

QMap<QString, QMap<QString, double> > CustomProperties::getCustomProperties() const
{
    return customProperties;
}

void CustomProperties::setCustomProperties(const QMap<QString, QMap<QString, double> > &value)
{
    customProperties = value;
}

void CustomProperties::slotAdd()
{
    if (availablePropertiesList->selectedItems().isEmpty() == false)
    {
        QString prop = availablePropertiesList->selectedItems().first()->text();
        if (jobPropertiesList->findItems(prop, Qt::MatchExactly).isEmpty())
            jobPropertiesList->addItem(prop);
    }
}

void CustomProperties::slotRemove()
{
    if (jobPropertiesList->selectedItems().isEmpty() == false)
    {
        QModelIndex i = jobPropertiesList->selectionModel()->currentIndex();
        jobPropertiesList->model()->removeRow(i.row());
    }
}

void CustomProperties::slotClear()
{
    jobPropertiesList->clear();
}

void CustomProperties::slotApply()
{
    if (currentCCD == nullptr)
        return;

    // Reset job custom properties first
    QMap<QString, QMap<QString, double> > newMap;

    for (int i=0; i < jobPropertiesList->count(); i++)
    {
        QString numberLabel = jobPropertiesList->item(i)->text();

        // Match against existing properties
        for(auto &indiProp : currentCCD->getProperties())
        {
            // If label matches then we have the property
            if (indiProp->getType() == INDI_NUMBER && QString(indiProp->getLabel()) == numberLabel)
            {
                QMap<QString, double> numberProperty;

                INumberVectorProperty *np = indiProp->getNumber();
                for (int i=0; i < np->nnp; i++)
                    numberProperty[np->np[i].name] = np->np[i].value;

                newMap[np->name] = numberProperty;

                break;
            }
       }
    }

    customProperties = newMap;

    close();
}
