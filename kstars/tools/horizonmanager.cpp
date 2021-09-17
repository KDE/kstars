/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "horizonmanager.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "linelist.h"
#include "ksnotification.h"
#include "Options.h"
#include "skymap.h"
#include "projections/projector.h"
#include "skycomponents/artificialhorizoncomponent.h"
#include "skycomponents/skymapcomposite.h"

#include <QStandardItemModel>

#include <kstars_debug.h>

#define MIN_NUMBER_POINTS 2

HorizonManagerUI::HorizonManagerUI(QWidget *p) : QFrame(p)
{
    setupUi(this);
}

HorizonManager::HorizonManager(QWidget *w) : QDialog(w)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    ui = new HorizonManagerUI(this);

    ui->setStyleSheet("QPushButton:checked { background-color: red; }");

    ui->addRegionB->setIcon(QIcon::fromTheme("list-add"));
    ui->addPointB->setIcon(QIcon::fromTheme("list-add"));
    ui->removeRegionB->setIcon(QIcon::fromTheme("list-remove"));
    ui->toggleCeilingB->setIcon(QIcon::fromTheme("window"));
    ui->removePointB->setIcon(QIcon::fromTheme("list-remove"));
    ui->clearPointsB->setIcon(QIcon::fromTheme("edit-clear"));
    ui->saveB->setIcon(QIcon::fromTheme("document-save"));
    ui->selectPointsB->setIcon(
        QIcon::fromTheme("snap-orthogonal"));

    ui->tipLabel->setPixmap(
        (QIcon::fromTheme("help-hint").pixmap(64, 64)));

    ui->regionValidation->setPixmap(
        QIcon::fromTheme("process-stop").pixmap(32, 32));
    ui->regionValidation->setToolTip(i18n("Region is invalid."));
    ui->regionValidation->hide();

    setWindowTitle(i18nc("@title:window", "Artificial Horizon Manager"));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(slotSaveChanges()));
    connect(buttonBox->button(QDialogButtonBox::Close), SIGNAL(clicked()), this, SLOT(slotClosed()));

    selectPoints = false;

    // Set up List view
    m_RegionsModel = new QStandardItemModel(0, 3, this);
    m_RegionsModel->setHorizontalHeaderLabels(QStringList()
            << i18n("Region") << i18nc("Azimuth", "Az") << i18nc("Altitude", "Alt"));

    ui->regionsList->setModel(m_RegionsModel);

    ui->pointsList->setModel(m_RegionsModel);
    ui->pointsList->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->pointsList->verticalHeader()->hide();
    ui->pointsList->setColumnHidden(0, true);

    horizonComponent = KStarsData::Instance()->skyComposite()->artificialHorizon();

    // Get the list
    const QList<ArtificialHorizonEntity *> *horizonList = horizonComponent->getHorizon().horizonList();

    for (auto &horizon : *horizonList)
    {
        QStandardItem *regionItem = new QStandardItem(horizon->region());
        regionItem->setCheckable(true);
        regionItem->setCheckState(horizon->enabled() ? Qt::Checked : Qt::Unchecked);

        if (horizon->ceiling())
            regionItem->setData(QIcon::fromTheme("window"), Qt::DecorationRole);
        else
            regionItem->setData(QIcon(), Qt::DecorationRole);
        regionItem->setData(horizon->ceiling(), Qt::UserRole);

        m_RegionsModel->appendRow(regionItem);

        SkyList *points = horizon->list()->points();

        for (auto &p : *points)
        {
            QList<QStandardItem *> pointsList;
            pointsList << new QStandardItem("") << new QStandardItem(p->az().toDMSString())
                       << new QStandardItem(p->alt().toDMSString());
            regionItem->appendRow(pointsList);
        }
    }

    ui->removeRegionB->setEnabled(true);
    ui->toggleCeilingB->setEnabled(true);

    connect(m_RegionsModel, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(verifyItemValue(QStandardItem*)));

    //Connect buttons
    connect(ui->addRegionB, SIGNAL(clicked()), this, SLOT(slotAddRegion()));
    connect(ui->removeRegionB, SIGNAL(clicked()), this, SLOT(slotRemoveRegion()));
    connect(ui->toggleCeilingB, SIGNAL(clicked()), this, SLOT(slotToggleCeiling()));

    connect(ui->regionsList, SIGNAL(clicked(QModelIndex)), this, SLOT(slotSetShownRegion(QModelIndex)));

    connect(ui->addPointB, SIGNAL(clicked()), this, SLOT(slotAddPoint()));
    connect(ui->removePointB, SIGNAL(clicked()), this, SLOT(slotRemovePoint()));
    connect(ui->clearPointsB, SIGNAL(clicked()), this, SLOT(clearPoints()));
    connect(ui->selectPointsB, SIGNAL(clicked(bool)), this, SLOT(setSelectPoints(bool)));

    connect(ui->pointsList->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)),
            this, SLOT(slotCurrentPointChanged(QModelIndex, QModelIndex)));

    connect(ui->saveB, SIGNAL(clicked()), this, SLOT(slotSaveChanges()));

    if (horizonList->count() > 0)
    {
        ui->regionsList->selectionModel()->setCurrentIndex(m_RegionsModel->index(0, 0),
                QItemSelectionModel::SelectCurrent);
        showRegion(0);
    }
}

