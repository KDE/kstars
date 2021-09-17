/*
    SPDX-FileCopyrightText: 2018 Valentin Boettcher <valentin@boettcher.cf>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "eclipsetool.h"
#include "ui_eclipsetool.h"
#include "ksplanetbase.h"
#include "kstarsdata.h"
#include "geolocation.h"
#include "dialogs/locationdialog.h"
#include "kstars.h"
#include "skymap.h"

#include <QtConcurrent>
#include <QFileDialog>
#include <QErrorMessage>
#include <QMenu>

EclipseTool::EclipseTool(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::EclipseTool)
{
    ui->setupUi(this);

    // set up the eclipse-partner selection
    ui->Obj1ComboBox->addItem(i18n("Earth Shadow"), KSPlanetBase::EARTH_SHADOW);
    ui->Obj2ComboBox->addItem(i18n("Moon"), KSPlanetBase::MOON);

    KStarsData *kd = KStarsData::Instance();
    KStarsDateTime dtStart(KStarsDateTime::currentDateTime());
    KStarsDateTime dtStop(dtStart.djd() + 365.24l); // one year

    m_geoLocation = kd->geo();
    ui->LocationButton->setText(m_geoLocation->fullName());

    ui->startDate->setDateTime(dtStart);
    ui->stopDate->setDateTime(dtStop);

    ui->ClearButton->setIcon(QIcon::fromTheme("edit-clear"));

    // set up slots
    connect(ui->LocationButton, &QPushButton::clicked, this, &EclipseTool::slotLocation);
    connect(ui->ComputeButton, &QPushButton::clicked, this, [&, this]() {
        // switch to progress bar
        ui->computeStack->setCurrentIndex(1);

        // reset progress
        ui->progressBar->setValue(0);

        QtConcurrent::run(this, &EclipseTool::slotCompute);
    });

    connect(ui->ClearButton, &QPushButton::clicked, &m_model, &EclipseModel::reset);
    connect(ui->ExportButton, &QPushButton::clicked, &m_model, &EclipseModel::exportAsCsv);

    // eclipse table
    ui->tableView->setModel(&m_model);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);

    // double click to view
    connect(ui->tableView, &QTableView::doubleClicked, this, [this](const QModelIndex &index){
        slotView(m_model.getEvent(index.row()));
    });

    connect(ui->tableView, &QTableView::customContextMenuRequested, this,  &EclipseTool::slotContextMenu);

    ui->progressBar->setMinimum(0);
    ui->progressBar->setMaximum(100);
}

EclipseTool::~EclipseTool()
{
    delete ui;
}

void EclipseTool::slotLocation()
{
    QPointer<LocationDialog> ld(new LocationDialog(this));
    if (ld->exec() == QDialog::Accepted && ld)
    {
        m_geoLocation = ld->selectedCity();
        ui->LocationButton->setText(m_geoLocation->fullName());
    }
    delete ld;
}

// NOTE: This will need redesign some day.
void EclipseTool::slotCompute()
{
    EclipseHandler * handler;
    KStarsDateTime dtStart(ui->startDate->dateTime()); // start date
    KStarsDateTime dtStop(ui->stopDate->dateTime());  // stop date
    long double startJD    = dtStart.djd();         // start julian day
    long double stopJD     = dtStop.djd();

    if(ui->Obj1ComboBox->currentData().toInt() == KSPlanetBase::EARTH_SHADOW && ui->Obj2ComboBox->currentData().toInt() == KSPlanetBase::MOON){
        handler = new LunarEclipseHandler();
    } else
        return;

    connect(handler, &EclipseHandler::signalEventFound, &m_model, &EclipseModel::slotAddEclipse);
    connect(handler, &EclipseHandler::signalProgress, ui->progressBar, &QProgressBar::setValue);
    connect(handler, &EclipseHandler::signalComputationFinished, this, &EclipseTool::slotFinished);

    handler->setGeoLocation(m_geoLocation);

    // let's go
    EclipseHandler::EclipseVector eclipses = handler->computeEclipses(startJD, stopJD);
    delete handler;
}

void EclipseTool::slotFinished()
{
    ui->computeStack->setCurrentIndex(0);
}

void EclipseTool::slotContextMenu(QPoint pos)
{
    int row = ui->tableView->indexAt(pos).row();
    EclipseEvent_s event = m_model.getEvent(row);

    QMenu * menu = new QMenu(this);
    QAction * view = new QAction(i18n("View in SkyMap"), menu);
    connect(view, &QAction::triggered, this, [=]{
        slotView(event);
    });
    menu->addAction(view);

    if(event->hasDetails()){
        QAction * details = new QAction(i18n("Show Details"), menu);
        connect(details, &QAction::triggered, this, [=] {
            event->slotShowDetails(); // call virtual
        });
        menu->addAction(details);
    }

    menu->popup(ui->tableView->viewport()->mapToGlobal(pos));
}

void EclipseTool::slotView(EclipseEvent_s event)
{
    SkyMap * map = KStars::Instance()->map();
    KStarsData * data = KStarsData::Instance();
    KStarsDateTime dt;

    dt.setDJD(event->getJD());
    data->changeDateTime(dt);
    data->setLocation(event->getGeolocation());

    map->setClickedObject(event->getEclipsingObjectFromSkyComposite());
    map->setClickedPoint(map->clickedObject());
    map->slotCenter();
}

EclipseModel::EclipseModel(QObject *parent) : QAbstractTableModel (parent)
{

}

int EclipseModel::rowCount(const QModelIndex &) const
{
    return m_eclipses.length();
}

QVariant EclipseModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    int col = index.column();

    if(role != Qt::DisplayRole)
        return QVariant();

    EclipseEvent * event = m_eclipses[row].get();

    switch(col) {
    case 0: /* Date */ {
        KStarsDateTime dt(event->getJD());
        return dt.toString(Qt::ISODate);
    }
    case 1: // Eclipsing Obj
        return event->getEclipsingObjectName();
    case 2: // Eclipsed Obj
        return event->getEclipsedObjectName();
    case 3: /* Eclipse Type */ {
        switch(event->getType()) {
        case EclipseEvent::FULL:
            return QString(i18n("Full"));
        case EclipseEvent::PARTIAL:
            return QString(i18n("Partial"));
        }
        break;
    }
    case 4: // Extra Info
        return event->getExtraInfo();
    }

    return QVariant();
}

