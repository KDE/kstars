/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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

    connect(addB, &QPushButton::clicked, this, &CustomProperties::slotAdd);
    connect(removeB, &QPushButton::clicked, this, &CustomProperties::slotRemove);
    connect(clearB, &QPushButton::clicked, this, &CustomProperties::slotClear);

    connect(buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(slotApply()));
}

void CustomProperties::setCCD(ISD::Camera *ccd)
{
    currentCCD = ccd;
    syncProperties();
}

void CustomProperties::syncProperties()
{
    availablePropertiesList->clear();
    QStringList props;

    const QStringList skipProperties = QStringList()
                                       << "CONNECTION"
                                       << "DEBUG"
                                       << "SIMULATION"
                                       << "CONFIG_PROCESS"
                                       << "CCD_TEMPERATURE"
                                       << "CCD_FRAME"
                                       << "CCD_EXPOSURE"
                                       << "CCD_BINNING"
                                       << "FRAME_TYPE"
                                       << "CCD_EXPOSURE_ABORT"
                                       << "GUIDER_FRAME"
                                       << "GUIDER_BINNING"
                                       << "GUIDER_EXPOSURE"
                                       << "GUIDER_EXPOSURE_ABORT"
                                       << "GUIDER_FRAME_TYPE"
                                       << "FILTER_SLOT"
                                       << "CCD_FRAME_RESET"
                                       << "WCS_CONTROL"
                                       << "UPLOAD_MODE"
                                       << "UPLOAD_SETTINGS"
                                       << "CCD_FILE_PATH"
                                       << "CCD_FAST_COUNT"
                                       << "ACTIVE_DEVICES"
                                       << "DEBUG_LEVEL"
                                       << "LOGGING_LEVEL"
                                       << "LOG_OUTPUT"
                                       << "FILE_DEBUG"
                                       << "EQUATORIAL_EOD_COORD"
                                       << "TARGET_EOD_COORD"
                                       << "TELESCOPE_TIMED_GUIDE_NS"
                                       << "TELESCOPE_TIMED_GUIDE_WE";


    for (auto &property : *currentCCD->getProperties())
    {
        const QString name = property->getName();
        // Skip empty properties
        if (name.isEmpty())
            continue;

        if (skipProperties.contains(name) ||
                property->getPermission() == IP_RO ||
                property->getType() == INDI_BLOB || property->getType() == INDI_LIGHT)
            continue;

        props << property->getLabel();
    }


    props.removeDuplicates();
    props.sort();
    availablePropertiesList->addItems(props);
}

QMap<QString, QMap<QString, QVariant> > CustomProperties::getCustomProperties() const
{
    return customProperties;
}

void CustomProperties::setCustomProperties(const QMap<QString, QMap<QString, QVariant> > &value)
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

    // Remove any keys in the list from custom properties
    for (int i = 0; i < jobPropertiesList->count(); i++)
        customProperties.remove(jobPropertiesList->item(i)->text());

    // Start from existing properties not in the list (external ones set by Ekos e.g. CCD_GAIN)
    QMap<QString, QMap<QString, QVariant> > newMap = customProperties;

    for (int i = 0; i < jobPropertiesList->count(); i++)
    {
        auto label = jobPropertiesList->item(i)->text();

        // Match against existing properties
        for(auto &oneProperty : *currentCCD->getProperties())
        {
            // Search by label
            if (label != oneProperty->getLabel())
                continue;

            // Now get all the elements for this property
            QMap<QString, QVariant> elements;

            switch (oneProperty->getType())
            {
                case INDI_SWITCH:
                    for (const auto &oneSwitch : *oneProperty->getSwitch())
                    {
                        elements[oneSwitch.getName()] = oneSwitch.getState();
                    }
                    break;
                case INDI_TEXT:
                    for (const auto &oneText : *oneProperty->getText())
                    {
                        elements[oneText.getName()] = oneText.getText();
                    }
                    break;
                case INDI_NUMBER:
                    for (const auto &oneNumber : *oneProperty->getNumber())
                    {
                        elements[oneNumber.getName()] = oneNumber.getValue();
                    }
                    break;

                default:
                    continue;
            }

            newMap[oneProperty->getName()] = elements;

            break;
        }
    }

    customProperties = newMap;
    close();
    emit valueChanged();
}