// If the user hit's the 'X', still want to remove the live preview.
void HorizonManager::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event);
    slotClosed();
}

// This gets the live preview to be shown when the window is shown.
void HorizonManager::showEvent(QShowEvent *event)
{
    QWidget::showEvent( event );
    QStandardItem *regionItem = m_RegionsModel->item(ui->regionsList->currentIndex().row(), 0);
    if (regionItem)
    {
        setupLivePreview(regionItem);
        SkyMap::Instance()->forceUpdateNow();
    }
}

// Highlights the current point.
void HorizonManager::slotCurrentPointChanged(const QModelIndex &selected, const QModelIndex &deselected)
{
    Q_UNUSED(deselected);
    if (livePreview.get() != nullptr &&
            selected.row() >= 0 &&
            selected.row() < livePreview->points()->size())
        horizonComponent->setSelectedPreviewPoint(selected.row());
    else
        horizonComponent->setSelectedPreviewPoint(-1);
    SkyMap::Instance()->forceUpdateNow();
}

// Controls the UI validation check-mark, which indicates if the current
// region is valid or not.
void HorizonManager::setupValidation(int region)
{
    QStandardItem *regionItem = m_RegionsModel->item(region, 0);

    if (regionItem && regionItem->rowCount() >= MIN_NUMBER_POINTS)
    {
        if (validate(region))
        {
            ui->regionValidation->setPixmap(
                QIcon::fromTheme("dialog-ok").pixmap(32, 32));
            ui->regionValidation->setEnabled(true);
            ui->regionValidation->setToolTip(i18n("Region is valid"));
        }
        else
        {
            ui->regionValidation->setPixmap(
                QIcon::fromTheme("process-stop").pixmap(32, 32));
            ui->regionValidation->setEnabled(false);
            ui->regionValidation->setToolTip(i18n("Region is invalid."));
        }

        ui->regionValidation->show();
    }
    else
        ui->regionValidation->hide();
}

void HorizonManager::showRegion(int regionID)
{
    if (regionID < 0 || regionID >= m_RegionsModel->rowCount())
        return;
    else
    {
        ui->pointsList->setRootIndex(m_RegionsModel->index(regionID, 0));
        ui->pointsList->setColumnHidden(0, true);

        QStandardItem *regionItem = m_RegionsModel->item(regionID, 0);

        if (regionItem->rowCount() > 0)
            ui->pointsList->setCurrentIndex(regionItem->child(regionItem->rowCount() - 1, 0)->index());
        else
            // Invalid index.
            ui->pointsList->setCurrentIndex(QModelIndex());

        setupValidation(regionID);

        ui->addPointB->setEnabled(true);
        ui->removePointB->setEnabled(true);
        ui->selectPointsB->setEnabled(true);
        ui->clearPointsB->setEnabled(true);

        if (regionItem != nullptr)
        {
            setupLivePreview(regionItem);
            SkyMap::Instance()->forceUpdateNow();
        }
    }

    ui->saveB->setEnabled(true);
}

