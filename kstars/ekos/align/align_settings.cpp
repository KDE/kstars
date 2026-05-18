/*
    SPDX-FileCopyrightText: 2013-2021 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2018-2020 Robert Lancaster <rlancaste@gmail.com>
    SPDX-FileCopyrightText: 2019-2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "align.h"
#include "fov.h"
#include "mountmodel.h"
#include "opsalign.h"
#include "opsprograms.h"
#include "opsastrometry.h"
#include "opsastrometryindexfiles.h"
#include "Options.h"

#include "ekos/auxiliary/filtermanager.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/opticaltrainsettings.h"
#include "ekos/auxiliary/profilesettings.h"
#include "ekos/auxiliary/stellarsolverprofileeditor.h"
#include "ekos/manager.h"
#include "indi/indirotator.h"
#include "kspaths.h"

#include <ekos_align_debug.h>

#include <KConfigDialog>
#include <KPageWidgetItem>

namespace Ekos
{

void Align::slotMountModel()
{
    if (!m_MountModel)
    {
        m_MountModel = new MountModel(this);
        connect(m_MountModel, &Ekos::MountModel::newLog, this, &Ekos::Align::appendLogText, Qt::UniqueConnection);
        connect(m_MountModel, &Ekos::MountModel::aborted, this, [this]()
        {
            if (m_Mount && m_Mount->isSlewing())
                m_Mount->abort();
            abort();
        });
        connect(this, &Ekos::Align::newStatus, m_MountModel, &Ekos::MountModel::setAlignStatus, Qt::UniqueConnection);
    }

    m_MountModel->show();
    m_MountModel->raise();
    m_MountModel->activateWindow();
}

void Align::refreshAlignOptions()
{
    solverFOV->setImageDisplay(Options::astrometrySolverWCS());
    m_RemoteAlignTimer.setInterval(Options::astrometryTimeout() * 1000);
    if (m_Rotator)
        m_RotatorControlPanel->updateFlipPolicy(Options::astrometryFlipRotationAllowed());
}

void Align::setupOptions()
{
    KConfigDialog *dialog = new KConfigDialog(this, "alignsettings", Options::self());

#ifdef Q_OS_MACOS
    dialog->setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    opsAlign = new OpsAlign(this);
    connect(opsAlign, &OpsAlign::settingsUpdated, this, &Ekos::Align::refreshAlignOptions);
    KPageWidgetItem *page = dialog->addPage(opsAlign, i18n("StellarSolver Options"));
    page->setIcon(QIcon(":/icons/StellarSolverIcon.png"));

    opsPrograms = new OpsPrograms(this);
    page = dialog->addPage(opsPrograms, i18n("External & Online Programs"));
    page->setIcon(QIcon(":/icons/astrometry.svg"));

    opsAstrometry = new OpsAstrometry(this);
    page = dialog->addPage(opsAstrometry, i18n("Scale & Position"));
    page->setIcon(QIcon(":/icons/center_telescope_red.svg"));

    optionsProfileEditor = new StellarSolverProfileEditor(this, Ekos::AlignProfiles, dialog);
    page = dialog->addPage(optionsProfileEditor, i18n("Align Options Profiles Editor"));
    connect(optionsProfileEditor, &StellarSolverProfileEditor::optionsProfilesUpdated, this, [this]()
    {
        if(QFile(savedOptionsProfiles).exists())
            m_StellarSolverProfiles = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
        else
            m_StellarSolverProfiles = getDefaultAlignOptionsProfiles();
        opsAlign->reloadOptionsProfiles();
    });
    page->setIcon(QIcon::fromTheme("configure"));

    connect(opsAlign, &OpsAlign::needToLoadProfile, this, [this, dialog, page](const QString & profile)
    {
        optionsProfileEditor->loadProfile(profile);
        dialog->setCurrentPage(page);
    });

    opsAstrometryIndexFiles = new OpsAstrometryIndexFiles(this);
    connect(opsAstrometryIndexFiles, &OpsAstrometryIndexFiles::newDownloadProgress, this, &Align::newDownloadProgress);
    m_IndexFilesPage = dialog->addPage(opsAstrometryIndexFiles, i18n("Index Files"));
    m_IndexFilesPage->setIcon(QIcon::fromTheme("map-flat"));
}

void Align::setupSolutionTable()
{
    solutionTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    clearAllSolutionsB->setIcon(
        QIcon::fromTheme("application-exit"));
    clearAllSolutionsB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    removeSolutionB->setIcon(QIcon::fromTheme("list-remove"));
    removeSolutionB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    exportSolutionsCSV->setIcon(
        QIcon::fromTheme("document-save-as"));
    exportSolutionsCSV->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    autoScaleGraphB->setIcon(QIcon::fromTheme("zoom-fit-best"));
    autoScaleGraphB->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    connect(clearAllSolutionsB, &QPushButton::clicked, this, &Ekos::Align::slotClearAllSolutionPoints);
    connect(removeSolutionB, &QPushButton::clicked, this, &Ekos::Align::slotRemoveSolutionPoint);
    connect(exportSolutionsCSV, &QPushButton::clicked, this, &Ekos::Align::exportSolutionPoints);
    connect(autoScaleGraphB, &QPushButton::clicked, this, &Ekos::Align::slotAutoScaleGraph);
    connect(mountModelB, &QPushButton::clicked, this, &Ekos::Align::slotMountModel);
    connect(solutionTable, &QTableWidget::cellClicked, this, &Ekos::Align::selectSolutionTableRow);
}

void Align::setupPlot()
{
    double accuracyRadius = alignAccuracyThreshold->value();

    alignPlot->setBackground(QBrush(Qt::black));
    alignPlot->setSelectionTolerance(10);

    alignPlot->xAxis->setBasePen(QPen(Qt::white, 1));
    alignPlot->yAxis->setBasePen(QPen(Qt::white, 1));

    alignPlot->xAxis->setTickPen(QPen(Qt::white, 1));
    alignPlot->yAxis->setTickPen(QPen(Qt::white, 1));

    alignPlot->xAxis->setSubTickPen(QPen(Qt::white, 1));
    alignPlot->yAxis->setSubTickPen(QPen(Qt::white, 1));

    alignPlot->xAxis->setTickLabelColor(Qt::white);
    alignPlot->yAxis->setTickLabelColor(Qt::white);

    alignPlot->xAxis->setLabelColor(Qt::white);
    alignPlot->yAxis->setLabelColor(Qt::white);

    alignPlot->xAxis->setLabelFont(QFont(font().family(), 10));
    alignPlot->yAxis->setLabelFont(QFont(font().family(), 10));

    alignPlot->xAxis->setLabelPadding(2);
    alignPlot->yAxis->setLabelPadding(2);

    alignPlot->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    alignPlot->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    alignPlot->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    alignPlot->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    alignPlot->xAxis->grid()->setZeroLinePen(QPen(Qt::yellow));
    alignPlot->yAxis->grid()->setZeroLinePen(QPen(Qt::yellow));

    alignPlot->xAxis->setLabel(i18n("dRA (arcsec)"));
    alignPlot->yAxis->setLabel(i18n("dDE (arcsec)"));

    alignPlot->xAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);
    alignPlot->yAxis->setRange(-accuracyRadius * 3, accuracyRadius * 3);

    alignPlot->setInteractions(QCP::iRangeZoom);
    alignPlot->setInteraction(QCP::iRangeDrag, true);

    alignPlot->addGraph();
    alignPlot->graph(0)->setLineStyle(QCPGraph::lsNone);
    alignPlot->graph(0)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, Qt::white, 15));

    buildTarget();

    connect(alignPlot, &QCustomPlot::mouseMove, this, &Ekos::Align::handlePointTooltip);
    connect(rightLayout, &QSplitter::splitterMoved, this, &Ekos::Align::handleVerticalPlotSizeChange);
    connect(alignSplitter, &QSplitter::splitterMoved, this, &Ekos::Align::handleHorizontalPlotSizeChange);

    alignPlot->resize(190, 190);
    alignPlot->replot();
}

void Align::setupFilterManager()
{
    // Do we have an existing filter manager?
    if (m_FilterManager)
        m_FilterManager->disconnect(this);

    // Create new or refresh device
    Ekos::Manager::Instance()->createFilterManager(m_FilterWheel);

    // Return global filter manager for this filter wheel.
    Ekos::Manager::Instance()->getFilterManager(m_FilterWheel->getDeviceName(), m_FilterManager);

    connect(m_FilterManager.get(), &FilterManager::ready, this, [this]()
    {
        if (filterPositionPending)
        {
            m_FocusState = FOCUS_IDLE;
            filterPositionPending = false;
            captureAndSolve(false);
        }
    });

    connect(m_FilterManager.get(), &FilterManager::failed, this, [this]()
    {
        if (filterPositionPending)
        {
            appendLogText(i18n("Filter operation failed."));
            abort();
        }
    });

    connect(m_FilterManager.get(), &FilterManager::newStatus, this, [this](Ekos::FilterState filterState)
    {
        if (filterPositionPending)
        {
            switch (filterState)
            {
                case FILTER_OFFSET:
                    appendLogText(i18n("Changing focus offset by %1 steps...", m_FilterManager->getTargetFilterOffset()));
                    break;

                case FILTER_CHANGE:
                {
                    const int filterComboIndex = m_FilterManager->getTargetFilterPosition() - 1;
                    if (filterComboIndex >= 0 && filterComboIndex < alignFilter->count())
                        appendLogText(i18n("Changing filter to %1...", alignFilter->itemText(filterComboIndex)));
                }
                break;

                case FILTER_AUTOFOCUS:
                    appendLogText(i18n("Auto focus on filter change..."));
                    break;

                default:
                    break;
            }
        }
    });

    connect(m_FilterManager.get(), &FilterManager::labelsChanged, this, &Align::checkFilter);
    connect(m_FilterManager.get(), &FilterManager::positionChanged, this, &Align::checkFilter);
}

void Align::setupOpticalTrainManager()
{
    connect(OpticalTrainManager::Instance(), &OpticalTrainManager::updated, this, &Align::refreshOpticalTrain);
    connect(trainB, &QPushButton::clicked, this, [this]()
    {
        OpticalTrainManager::Instance()->openEditor(opticalTrainCombo->currentText());
    });
    connect(opticalTrainCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index)
    {
        ProfileSettings::Instance()->setOneSetting(ProfileSettings::AlignOpticalTrain,
                OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(index)));
        refreshOpticalTrain();
        Q_EMIT trainChanged();
    });
}

void Align::refreshOpticalTrain()
{
    opticalTrainCombo->blockSignals(true);
    opticalTrainCombo->clear();
    opticalTrainCombo->addItems(OpticalTrainManager::Instance()->getTrainNames());
    trainB->setEnabled(true);

    QVariant trainID = ProfileSettings::Instance()->getOneSetting(ProfileSettings::AlignOpticalTrain);

    if (trainID.isValid())
    {
        auto id = trainID.toUInt();

        // If train not found, select the first one available.
        if (OpticalTrainManager::Instance()->exists(id) == false)
        {
            qCWarning(KSTARS_EKOS_ALIGN) << "Optical train doesn't exist for id" << id;
            id = OpticalTrainManager::Instance()->id(opticalTrainCombo->itemText(0));
        }

        auto name = OpticalTrainManager::Instance()->name(id);

        opticalTrainCombo->setCurrentText(name);

        auto scope = OpticalTrainManager::Instance()->getScope(name);
        m_FocalLength = scope["focal_length"].toDouble(-1);
        m_Aperture = scope["aperture"].toDouble(-1);
        m_FocalRatio = scope["focal_ratio"].toDouble(-1);
        m_Reducer = OpticalTrainManager::Instance()->getReducer(name);

        // DSLR Lens Aperture
        if (m_Aperture < 0 && m_FocalRatio > 0)
            m_Aperture = m_FocalLength / m_FocalRatio;

        auto mount = OpticalTrainManager::Instance()->getMount(name);
        setMount(mount);

        auto camera = OpticalTrainManager::Instance()->getCamera(name);
        if (camera)
        {
            camera->setScopeInfo(m_FocalLength * m_Reducer, m_Aperture);
            opticalTrainCombo->setToolTip(QString("%1 @ %2").arg(camera->getDeviceName(), scope["name"].toString()));
        }
        setCamera(camera);

        syncTelescopeInfo();

        auto filterWheel = OpticalTrainManager::Instance()->getFilterWheel(name);
        setFilterWheel(filterWheel);

        auto rotator = OpticalTrainManager::Instance()->getRotator(name);
        setRotator(rotator);

        auto dustcap = OpticalTrainManager::Instance()->getDustCap(name);
        setDustCap(dustcap);

        // PAC is now set via Manager::syncGenericDevice() -> Align::setPAC()

        // Load train settings
        OpticalTrainSettings::Instance()->setOpticalTrainID(id);
        auto settings = OpticalTrainSettings::Instance()->getOneSetting(OpticalTrainSettings::Align);
        if (settings.isValid())
        {
            auto map = settings.toJsonObject().toVariantMap();
            if (map != m_Settings)
            {
                m_Settings.clear();
                setAllSettings(map);
            }
        }
        else
            m_Settings = m_GlobalSettings;

        // Need to save information used for Mosaic planner
        Options::setTelescopeFocalLength(m_FocalLength);
        Options::setCameraPixelWidth(m_CameraPixelWidth);
        Options::setCameraPixelHeight(m_CameraPixelHeight);
        Options::setCameraWidth(m_CameraWidth);
        Options::setCameraHeight(m_CameraHeight);
    }

    opticalTrainCombo->blockSignals(false);
}

void Align::syncSettings()
{
    QDoubleSpinBox *dsb = nullptr;
    QSpinBox *sb = nullptr;
    QCheckBox *cb = nullptr;
    QComboBox *cbox = nullptr;
    QRadioButton *cradio = nullptr;

    QString key;
    QVariant value;

    if ( (dsb = qobject_cast<QDoubleSpinBox * >(sender())))
    {
        key = dsb->objectName();
        value = dsb->value();

    }
    else if ( (sb = qobject_cast<QSpinBox * >(sender())))
    {
        key = sb->objectName();
        value = sb->value();
    }
    else if ( (cb = qobject_cast<QCheckBox * >(sender())))
    {
        key = cb->objectName();
        value = cb->isChecked();
    }
    else if ( (cbox = qobject_cast<QComboBox * >(sender())))
    {
        key = cbox->objectName();
        value = cbox->currentText();
    }
    else if ( (cradio = qobject_cast<QRadioButton * >(sender())))
    {
        key = cradio->objectName();
        // Discard false requests
        if (cradio->isChecked() == false)
        {
            m_Settings.remove(key);
            return;
        }
        value = true;
    }

    // Save immediately
    Options::self()->setProperty(key.toLatin1(), value);

    m_Settings[key] = value;
    m_GlobalSettings[key] = value;
    m_DebounceTimer.start();
}

void Align::settleSettings()
{
    Q_EMIT settingsUpdated(getAllSettings());
    // Save to optical train specific settings as well
    OpticalTrainSettings::Instance()->setOpticalTrainID(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()));
    OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::Align, m_Settings);
}

void Align::loadGlobalSettings()
{
    QString key;
    QVariant value;

    QVariantMap settings;
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox * >())
    {
        if (oneWidget->objectName() == "opticalTrainCombo")
            continue;

        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid() && oneWidget->count() > 0)
        {
            oneWidget->setCurrentText(value.toString());
            settings[key] = value;
        }
    }

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox * >())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setValue(value.toDouble());
            settings[key] = value;
        }
    }

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox * >())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setValue(value.toInt());
            settings[key] = value;
        }
    }

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox * >())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setChecked(value.toBool());
            settings[key] = value;
        }
    }

    // All Radio buttons
    for (auto &oneWidget : findChildren<QRadioButton * >())
    {
        key = oneWidget->objectName();
        value = Options::self()->property(key.toLatin1());
        if (value.isValid())
        {
            oneWidget->setChecked(value.toBool());
            settings[key] = value;
        }
    }

    m_GlobalSettings = m_Settings = settings;
}

void Align::connectSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox * >())
        connect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Align::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox * >())
        connect(oneWidget, &QDoubleSpinBox::editingFinished, this, &Ekos::Align::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox * >())
        connect(oneWidget, &QSpinBox::editingFinished, this, &Ekos::Align::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox * >())
        connect(oneWidget, &QCheckBox::toggled, this, &Ekos::Align::syncSettings);

    // All Radio buttons
    for (auto &oneWidget : findChildren<QRadioButton * >())
        connect(oneWidget, &QRadioButton::toggled, this, &Ekos::Align::syncSettings);

    // Train combo box should NOT be synced.
    disconnect(opticalTrainCombo, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Align::syncSettings);
}

void Align::disconnectSettings()
{
    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox * >())
        disconnect(oneWidget, QOverload<int>::of(&QComboBox::activated), this, &Ekos::Align::syncSettings);

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox * >())
        disconnect(oneWidget, &QDoubleSpinBox::editingFinished, this, &Ekos::Align::syncSettings);

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox * >())
        disconnect(oneWidget, &QSpinBox::editingFinished, this, &Ekos::Align::syncSettings);

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox * >())
        disconnect(oneWidget, &QCheckBox::toggled, this, &Ekos::Align::syncSettings);

    // All Radio buttons
    for (auto &oneWidget : findChildren<QRadioButton * >())
        disconnect(oneWidget, &QRadioButton::toggled, this, &Ekos::Align::syncSettings);
}

QVariantMap Align::getAllSettings() const
{
    QVariantMap settings;

    // All Combo Boxes
    for (auto &oneWidget : findChildren<QComboBox * >())
        settings.insert(oneWidget->objectName(), oneWidget->currentText());

    // All Double Spin Boxes
    for (auto &oneWidget : findChildren<QDoubleSpinBox * >())
        settings.insert(oneWidget->objectName(), oneWidget->value());

    // All Spin Boxes
    for (auto &oneWidget : findChildren<QSpinBox * >())
        settings.insert(oneWidget->objectName(), oneWidget->value());

    // All Checkboxes
    for (auto &oneWidget : findChildren<QCheckBox * >())
        settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    // All Radio Buttons
    for (auto &oneWidget : findChildren<QRadioButton * >())
        settings.insert(oneWidget->objectName(), oneWidget->isChecked());

    return settings;
}

void Align::setAllSettings(const QVariantMap &settings)
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

        // Radio button
        auto radioButton = findChild<QRadioButton*>(name);
        if (radioButton)
        {
            syncControl(settings, name, radioButton);
            continue;
        }
    }

    // Sync to options
    for (auto &key : settings.keys())
    {
        auto value = settings[key];
        // Save immediately
        Options::self()->setProperty(key.toLatin1(), value);
        Options::self()->save();

        m_Settings[key] = value;
        m_GlobalSettings[key] = value;
    }

    Q_EMIT settingsUpdated(getAllSettings());

    // Save to optical train specific settings as well
    OpticalTrainSettings::Instance()->setOpticalTrainID(OpticalTrainManager::Instance()->id(opticalTrainCombo->currentText()));
    OpticalTrainSettings::Instance()->setOneSetting(OpticalTrainSettings::Align, m_Settings);

    // Restablish connections
    connectSettings();
}

bool Align::syncControl(const QVariantMap &settings, const QString &key, QWidget * widget)
{
    QSpinBox *pSB = nullptr;
    QDoubleSpinBox *pDSB = nullptr;
    QCheckBox *pCB = nullptr;
    QComboBox *pComboBox = nullptr;
    QRadioButton *pRadioButton = nullptr;
    bool ok = false;

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
    else if ((pRadioButton = qobject_cast<QRadioButton *>(widget)))
    {
        const bool value = settings[key].toBool();
        if (value)
            pRadioButton->setChecked(true);
        return true;
    }

    return false;
}

}
