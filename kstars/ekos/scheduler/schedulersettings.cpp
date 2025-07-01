/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scheduler.h"

#include "Options.h"
#include "kstars.h"
#include "schedulerjob.h"
#include "schedulerprocess.h"
#include "schedulermodulestate.h"
#include "schedulerutils.h"

#include <KConfigDialog>

#include <ekos_scheduler_debug.h>

namespace Ekos
{

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Scheduler::loadGlobalSettings()
{
    QString key;
    QVariant value;

    QVariantMap settings;
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid() && oneWidget->count() > 0)
        {
            oneWidget->setCurrentText(value.toString());
            settings[key] = value;
        }
        else
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Option" << key << "not found!";
    }

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setValue(value.toDouble());
            settings[key] = value;
        }
        else
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Option" << key << "not found!";
    }

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setValue(value.toInt());
            settings[key] = value;
        }
        else
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Option" << key << "not found!";
    }

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setChecked(value.toBool());
            settings[key] = value;
        }
        else
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Option" << key << "not found!";
    }

    // All Line Edits
    for (auto &oneWidget : findChildren<QLineEdit*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setText(value.toString());
            settings[key] = value;

            if (key == "sequenceEdit")
                setSequence(value.toString());
            else if (key == "schedulerStartupScript")
                moduleState()->setStartupScriptURL(QUrl::fromUserInput(value.toString()));
            else if (key == "schedulerShutdownScript")
                moduleState()->setShutdownScriptURL(QUrl::fromUserInput(value.toString()));
        }
        else
            qCDebug(KSTARS_EKOS_SCHEDULER) << "Option" << key << "not found!";
    }

    // All Radio buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setChecked(value.toBool());
            settings[key] = value;
        }
    }

    // All QDateTime edits
    for (auto &oneWidget : findChildren<QDateTimeEdit*>())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setDateTime(QDateTime::fromString(value.toString(), Qt::ISODate));
            settings[key] = value;
        }
    }

    setErrorHandlingStrategy(static_cast<ErrorHandlingStrategy>(Options::errorHandlingStrategy()));

    m_GlobalSettings = m_Settings = settings;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Scheduler::syncSettings()
{
    QDoubleSpinBox *dsb = nullptr;
    QSpinBox *sb = nullptr;
    QCheckBox *cb = nullptr;
    QRadioButton *rb = nullptr;
    QComboBox *cbox = nullptr;
    QLineEdit *lineedit = nullptr;
    QDateTimeEdit *datetimeedit = nullptr;

    QString key;
    QVariant value;
    bool removeKey = false;

    if ( (dsb = qobject_cast<QDoubleSpinBox*>(sender())))
    {
        key = dsb->objectName();
        value = dsb->value();

    }
    else if ( (sb = qobject_cast<QSpinBox*>(sender())))
    {
        key = sb->objectName();
        value = sb->value();
    }
    else if ( (cb = qobject_cast<QCheckBox*>(sender())))
    {
        key = cb->objectName();
        value = cb->isChecked();
    }
    else if ( (rb = qobject_cast<QRadioButton*>(sender())))
    {
        key = rb->objectName();
        // N.B. We need to remove radio button false from local settings
        // since we need to only have the exclusive key present
        if (rb->isChecked() == false)
        {
            removeKey = true;
            value = false;
        }
        else
            value = true;
    }
    else if ( (cbox = qobject_cast<QComboBox*>(sender())))
    {
        key = cbox->objectName();
        value = cbox->currentText();
    }
    else if ( (lineedit = qobject_cast<QLineEdit*>(sender())))
    {
        key = lineedit->objectName();
        value = lineedit->text();
    }
    else if ( (datetimeedit = qobject_cast<QDateTimeEdit*>(sender())))
    {
        key = datetimeedit->objectName();
        value = datetimeedit->dateTime().toString(Qt::ISODate);
    }

    // Save immediately
    Options::self()->setProperty(key.toLatin1(), value);

    if (removeKey)
        m_Settings.remove(key);
    else
        m_Settings[key] = value;
    m_GlobalSettings[key] = value;

    m_DebounceTimer.start();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Scheduler::settleSettings()
{
    emit settingsUpdated(getAllSettings());
    Options::self()->save();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
QVariantMap Scheduler::getAllSettings() const
{
    QVariantMap settings;

    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->currentText());

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->value());

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->value());

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    // All Line Edits
    for (auto &oneWidget : findChildren<QLineEdit*>())
    {
        // Many other widget types (e.g. spinboxes) apparently have QLineEdit inside them so we want to skip those
        if (!oneWidget->objectName().startsWith("qt_"))
            settings.insert(oneWidget->objectName(), oneWidget->text());
    }

    // All Radio Buttons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    // All QDateTime
    for (auto &oneWidget : findChildren<QDateTimeEdit*>())
    {
        settings.insert(oneWidget->objectName(), oneWidget->dateTime().toString(Qt::ISODate));
    }

    return settings;
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
void Scheduler::setAllSettings(const QVariantMap &settings)
{
    // Disconnect settings that we don't end up calling syncSettings while
    // performing the changes.
    disconnectSettings();

    for (auto &name : settings.keys())
    {
        // Combo
        auto comboBox = findChild<QComboBox*>(name);
        if (comboBox)
        {
            syncControl(settings, name, comboBox);
            continue;
        }

        // Double spinbox
        auto doubleSpinBox = findChild<QDoubleSpinBox*>(name);
        if (doubleSpinBox)
        {
            syncControl(settings, name, doubleSpinBox);
            continue;
        }

        // spinbox
        auto spinBox = findChild<QSpinBox*>(name);
        if (spinBox)
        {
            syncControl(settings, name, spinBox);
            continue;
        }

        // checkbox
        auto checkbox = findChild<QCheckBox*>(name);
        if (checkbox)
        {
            syncControl(settings, name, checkbox);
            continue;
        }

        // Line Edits
        auto lineedit = findChild<QLineEdit*>(name);
        if (lineedit)
        {
            syncControl(settings, name, lineedit);

            if (name == "sequenceEdit")
                setSequence(lineedit->text());
            else if (name == "fitsEdit")
                processFITSSelection(QUrl::fromLocalFile(lineedit->text()));
            else if (name == "schedulerStartupScript")
                moduleState()->setStartupScriptURL(QUrl::fromUserInput(lineedit->text()));
            else if (name == "schedulerShutdownScript")
                moduleState()->setShutdownScriptURL(QUrl::fromUserInput(lineedit->text()));

            continue;
        }

        // Radio button
        auto radioButton = findChild<QRadioButton*>(name);
        if (radioButton)
        {
            syncControl(settings, name, radioButton);
            continue;
        }

        auto datetimeedit = findChild<QDateTimeEdit*>(name);
        if (datetimeedit)
        {
            syncControl(settings, name, datetimeedit);
            continue;
        }
    }

    m_Settings = settings;

    // Restablish connections
    connectSettings();
}