bool HorizonManager::validate(int regionID)
{
    QStandardItem *regionItem = m_RegionsModel->item(regionID, 0);

    if (regionItem == nullptr || regionItem->rowCount() < MIN_NUMBER_POINTS)
        return false;

    for (int i = 0; i < regionItem->rowCount(); i++)
    {
        dms az  = dms::fromString(regionItem->child(i, 1)->data(Qt::DisplayRole).toString(), true);
        dms alt = dms::fromString(regionItem->child(i, 2)->data(Qt::DisplayRole).toString(), true);

        if (std::isnan(az.Degrees()) || std::isnan(alt.Degrees()))
            return false;
    }

    return true;
}

void HorizonManager::removeEmptyRows(int regionID)
{
    QStandardItem *regionItem = m_RegionsModel->item(regionID, 0);

    if (regionItem == nullptr)
        return;

    QList<int> emptyRows;
    for (int i = 0; i < regionItem->rowCount(); i++)
    {
        dms az  = dms::fromString(regionItem->child(i, 1)->data(Qt::DisplayRole).toString(), true);
        dms alt = dms::fromString(regionItem->child(i, 2)->data(Qt::DisplayRole).toString(), true);

        if (std::isnan(az.Degrees()) || std::isnan(alt.Degrees()))
            emptyRows.append(i);
    }
    std::sort(emptyRows.begin(), emptyRows.end(), [](int a, int b) -> bool
    {
        return a > b;
    });
    for (int i = 0; i < emptyRows.size(); ++i)
        regionItem->removeRow(emptyRows[i]);
    return;
}

void HorizonManager::slotAddRegion()
{
    terminateLivePreview();

    setPointSelection(false);

    QStandardItem *regionItem = new QStandardItem(i18n("Region %1", m_RegionsModel->rowCount() + 1));
    regionItem->setCheckable(true);
    regionItem->setCheckState(Qt::Checked);
    m_RegionsModel->appendRow(regionItem);

    QModelIndex index = regionItem->index();
    ui->regionsList->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);

    showRegion(m_RegionsModel->rowCount() - 1);
}

void HorizonManager::slotToggleCeiling()
{
    int regionID = ui->regionsList->currentIndex().row();
    QStandardItem *regionItem = m_RegionsModel->item(regionID, 0);

    bool turnCeilingOn = !regionItem->data(Qt::UserRole).toBool();
    if (turnCeilingOn)
    {
        regionItem->setData(QIcon::fromTheme("window"), Qt::DecorationRole);
        regionItem->setData(true, Qt::UserRole);
    }
    else
    {
        regionItem->setData(QIcon(), Qt::DecorationRole);
        regionItem->setData(false, Qt::UserRole);
    }
}

void HorizonManager::slotRemoveRegion()
{
    terminateLivePreview();

    setPointSelection(false);

    int regionID = ui->regionsList->currentIndex().row();
    deleteRegion(regionID);

    if (regionID > 0)
        showRegion(regionID - 1);
    else if (m_RegionsModel->rowCount() == 0)
    {
        ui->regionValidation->hide();
        m_RegionsModel->clear();
    }
}

void HorizonManager::deleteRegion(int regionID)
{
    if (regionID == -1)
        return;

    if (regionID < m_RegionsModel->rowCount())
    {
        horizonComponent->removeRegion(m_RegionsModel->item(regionID, 0)->data(Qt::DisplayRole).toString());
        m_RegionsModel->removeRow(regionID);
        SkyMap::Instance()->forceUpdate();
    }
}

void HorizonManager::slotClosed()
{
    setSelectPoints(false);
    terminateLivePreview();
    SkyMap::Instance()->forceUpdate();
}