void EclipseModel::slotAddEclipse(EclipseEvent_s eclipse)
{
    m_eclipses.append(eclipse);
    emit layoutChanged(); // there must be a better way
}

void EclipseModel::exportAsCsv()
{
    int i, j;
    QByteArray line;

    QFileDialog dialog;
    dialog.setNameFilter(i18n("CSV Files (*.csv)"));
    dialog.setDefaultSuffix("csv");
    dialog.setWindowTitle(i18nc("@title:window", "Export Eclipses"));

    QString fname;
    if(dialog.exec())
        fname = dialog.selectedFiles()[0];
    else {
        QErrorMessage msg;
        msg.showMessage(i18n("Could not export."));
        return;
    }

    QFile file(fname);

    file.open(QIODevice::WriteOnly | QIODevice::Text);

    for (i = 0; i < rowCount(); ++i)
    {
        for (j = 0; j < columnCount(); ++j)
        {
            line.append(data(index(i, j), Qt::DisplayRole).toByteArray());
            if (j < columnCount() - 1)
                line.append(";");
            else
                line.append("\n");
        }
        file.write(line);
        line.clear();
    }

    file.close();
}

QVariant EclipseModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Horizontal) {
            switch (section)
            {
            case 0:
                return QString("Date");
            case 1:
                return QString("Eclipsing Object");
            case 2:
                return QString("Eclipsed Object");
            case 3:
                return QString("Eclipse Type");
            case 4:
                return QString("Extra Information");
            }
        }
    }
    return QVariant();
}

void EclipseModel::reset()
{
    emit beginResetModel();
    emit endResetModel();
}