///////////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////////
bool Scheduler::syncControl(const QVariantMap &settings, const QString &key, QWidget * widget)
{
    QSpinBox *pSB = nullptr;
    QDoubleSpinBox *pDSB = nullptr;
    QCheckBox *pCB = nullptr;
    QComboBox *pComboBox = nullptr;
    QLineEdit *pLineEdit = nullptr;
    QRadioButton *pRadioButton = nullptr;
    QDateTimeEdit *pDateTimeEdit = nullptr;
    bool ok = true;

    if ((pSB = qobject_cast<QSpinBox *>(widget)))
    {
        const int value = settings[key].toInt(&ok);
        if (ok)
        {
            pSB->setValue(value);
            return true;
        }
    }
    else if ((pDSB = qobject_cast<QDoubleSpinBox *>(widget)))
    {
        const double value = settings[key].toDouble(&ok);
        if (ok)
        {
            pDSB->setValue(value);
            return true;
        }
    }
    else if ((pCB = qobject_cast<QCheckBox *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value != pCB->isChecked())
            pCB->setChecked(value);
        return true;
    }
    // ONLY FOR STRINGS, not INDEX
    else if ((pComboBox = qobject_cast<QComboBox *>(widget)))
    {
        const QString value = settings[key].toString();
        pComboBox->setCurrentText(value);
        return true;
    }
    else if ((pLineEdit = qobject_cast<QLineEdit *>(widget)))
    {
        const auto value = settings[key].toString();
        pLineEdit->setText(value);
        return true;
    }
    else if ((pRadioButton = qobject_cast<QRadioButton *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value)
            pRadioButton->setChecked(true);
        return true;
    }
    else if ((pDateTimeEdit = qobject_cast<QDateTimeEdit *>(widget)))
    {
        const auto value = QDateTime::fromString(settings[key].toString(), Qt::ISODate);
        pDateTimeEdit->setDateTime(value);
        return true;
    }

    return false;
}

void Scheduler::connectSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        connect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Scheduler::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        connect(oneWidget, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Ekos::Scheduler::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        connect(oneWidget, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::Scheduler::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        connect(oneWidget, &QCheckBox::toggled, this, &Ekos::Scheduler::syncSettings);

    // All Radio Butgtons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        connect(oneWidget, &QRadioButton::toggled, this, &Ekos::Scheduler::syncSettings);

    // All QLineEdits
    for (auto &oneWidget : findChildren<QLineEdit*>())
    {
        // Many other widget types (e.g. spinboxes) apparently have QLineEdit inside them so we want to skip those
        if (!oneWidget->objectName().startsWith("qt_"))
            connect(oneWidget, &QLineEdit::textChanged, this, &Ekos::Scheduler::syncSettings);
    }

    // All QDateTimeEdit
    for (auto &oneWidget : findChildren<QDateTimeEdit*>())
        connect(oneWidget, &QDateTimeEdit::dateTimeChanged, this, &Ekos::Scheduler::syncSettings);
}

void Scheduler::disconnectSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox*>())
        disconnect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Scheduler::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox*>())
        disconnect(oneWidget, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &Ekos::Scheduler::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox*>())
        disconnect(oneWidget, QOverload<int>::of(&QSpinBox::valueChanged), this, &Ekos::Scheduler::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox*>())
        disconnect(oneWidget, &QCheckBox::toggled, this, &Ekos::Scheduler::syncSettings);

    // All Radio Butgtons
    for (auto &oneWidget : findChildren<QRadioButton*>())
        disconnect(oneWidget, &QRadioButton::toggled, this, &Ekos::Scheduler::syncSettings);

    // All QLineEdits
    for (auto &oneWidget : findChildren<QLineEdit*>())
        disconnect(oneWidget, &QLineEdit::editingFinished, this, &Ekos::Scheduler::syncSettings);

    // All QDateTimeEdit
    for (auto &oneWidget : findChildren<QDateTimeEdit*>())
        disconnect(oneWidget, &QDateTimeEdit::editingFinished, this, &Ekos::Scheduler::syncSettings);
}

}