void HorizonManager::slotSaveChanges()
{
    terminateLivePreview();
    setPointSelection(false);

    for (int i = 0; i < m_RegionsModel->rowCount(); i++)
    {
        removeEmptyRows(i);
        if (validate(i) == false)
        {
            KSNotification::error(i18n("%1 region is invalid.",
                                       m_RegionsModel->item(i, 0)->data(Qt::DisplayRole).toString()));
            return;
        }
    }

    for (int i = 0; i < m_RegionsModel->rowCount(); i++)
    {
        QStandardItem *regionItem = m_RegionsModel->item(i, 0);
        QString regionName        = regionItem->data(Qt::DisplayRole).toString();

        horizonComponent->removeRegion(regionName);

        std::shared_ptr<LineList> list(new LineList());
        dms az, alt;
        std::shared_ptr<SkyPoint> p;

        for (int j = 0; j < regionItem->rowCount(); j++)
        {
            az  = dms::fromString(regionItem->child(j, 1)->data(Qt::DisplayRole).toString(), true);
            alt = dms::fromString(regionItem->child(j, 2)->data(Qt::DisplayRole).toString(), true);
            if (qIsNaN(az.Degrees()) || qIsNaN(alt.Degrees())) continue;

            p.reset(new SkyPoint());
            p->setAz(az);
            p->setAlt(alt);
            p->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

            list->append(p);
        }

        const bool ceiling = regionItem->data(Qt::UserRole).toBool();
        horizonComponent->addRegion(regionName, regionItem->checkState() == Qt::Checked ? true : false, list, ceiling);
    }

    horizonComponent->save();

    SkyMap::Instance()->forceUpdateNow();
}

void HorizonManager::slotSetShownRegion(QModelIndex idx)
{
    showRegion(idx.row());
}

