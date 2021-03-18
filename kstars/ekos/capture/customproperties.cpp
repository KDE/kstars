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

    addB->setIcon(QIcon::fromTheme("go-next"));
    addB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    removeB->setIcon(QIcon::fromTheme("go-previous"));
    removeB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    clearB->setIcon(QIcon::fromTheme("edit-clear"));
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

    const QStringList skipProperties = QStringList() << "CCD_TEMPERATURE" << "CCD_FRAME" << "CCD_EXPOSURE"
                                       << "CCD_BINNING" << "GUIDER_FRAME" << "GUIDER_BINNING"
                                       << "GUIDER_EXPOSURE" << "FILTER_SLOT" << "TELESCOPE_TIMED_GUIDE_NS"
                                       << "TELESCOPE_TIMED_GUIDE_WE";

    for (auto &property : *currentCCD->getProperties())
    {
        const QString name = property->getName();
        // Skip empty properties
        if (name.isEmpty())
            continue;

        if (property->getType() == INDI_NUMBER && property->getPermission() != IP_RO && skipProperties.contains(name) == false)
            props << property->getLabel();
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

    for (int i = 0; i < jobPropertiesList->count(); i++)
    {
        QString numberLabel = jobPropertiesList->item(i)->text();

        // Match against existing properties
        for(auto &indiProp : *currentCCD->getProperties())
        {
            // If label matches then we have the property
            if (indiProp->getType() == INDI_NUMBER && QString(indiProp->getLabel()) == numberLabel)
            {
                QMap<QString, double> numberProperty;

                INumberVectorProperty *np = indiProp->getNumber();
                for (int i = 0; i < np->nnp; i++)
                    numberProperty[np->np[i].name] = np->np[i].value;

                newMap[np->name] = numberProperty;

                break;
            }
        }
    }

    customProperties = newMap;
    close();
    emit valueChanged();
}
