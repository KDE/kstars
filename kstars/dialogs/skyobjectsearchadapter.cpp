/*
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skyobjectsearchadapter.h"
#include "finddialog.h"
#include "skyobjectlistmodel.h"
#include "catalogscomponent.h"
#include "kstarsdata.h"
#include "skyobjects/skyobject.h"
#include "skycomponents/skymapcomposite.h"

#include <QLineEdit>
#include <QTimer>
#include <QCompleter>
#include <QSortFilterProxyModel>
#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>

SkyObjectSearchAdapter::SkyObjectSearchAdapter(QLineEdit *lineEdit, QObject *parent)
    : QObject(parent)
    , m_lineEdit(lineEdit)
    , m_dbManager(CatalogsDB::dso_db_path())
{
    m_model = new SkyObjectListModel(this);

    m_sortModel = new QSortFilterProxyModel(this);
    m_sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_sortModel->setSourceModel(m_model);
    m_sortModel->setSortRole(Qt::DisplayRole);
    m_sortModel->setFilterRole(Qt::DisplayRole);
    m_sortModel->setDynamicSortFilter(true);
    m_sortModel->sort(0);

    m_completer = new QCompleter(m_sortModel, this);
    m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionRole(Qt::DisplayRole);
    m_completer->setMaxVisibleItems(10);
    m_completer->setWidget(m_lineEdit);

    connect(m_completer, QOverload<const QModelIndex &>::of(&QCompleter::activated),
            this, &SkyObjectSearchAdapter::activated);
    connect(m_lineEdit, &QLineEdit::textEdited, this, &SkyObjectSearchAdapter::enqueueSearch);

    // plain leading search icon
    m_lineEdit->addAction(QIcon::fromTheme("search"), QLineEdit::LeadingPosition);

    // small trailing chevron, opens the category filter menu. Hand-drawn
    // (rather than a themed standard icon) so it stays crisp this small.
    const int chevronIconSize = 12;
    QPixmap chevronPixmap(chevronIconSize, chevronIconSize);
    chevronPixmap.fill(Qt::transparent);
    {
        QPainter painter(&chevronPixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        QPen pen(m_lineEdit->palette().color(QPalette::WindowText));
        pen.setWidthF(1.4);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);

        const qreal w = chevronIconSize * 0.55;
        const qreal h = chevronIconSize * 0.3;
        const qreal x0 = (chevronIconSize - w) / 2.0;
        const qreal y0 = (chevronIconSize - h) / 2.0;
        QPainterPath chevron;
        chevron.moveTo(x0, y0);
        chevron.lineTo(x0 + w / 2.0, y0 + h);
        chevron.lineTo(x0 + w, y0);
        painter.strokePath(chevron, pen);
    }

    auto *filterAction = m_lineEdit->addAction(QIcon(chevronPixmap), QLineEdit::TrailingPosition);
    filterAction->setToolTip(i18n("Restrict search to an object category"));
    buildFilterMenu(filterAction);
}

void SkyObjectSearchAdapter::buildFilterMenu(QAction *filterAction)
{
    auto *menu = new QMenu(m_lineEdit);
    auto *group = new QActionGroup(menu);
    group->setExclusive(true);

    const QList<QPair<SearchCategory, QString>> categories =
    {
        { SearchCategory::Any, i18n("Any") },
        { SearchCategory::Stars, i18n("Stars") },
        { SearchCategory::SolarSystem, i18n("Solar System") },
        { SearchCategory::OpenClusters, i18n("Open Clusters") },
        { SearchCategory::GlobularClusters, i18n("Globular Clusters") },
        { SearchCategory::GaseousNebulae, i18n("Gaseous Nebulae") },
        { SearchCategory::PlanetaryNebulae, i18n("Planetary Nebulae") },
        { SearchCategory::Galaxies, i18n("Galaxies") },
        { SearchCategory::Comets, i18n("Comets") },
        { SearchCategory::Asteroids, i18n("Asteroids") },
        { SearchCategory::Constellations, i18n("Constellations") },
        { SearchCategory::Supernovae, i18n("Supernovae") },
        { SearchCategory::Satellites, i18n("Satellites") },
    };

    for (const auto &entry : categories)
    {
        SearchCategory category = entry.first;
        auto *action = menu->addAction(entry.second);
        action->setCheckable(true);
        action->setChecked(category == m_category);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, category]()
        {
            m_category = category;
            if (FindDialog::processSearchText(m_lineEdit->text()).length() >= 2)
                filterList();
        });
    }

    connect(filterAction, &QAction::triggered, this, [this, menu]()
    {
        const int x = m_lineEdit->width() - menu->sizeHint().width();
        menu->popup(m_lineEdit->mapToGlobal(QPoint(x, m_lineEdit->height())));
    });
}

void SkyObjectSearchAdapter::enqueueSearch()
{
    if (m_timer == nullptr)
    {
        m_timer = new QTimer(this);
        m_timer->setSingleShot(true);
        connect(m_timer, &QTimer::timeout, this, &SkyObjectSearchAdapter::filterList);
    }
    m_timer->start(500);
}

void SkyObjectSearchAdapter::filterList()
{
    const QString searchText = FindDialog::processSearchText(m_lineEdit->text());

    if (searchText.length() < 2)
    {
        m_completer->popup()->hide();
        return;
    }

    // pull in DSOs not yet loaded into memory (e.g. below the current magnitude limit)
    auto objs = m_dbManager.find_objects_by_name(searchText, 10);
    for (const auto &obj : objs)
        KStarsData::Instance()->skyComposite()->catalogsComponent()->insertStaticObject(obj);

    m_model->setSkyObjectsList(objectsForCategory());

    m_sortModel->setFilterFixedString(searchText);
    m_completer->complete();
}

QVector<QPair<QString, const SkyObject *>> SkyObjectSearchAdapter::objectsForCategory() const
{
    auto *data = KStarsData::Instance();
    QVector<QPair<QString, const SkyObject *>> objects;

    switch (m_category)
    {
        case SearchCategory::Any:
            for (auto type : data->skyComposite()->objectLists().keys())
                objects.append(data->skyComposite()->objectLists(SkyObject::TYPE(type)));
            break;
        case SearchCategory::Stars:
            objects.append(data->skyComposite()->objectLists(SkyObject::STAR));
            objects.append(data->skyComposite()->objectLists(SkyObject::CATALOG_STAR));
            break;
        case SearchCategory::SolarSystem:
            objects.append(data->skyComposite()->objectLists(SkyObject::PLANET));
            objects.append(data->skyComposite()->objectLists(SkyObject::COMET));
            objects.append(data->skyComposite()->objectLists(SkyObject::ASTEROID));
            objects.append(data->skyComposite()->objectLists(SkyObject::MOON));
            break;
        case SearchCategory::OpenClusters:
            objects = data->skyComposite()->objectLists(SkyObject::OPEN_CLUSTER);
            break;
        case SearchCategory::GlobularClusters:
            objects = data->skyComposite()->objectLists(SkyObject::GLOBULAR_CLUSTER);
            break;
        case SearchCategory::GaseousNebulae:
            objects = data->skyComposite()->objectLists(SkyObject::GASEOUS_NEBULA);
            break;
        case SearchCategory::PlanetaryNebulae:
            objects = data->skyComposite()->objectLists(SkyObject::PLANETARY_NEBULA);
            break;
        case SearchCategory::Galaxies:
            objects = data->skyComposite()->objectLists(SkyObject::GALAXY);
            break;
        case SearchCategory::Comets:
            objects = data->skyComposite()->objectLists(SkyObject::COMET);
            break;
        case SearchCategory::Asteroids:
            objects = data->skyComposite()->objectLists(SkyObject::ASTEROID);
            break;
        case SearchCategory::Constellations:
            objects = data->skyComposite()->objectLists(SkyObject::CONSTELLATION);
            break;
        case SearchCategory::Supernovae:
            objects = data->skyComposite()->objectLists(SkyObject::SUPERNOVA);
            break;
        case SearchCategory::Satellites:
            objects = data->skyComposite()->objectLists(SkyObject::SATELLITE);
            break;
    }

    return objects;
}

void SkyObjectSearchAdapter::activated(const QModelIndex &index)
{
    QVariant sObj = index.data(SkyObjectListModel::SkyObjectRole);
    auto *object = reinterpret_cast<SkyObject *>(sObj.value<void *>());
    if (object == nullptr)
        return;

    m_lineEdit->setText(object->name());
    Q_EMIT objectSelected(object);
}