// Copies values from the model to the livePreview, for the passed in region,
// and passes the livePreview to the horizonComponent, which renders the live preview.
void HorizonManager::setupLivePreview(QStandardItem * region)
{
    if (region == nullptr) return;
    livePreview.reset(new LineList());
    const int numPoints = region->rowCount();
    for (int i = 0; i < numPoints; i++)
    {
        QStandardItem *azItem  = region->child(i, 1);
        QStandardItem *altItem = region->child(i, 2);

        const dms az  = dms::fromString(azItem->data(Qt::DisplayRole).toString(), true);
        const dms alt = dms::fromString(altItem->data(Qt::DisplayRole).toString(), true);
        // Don't render points with bad values.
        if (qIsNaN(az.Degrees()) || qIsNaN(alt.Degrees()))
            continue;

        std::shared_ptr<SkyPoint> point(new SkyPoint());
        point->setAz(az);
        point->setAlt(alt);
        point->HorizontalToEquatorial(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

        livePreview->append(point);
    }

    horizonComponent->setLivePreview(livePreview);
}

void HorizonManager::addPoint(SkyPoint *skyPoint)
{
    QStandardItem *region = m_RegionsModel->item(ui->regionsList->currentIndex().row(), 0);
    if (region == nullptr)
        return;

    // Add the point after the current index in pointsList (row + 1).
    // If there is no current index, or if somehow (shouldn't happen)
    // the current index is larger than the list size, insert the point at the end
    int row = ui->pointsList->currentIndex().row();
    if ((row < 0) || (row >= region->rowCount()))
        row = region->rowCount();
    else row = row + 1;

    QList<QStandardItem *> pointsList;
    pointsList << new QStandardItem("") << new QStandardItem("") << new QStandardItem("");

    region->insertRow(row, pointsList);
    auto index = region->child(row, 0)->index();
    ui->pointsList->setCurrentIndex(index);

    m_RegionsModel->setHorizontalHeaderLabels(QStringList()
            << i18n("Region") << i18nc("Azimuth", "Az") << i18nc("Altitude", "Alt"));
    ui->pointsList->setColumnHidden(0, true);
    ui->pointsList->setRootIndex(region->index());

    // If a point was supplied (i.e. the user clicked on the SkyMap, as opposed to
    // just clicking the addPoint button), then set up its coordinates.
    if (skyPoint != nullptr)
    {
        QStandardItem *az  = region->child(row, 1);
        QStandardItem *alt = region->child(row, 2);

        az->setData(skyPoint->az().toDMSString(), Qt::DisplayRole);
        alt->setData(skyPoint->alt().toDMSString(), Qt::DisplayRole);

        setupLivePreview(region);
        slotCurrentPointChanged(ui->pointsList->currentIndex(), ui->pointsList->currentIndex());
    }
}

// Called when the user clicks on the SkyMap to add a new point.
void HorizonManager::addSkyPoint(SkyPoint * skypoint)
{
    if (selectPoints == false)
        return;
    // Make a copy.  This point wasn't staying stable in UI tests.
    SkyPoint pt = *skypoint;
    addPoint(&pt);
}

// Called when the user clicks on the addPoint button.
void HorizonManager::slotAddPoint()
{
    addPoint(nullptr);
}

void HorizonManager::slotRemovePoint()
{
    int regionID = ui->regionsList->currentIndex().row();
    QStandardItem *regionItem = m_RegionsModel->item(regionID, 0);
    if (regionItem == nullptr)
        return;

    int row = ui->pointsList->currentIndex().row();
    if (row == -1)
        row = regionItem->rowCount() - 1;
    regionItem->removeRow(row);

    setupValidation(regionID);

    if (livePreview.get() && row < livePreview->points()->count())
    {
        livePreview->points()->takeAt(row);

        if (livePreview->points()->isEmpty())
            terminateLivePreview();
        else
            SkyMap::Instance()->forceUpdateNow();
    }
}

void HorizonManager::clearPoints()
{
    QStandardItem *regionItem = m_RegionsModel->item(ui->regionsList->currentIndex().row(), 0);

    if (regionItem)
    {
        regionItem->removeRows(0, regionItem->rowCount());

        horizonComponent->removeRegion(regionItem->data(Qt::DisplayRole).toString(), true);

        ui->regionValidation->hide();
    }

    terminateLivePreview();
}

void HorizonManager::setSelectPoints(bool enable)
{
    selectPoints = enable;
    ui->selectPointsB->clearFocus();
}

void HorizonManager::verifyItemValue(QStandardItem * item)
{
    bool azOK = true, altOK = true;

    if (item->column() >= 1)
    {
        QStandardItem *parent = item->parent();

        dms azAngle  = dms::fromString(parent->child(item->row(), 1)->data(Qt::DisplayRole).toString(), true);
        dms altAngle = dms::fromString(parent->child(item->row(), 2)->data(Qt::DisplayRole).toString(), true);

        if (std::isnan(azAngle.Degrees()))
            azOK = false;
        if (std::isnan(altAngle.Degrees()))
            altOK = false;

        if ((item->column() == 1 && azOK == false) || (item->column() == 2 && altOK == false))

        {
            KSNotification::error(i18n("Invalid angle value: %1", item->data(Qt::DisplayRole).toString()));
            disconnect(m_RegionsModel, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(verifyItemValue(QStandardItem*)));
            item->setData(QVariant(qQNaN()), Qt::DisplayRole);
            connect(m_RegionsModel, SIGNAL(itemChanged(QStandardItem*)), this, SLOT(verifyItemValue(QStandardItem*)));
            return;
        }
        else if (azOK && altOK)
        {
            setupLivePreview(item->parent());
            setupValidation(ui->regionsList->currentIndex().row());
            SkyMap::Instance()->forceUpdateNow();
        }
    }
}

void HorizonManager::terminateLivePreview()
{
    if (!livePreview.get())
        return;

    livePreview.reset();
    horizonComponent->setLivePreview(livePreview);
}

void HorizonManager::setPointSelection(bool enable)
{
    selectPoints = enable;
    ui->selectPointsB->setChecked(enable);
}
