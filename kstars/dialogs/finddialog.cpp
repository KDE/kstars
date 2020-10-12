/***************************************************************************
                          finddialog.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Jul 4 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "finddialog.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "ksnotification.h"
#include "Options.h"
#include "detaildialog.h"
#include "skymap.h"
#include "skyobjects/skyobject.h"
#include "skycomponents/starcomponent.h"
#include "skycomponents/skymapcomposite.h"
#include "tools/nameresolver.h"
#include "skyobjectlistmodel.h"
#include "catalogscomponent.h"
#include <KMessageBox>

#include <QSortFilterProxyModel>
#include <QStringListModel>
#include <QTimer>
#include <QComboBox>
#include <QLineEdit>

FindDialog *FindDialog::m_Instance = nullptr;

FindDialogUI::FindDialogUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);

    FilterType->addItem(i18n("Any"));
    FilterType->addItem(i18n("Stars"));
    FilterType->addItem(i18n("Solar System"));
    FilterType->addItem(i18n("Open Clusters"));
    FilterType->addItem(i18n("Globular Clusters"));
    FilterType->addItem(i18n("Gaseous Nebulae"));
    FilterType->addItem(i18n("Planetary Nebulae"));
    FilterType->addItem(i18n("Galaxies"));
    FilterType->addItem(i18n("Comets"));
    FilterType->addItem(i18n("Asteroids"));
    FilterType->addItem(i18n("Constellations"));
    FilterType->addItem(i18n("Supernovae"));
    FilterType->addItem(i18n("Satellites"));

    SearchList->setMinimumWidth(256);
    SearchList->setMinimumHeight(320);
}

FindDialog *FindDialog::Instance()
{
    if (m_Instance == nullptr)
        m_Instance = new FindDialog(KStars::Instance());

    return m_Instance;
}

FindDialog::FindDialog(QWidget *parent)
    : QDialog(parent), timer(nullptr),
      m_targetObject(nullptr), m_manager{ CatalogsDB::dso_db_path() }

{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    ui = new FindDialogUI(this);

    setWindowTitle(i18nc("@title:window", "Find Object"));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(slotOk()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    okB = buttonBox->button(QDialogButtonBox::Ok);

    QPushButton *detailB = new QPushButton(i18n("Details..."));
    buttonBox->addButton(detailB, QDialogButtonBox::ActionRole);
    connect(detailB, SIGNAL(clicked()), this, SLOT(slotDetails()));

    ui->InternetSearchButton->setVisible(Options::resolveNamesOnline());
    ui->InternetSearchButton->setEnabled(false);
    connect(ui->InternetSearchButton, SIGNAL(clicked()), this, SLOT(slotResolve()));

    ui->FilterType->setCurrentIndex(0); // show all types of objects

    fModel = new SkyObjectListModel(this);
    connect(KStars::Instance()->map(), &SkyMap::removeSkyObject, fModel, &SkyObjectListModel::removeSkyObject);
    sortModel = new QSortFilterProxyModel(ui->SearchList);
    sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    sortModel->setSourceModel(fModel);
    sortModel->setSortRole(Qt::DisplayRole);
    sortModel->setFilterRole(Qt::DisplayRole);
    sortModel->setDynamicSortFilter(true);
    sortModel->sort(0);

    ui->SearchList->setModel(sortModel);

    // Connect signals to slots
    connect(ui->clearHistoryB, &QPushButton::clicked, [&]()
    {
        ui->clearHistoryB->setEnabled(false);
        m_HistoryCombo->clear();
        m_HistoryList.clear();
    });

    m_HistoryCombo = new QComboBox(ui->showHistoryB);
    m_HistoryCombo->move(0, ui->showHistoryB->height());
    connect(ui->showHistoryB, &QPushButton::clicked, [&]()
    {
        if (m_HistoryList.empty() == false)
        {
            m_HistoryCombo->showPopup();
        }
    });

    connect(m_HistoryCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
            [&](int index)
    {
        m_targetObject = m_HistoryList[index];
        m_targetObject->updateCoordsNow(KStarsData::Instance()->updateNum());
        m_HistoryCombo->setCurrentIndex(-1);
        m_HistoryCombo->hidePopup();
        accept();
    });
    connect(ui->SearchBox, &QLineEdit::textChanged, this, &FindDialog::enqueueSearch);
    connect(ui->SearchBox, &QLineEdit::returnPressed, this, &FindDialog::slotOk);
    connect(ui->FilterType, &QComboBox::currentTextChanged, this, &FindDialog::enqueueSearch);
    connect(ui->SearchList, SIGNAL(doubleClicked(QModelIndex)), SLOT(slotOk()));

    // Set focus to object name edit
    ui->SearchBox->setFocus();

    // First create and paint dialog and then load list
    QTimer::singleShot(0, this, SLOT(init()));

    listFiltered = false;
}

void FindDialog::init()
{
    const auto &objs = m_manager.get_objects(Options::magLimitDrawDeepSky(), 100);
    for (const auto &obj : objs)
    {
        KStarsData::Instance()->skyComposite()->catalogsComponent()->insertStaticObject(
            obj);
    }
    ui->SearchBox->clear();
    filterByType();
    sortModel->sort(0);
    initSelection();
    m_targetObject = nullptr;
}

void FindDialog::showEvent(QShowEvent *e)
{
    ui->SearchBox->setFocus();
    e->accept();
}

void FindDialog::initSelection()
{
    if (sortModel->rowCount() <= 0)
    {
        okB->setEnabled(false);
        return;
    }

    //    ui->SearchBox->setModel(sortModel);
    //    ui->SearchBox->setModelColumn(0);

    if (ui->SearchBox->text().isEmpty())
    {
        //Pre-select the first item
        QModelIndex selectItem = sortModel->index(0, sortModel->filterKeyColumn(), QModelIndex());
        switch (ui->FilterType->currentIndex())
        {
            case 0: //All objects, choose Andromeda galaxy
            {
                QModelIndex qmi = fModel->index(fModel->indexOf(i18n("Andromeda Galaxy")));
                selectItem      = sortModel->mapFromSource(qmi);
                break;
            }
            case 1: //Stars, choose Aldebaran
            {
                QModelIndex qmi = fModel->index(fModel->indexOf(i18n("Aldebaran")));
                selectItem      = sortModel->mapFromSource(qmi);
                break;
            }
            case 2: //Solar system or Asteroids, choose Aaltje
            case 9:
            {
                QModelIndex qmi = fModel->index(fModel->indexOf(i18n("Aaltje")));
                selectItem      = sortModel->mapFromSource(qmi);
                break;
            }
            case 8: //Comets, choose 'Aarseth-Brewington (1989 W1)'
            {
                QModelIndex qmi = fModel->index(fModel->indexOf(i18n("Aarseth-Brewington (1989 W1)")));
                selectItem      = sortModel->mapFromSource(qmi);
                break;
            }
        }

        if (selectItem.isValid())
        {
            ui->SearchList->selectionModel()->select(selectItem, QItemSelectionModel::ClearAndSelect);
            ui->SearchList->scrollTo(selectItem);
            ui->SearchList->setCurrentIndex(selectItem);

            okB->setEnabled(true);
        }
    }

    listFiltered = true;
}

void FindDialog::filterByType()
{
    KStarsData *data = KStarsData::Instance();

    switch (ui->FilterType->currentIndex())
    {
        case 0: // All object types
        {
            QVector<QPair<QString, const SkyObject *>> allObjects;
            foreach (int type, data->skyComposite()->objectLists().keys())
            {
                allObjects.append(data->skyComposite()->objectLists(SkyObject::TYPE(type)));
            }
            fModel->setSkyObjectsList(allObjects);
            break;
        }
        case 1: //Stars
        {
            QVector<QPair<QString, const SkyObject *>> starObjects;
            starObjects.append(data->skyComposite()->objectLists(SkyObject::STAR));
            starObjects.append(data->skyComposite()->objectLists(SkyObject::CATALOG_STAR));
            fModel->setSkyObjectsList(starObjects);
            break;
        }
        case 2: //Solar system
        {
            QVector<QPair<QString, const SkyObject *>> ssObjects;
            ssObjects.append(data->skyComposite()->objectLists(SkyObject::PLANET));
            ssObjects.append(data->skyComposite()->objectLists(SkyObject::COMET));
            ssObjects.append(data->skyComposite()->objectLists(SkyObject::ASTEROID));
            ssObjects.append(data->skyComposite()->objectLists(SkyObject::MOON));

            fModel->setSkyObjectsList(ssObjects);
            break;
        }
        case 3: //Open Clusters
            fModel->setSkyObjectsList(data->skyComposite()->objectLists(SkyObject::OPEN_CLUSTER));
            break;
        case 4: //Globular Clusters
            fModel->setSkyObjectsList(data->skyComposite()->objectLists(SkyObject::GLOBULAR_CLUSTER));
            break;
        case 5: //Gaseous nebulae
            fModel->setSkyObjectsList(data->skyComposite()->objectLists(SkyObject::GASEOUS_NEBULA));
            break;
        case 6: //Planetary nebula
            fModel->setSkyObjectsList(data->skyComposite()->objectLists(SkyObject::PLANETARY_NEBULA));
            break;
        case 7: //Galaxies
            fModel->setSkyObjectsList(data->skyComposite()->objectLists(SkyObject::GALAXY));
            break;
        case 8: //Comets
            fModel->setSkyObjectsList(data->skyComposite()->objectLists(SkyObject::COMET));
            break;
        case 9: //Asteroids
            fModel->setSkyObjectsList(data->skyComposite()->objectLists(SkyObject::ASTEROID));
            break;
        case 10: //Constellations
            fModel->setSkyObjectsList(data->skyComposite()->objectLists(SkyObject::CONSTELLATION));
            break;
        case 11: //Supernovae
            fModel->setSkyObjectsList(data->skyComposite()->objectLists(SkyObject::SUPERNOVA));
            break;
        case 12: //Satellites
            fModel->setSkyObjectsList(data->skyComposite()->objectLists(SkyObject::SATELLITE));
            break;
    }
}

void FindDialog::filterList()
{
    QString SearchText = processSearchText();
    const auto &objs   = m_manager.find_objects_by_name(SearchText, 10);
    for (const auto &obj : objs)
    {
        KStarsData::Instance()->skyComposite()->catalogsComponent()->insertStaticObject(
            obj);
    }

    sortModel->setFilterFixedString(SearchText);
    ui->InternetSearchButton->setText(i18n("or search the Internet for %1", SearchText));
    filterByType();
    initSelection();

    //Select the first item in the list that begins with the filter string
    if (!SearchText.isEmpty())
    {
        QStringList mItems =
            fModel->filter(QRegExp('^' + SearchText, Qt::CaseInsensitive));
        mItems.sort();

        if (mItems.size())
        {
            QModelIndex qmi        = fModel->index(fModel->indexOf(mItems[0]));
            QModelIndex selectItem = sortModel->mapFromSource(qmi);

            if (selectItem.isValid())
            {
                ui->SearchList->selectionModel()->select(
                    selectItem, QItemSelectionModel::ClearAndSelect);
                ui->SearchList->scrollTo(selectItem);
                ui->SearchList->setCurrentIndex(selectItem);

                okB->setEnabled(true);
            }
        }
        ui->InternetSearchButton->setEnabled(!mItems.contains(
            SearchText)); // Disable searching the internet when an exact match for SearchText exists in KStars
    }
    else
        ui->InternetSearchButton->setEnabled(false);

    listFiltered = true;
}

SkyObject *FindDialog::selectedObject() const
{
    QModelIndex i = ui->SearchList->currentIndex();
    QVariant sObj = sortModel->data(sortModel->index(i.row(), 0), SkyObjectListModel::SkyObjectRole);

    return reinterpret_cast<SkyObject*>(sObj.value<void *>());
}

void FindDialog::enqueueSearch()
{
    listFiltered = false;
    if (timer)
    {
        timer->stop();
    }
    else
    {
        timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, SIGNAL(timeout()), this, SLOT(filterList()));
    }
    timer->start(500);
}

// Process the search box text to replace equivalent names like "m93" with "m 93"
QString FindDialog::processSearchText(QString searchText)
{
    QRegExp re;
    re.setCaseSensitivity(Qt::CaseInsensitive);

    // Remove multiple spaces and replace them by a single space
    re.setPattern("  +");
    searchText.replace(re, " ");

    // If it is an NGC/IC/M catalog number, as in "M 76" or "NGC 5139", check for absence of the space
    re.setPattern("^(m|ngc|ic)\\s*\\d*$");
    if (searchText.contains(re))
    {
        re.setPattern("\\s*(\\d+)");
        searchText.replace(re, " \\1");
        re.setPattern("\\s*$");
        searchText.remove(re);
        re.setPattern("^\\s*");
        searchText.remove(re);
    }

    // TODO after KDE 4.1 release:
    // If it is a IAU standard three letter abbreviation for a constellation, then go to that constellation
    // Check for genetive names of stars. Example: alp CMa must go to alpha Canis Majoris

    return searchText;
}

void FindDialog::slotOk()
{
    //If no valid object selected, show a sorry-box.  Otherwise, emit accept()
    SkyObject *selObj;
    if (!listFiltered)
    {
        filterList();
    }
    selObj = selectedObject();
    finishProcessing(selObj, Options::resolveNamesOnline());
}

void FindDialog::slotResolve()
{
    finishProcessing(nullptr, true);
}

CatalogObject *FindDialog::resolveAndAdd(CatalogsDB::DBManager &db_manager, const QString &query)
{
    CatalogObject *dso = nullptr;
    const auto &cedata = NameResolver::resolveName(query);

    if (cedata.first)
    {
        db_manager.add_object(CatalogsDB::user_catalog_id, cedata.second);
        const auto &added_object =
            db_manager.get_object(cedata.second.getId(), CatalogsDB::user_catalog_id);

        if (added_object.first)
        {
            dso = &KStarsData::Instance()
                  ->skyComposite()
                  ->catalogsComponent()
                  ->insertStaticObject(added_object.second);
        }
    }
    return dso;
}

void FindDialog::finishProcessing(SkyObject *selObj, bool resolve)
{
    if (!selObj && resolve)
    {
        selObj = resolveAndAdd(m_manager, processSearchText());
    }
    m_targetObject = selObj;
    if (selObj == nullptr)
    {
        QString message = i18n("No object named %1 found.", ui->SearchBox->text());
        KSNotification::sorry(message, i18n("Bad object name"));
    }
    else
    {
        selObj->updateCoordsNow(KStarsData::Instance()->updateNum());
        if (m_HistoryList.contains(selObj) == false)
        {
            switch (selObj->type())
            {
                case SkyObject::OPEN_CLUSTER:
                case SkyObject::GLOBULAR_CLUSTER:
                case SkyObject::GASEOUS_NEBULA:
                case SkyObject::PLANETARY_NEBULA:
                case SkyObject::SUPERNOVA_REMNANT:
                case SkyObject::GALAXY:
                    if (selObj->name() != selObj->longname())
                        m_HistoryCombo->addItem(QString("%1 (%2)")
                                                    .arg(selObj->name())
                                                    .arg(selObj->longname()));
                    else
                        m_HistoryCombo->addItem(QString("%1").arg(selObj->longname()));
                    break;

                case SkyObject::STAR:
                case SkyObject::CATALOG_STAR:
                case SkyObject::PLANET:
                case SkyObject::COMET:
                case SkyObject::ASTEROID:
                case SkyObject::CONSTELLATION:
                case SkyObject::MOON:
                case SkyObject::ASTERISM:
                case SkyObject::GALAXY_CLUSTER:
                case SkyObject::DARK_NEBULA:
                case SkyObject::QUASAR:
                case SkyObject::MULT_STAR:
                case SkyObject::RADIO_SOURCE:
                case SkyObject::SATELLITE:
                case SkyObject::SUPERNOVA:
                default:
                    m_HistoryCombo->addItem(QString("%1").arg(selObj->longname()));
                    break;
            }

            m_HistoryList.append(selObj);
        }
        ui->clearHistoryB->setEnabled(true);
        accept();
    }
}
void FindDialog::keyPressEvent(QKeyEvent *e)
{
    switch (e->key())
    {
        case Qt::Key_Escape:
            reject();
            break;
        case Qt::Key_Up:
        {
            int currentRow = ui->SearchList->currentIndex().row();
            if (currentRow > 0)
            {
                QModelIndex selectItem = sortModel->index(currentRow - 1, sortModel->filterKeyColumn(), QModelIndex());
                ui->SearchList->selectionModel()->setCurrentIndex(selectItem, QItemSelectionModel::SelectCurrent);
            }
            break;
        }
        case Qt::Key_Down:
        {
            int currentRow = ui->SearchList->currentIndex().row();
            if (currentRow < sortModel->rowCount() - 1)
            {
                QModelIndex selectItem = sortModel->index(currentRow + 1, sortModel->filterKeyColumn(), QModelIndex());
                ui->SearchList->selectionModel()->setCurrentIndex(selectItem, QItemSelectionModel::SelectCurrent);
            }
            break;
        }
    }
}

void FindDialog::slotDetails()
{
    if (selectedObject())
    {
        QPointer<DetailDialog> dd = new DetailDialog(selectedObject(), KStarsData::Instance()->ut(),
                KStarsData::Instance()->geo(), KStars::Instance());
        dd->exec();
        delete dd;
    }
}

int FindDialog::execWithParent(QWidget* parent)
{
    QWidget * const oldParent = parentWidget();

    if (nullptr != parent)
    {
        setParent(parent);
        setWindowFlag(Qt::Dialog, true);
    }

    int const result = QDialog::exec();

    if (nullptr != parent)
    {
        setParent(oldParent);
        setWindowFlag(Qt::Dialog, true);
    }

    return result;
}
