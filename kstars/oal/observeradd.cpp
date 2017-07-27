/***************************************************************************
                          observeradd.cpp  -  description

                             -------------------
    begin                : Sunday July 26, 2009
    copyright            : (C) 2009 by Prakash Mohan
    email                : prakash.mohan@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "observeradd.h"

#include "kstarsdata.h"

#include <QSqlTableModel>

ObserverAdd::ObserverAdd()
{
    // Setting up the widget from the .ui file and adding it to the QDialog
    QWidget *widget = new QWidget;

    ui.setupUi(widget);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(widget);
    setLayout(mainLayout);

    setWindowTitle(i18n("Manage Observers"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    ui.AddObserverB->setIcon(QIcon::fromTheme("list-add", QIcon(":/icons/breeze/default/list-add.svg")));
    ui.RemoveObserverB->setIcon(QIcon::fromTheme("list-remove", QIcon(":/icons/breeze/default/list-remove.svg")));

    // Load the observers list from the file
    loadObservers();
    QSqlDatabase db       = KStarsData::Instance()->userdb()->GetDatabase();
    QSqlTableModel *users = new QSqlTableModel(0, db);
    users->setTable("user");
    users->select();
    ui.tableView->setModel(users);
    ui.tableView->setColumnHidden(0, true);
    ui.tableView->horizontalHeader()->resizeContentsPrecision();
    ui.tableView->viewport()->update();
    ui.AddObserverB->setEnabled(false);
    ui.RemoveObserverB->setEnabled(false);
    ui.tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    // Make connections
    connect(ui.AddObserverB, SIGNAL(clicked()), this, SLOT(slotAddObserver()));
    connect(ui.RemoveObserverB, SIGNAL(clicked()), this, SLOT(slotRemoveObserver()));
    connect(ui.Name, SIGNAL(textChanged(QString)), this, SLOT(checkObserverInfo()));
    connect(ui.Surname, SIGNAL(textChanged(QString)), this, SLOT(checkObserverInfo()));
    //connect (ui.tableView->verticalHeader(),SIGNAL(sectionClicked(int)),this,SLOT(checkTableInfo()));
    connect(ui.tableView, SIGNAL(clicked(QModelIndex)), this, SLOT(auxSlot()));
}
void ObserverAdd::auxSlot()
{
    ui.RemoveObserverB->setEnabled(true);
}

void ObserverAdd::checkObserverInfo()
{
    if (ui.Name->text().isEmpty() || ui.Surname->text().isEmpty())
        ui.AddObserverB->setEnabled(false);
    else
        ui.AddObserverB->setEnabled(true);
}

void ObserverAdd::slotUpdateModel()
{
    QSqlDatabase db       = KStarsData::Instance()->userdb()->GetDatabase();
    QSqlTableModel *users = new QSqlTableModel(0, db);
    users->setTable("user");
    users->select();
    ui.tableView->setModel(users);
    ui.tableView->setColumnHidden(0, true);
    ui.tableView->horizontalHeader()->resizeContentsPrecision();
    ui.tableView->viewport()->update();
}

void ObserverAdd::slotRemoveObserver()
{
    QModelIndex index = ui.tableView->currentIndex();
    int nr            = index.row();
    QString s         = ui.tableView->model()->data(ui.tableView->model()->index(nr, 0)).toString();

    KStarsData::Instance()->userdb()->DeleteObserver(s);
    ui.RemoveObserverB->setEnabled(false);
    slotUpdateModel();
}

void ObserverAdd::slotAddObserver()
{
    if (KStarsData::Instance()->userdb()->FindObserver(ui.Name->text(), ui.Surname->text()))
    {
        if (OAL::warningOverwrite(
                i18n("Another Observer already exists with the given Name and Surname, Overwrite?")) == KMessageBox::No)
            return;
    }

    KStarsData::Instance()->userdb()->AddObserver(ui.Name->text(), ui.Surname->text(), ui.Contact->text());

    //Reload observers into OAL::m_observers
    loadObservers();
    // Reset the UI for a fresh addition
    ui.Name->clear();
    ui.Surname->clear();
    ui.Contact->clear();
    slotUpdateModel();
}

void ObserverAdd::loadObservers()
{
    KStarsData::Instance()->logObject()->readObservers();
}
