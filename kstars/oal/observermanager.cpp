#include "observermanager.h"
#include "QStandardItemModel"
#include "kstars/kstars.h"
#include "kstars/kstarsdata.h"
#include "oal.h"

ObserverManagerUi::ObserverManagerUi(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}


ObserverManager::ObserverManager(QWidget *parent) : KDialog(parent),
    mKstars(KStars::Instance()), mModel(0)
{
    mUi = new ObserverManagerUi(this);

    createModel();

    setMainWidget(mUi);
    setButtons(KDialog::Close);

    mUi->saveButton->setEnabled(false);
    mUi->deleteButton->setEnabled(false);

    connect(mUi->addButton, SIGNAL(clicked()), this, SLOT(addObserver()));
    connect(mUi->saveButton, SIGNAL(clicked()), this, SLOT(saveChanges()));
    connect(mUi->deleteButton, SIGNAL(clicked()), this, SLOT(deleteObserver()));
    connect(mUi->observerTableView, SIGNAL(clicked(QModelIndex)), this, SLOT(showObserver(QModelIndex)));
}

ObserverManager::~ObserverManager()
{}

void ObserverManager::addObserver()
{
    QList<QStandardItem*> row;
    QStandardItem *participatingBox = new QStandardItem;
    participatingBox->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    participatingBox->setCheckState(Qt::Unchecked);

    row << participatingBox << new QStandardItem(mUi->nameLineEdit->text()) << new QStandardItem(mUi->surnameLineEdit->text())
        << new QStandardItem(mUi->contactLineEdit->text());
    mModel->appendRow(row);

    saveChangesToObsList();
}

void ObserverManager::saveChanges()
{
    int rowNumber = mUi->observerTableView->selectionModel()->selectedRows().first().row();
    bool wasParticipating = mModel->data(mModel->index(rowNumber, 0), Qt::CheckStateRole).toBool();
    mModel->removeRow(rowNumber);

    QList<QStandardItem*> row;
    QStandardItem *participatingBox = new QStandardItem;
    participatingBox->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    participatingBox->setCheckState(wasParticipating ? Qt::Checked : Qt::Unchecked);

    row << participatingBox << new QStandardItem(mUi->nameLineEdit->text()) << new QStandardItem(mUi->surnameLineEdit->text())
        << new QStandardItem(mUi->contactLineEdit->text());
    mModel->insertRow(rowNumber, row);

    mUi->observerTableView->selectRow(rowNumber);

    saveChangesToObsList();
}

void ObserverManager::deleteObserver()
{
    if(mUi->observerTableView->selectionModel()->selectedRows().isEmpty())
        return;

    int rowNumber = mUi->observerTableView->selectionModel()->selectedRows().first().row();
    mModel->removeRow(rowNumber);

    mUi->nameLineEdit->clear();
    mUi->surnameLineEdit->clear();
    mUi->contactLineEdit->clear();

    mUi->saveButton->setEnabled(false);
    mUi->deleteButton->setEnabled(false);

    if(rowNumber) {
        mUi->observerTableView->selectRow(rowNumber);
        QModelIndex index = mModel->index(rowNumber, 0);
        showObserver(index);
    }
    saveChangesToObsList();
}

void ObserverManager::showObserver(QModelIndex idx)
{
    mUi->nameLineEdit->setText(mModel->data(mModel->index(idx.row(), 1)).toString());
    mUi->surnameLineEdit->setText(mModel->data(mModel->index(idx.row(), 2)).toString());
    mUi->contactLineEdit->setText(mModel->data(mModel->index(idx.row(), 3)).toString());

    mUi->saveButton->setEnabled(true);
    mUi->deleteButton->setEnabled(true);
}

void ObserverManager::createModel()
{
    if(mModel)
        delete mModel;

    mModel = new QStandardItemModel(0, 4, this);

    // set horizontal labels
    QStringList labels;
    labels << i18n("Participating") << i18n("Name") << i18n("Surname") << i18n("Contact");
    mModel->setHorizontalHeaderLabels(labels);

    int rowNumber = 0;
    foreach(OAL::Observer* observer, *mKstars->data()->logObject()->observerList()) {
        QList<QStandardItem*> row;
        QStandardItem *participatingBox = new QStandardItem;
        participatingBox->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        participatingBox->setCheckState(Qt::Unchecked);

        row << participatingBox << new QStandardItem(observer->name()) << new QStandardItem(observer->surname())
            << new QStandardItem(observer->contact());

        mModel->insertRow(rowNumber, row);
    }

    mUi->observerTableView->setModel(mModel);
    mUi->observerTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
}

void ObserverManager::saveChangesToObsList()
{
    for(int i = 0; i < mModel->rowCount(); i++) {
        QList<OAL::Observer*> *obsList = mKstars->data()->logObject()->observerList();
        qDeleteAll(*obsList);
        obsList->clear();

        OAL::Observer *observer = new OAL::Observer(i18n("observer_") + QString::number(i),
                                                    mModel->item(i, 1)->data().toString(),
                                                    mModel->item(i, 2)->data().toString(),
                                                    mModel->item(i, 3)->data().toString());
        obsList->append(observer);
    }
}
