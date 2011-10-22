/***************************************************************************
                          execute.cpp  -  description

                             -------------------
    begin                : Friday July 21, 2009
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

#include "oal/execute.h"

#include <QFile>
#include <QStandardItemModel>
#include <QStringListModel>

#include <kmessagebox.h>
#include <kfiledialog.h>
#include "kstarsdata.h"
#include "observer.h"
#include "observation.h"
#include "site.h"
#include "session.h"
#include "scope.h"
#include "eyepiece.h"
#include "lens.h"
#include "filter.h"
#include "observationtarget.h"
#include "observationtreeitem.h"
#include "observermanager.h"
#include "skyobjects/skyobject.h"
#include "dialogs/locationdialog.h"
#include "dialogs/finddialog.h"

using namespace OAL;

Execute::Execute()
{
    QWidget *w = new QWidget;
    m_Ui.setupUi(w);
    setMainWidget(w);
    setCaption(i18n("Execute Session"));
    setButtons(KDialog::User1|KDialog::Close);
    setButtonGuiItem(KDialog::User1, KGuiItem(i18n("End Session"), QString(), i18n("Save and End the current session")));

    m_Ui.targetsTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_Ui.targetsTreeView->header()->hide();

    m_Ks = KStars::Instance();
    m_ObservationsModel = 0;

    // Set log object
    m_LogObject = m_Ks->data()->logObject();

    m_TargetAliasesModel = new QStringListModel(m_Ui.targetAliasesListView);
    m_Ui.targetAliasesListView->setModel(m_TargetAliasesModel);

    // Initialize the lists and parameters
    init();
    m_Ui.targetsTreeView->hide();
    m_Ui.addTargetButton->hide();

    //make connections
    connect(this, SIGNAL(user1Clicked()), this, SLOT(slotSaveLog()));
    connect(m_Ui.sessionLocationButton, SIGNAL(clicked()), this, SLOT(slotLocation()));
    connect(m_Ui.targetsTreeView, SIGNAL(clicked(QModelIndex)), this, SLOT(slotSetTargetOrObservation(QModelIndex)));
    connect(m_Ui.sessionsListWidget, SIGNAL(currentRowChanged(int)), this, SLOT(slotSetSession(int)));
    connect(m_Ui.showSessionsButton, SIGNAL(leftClickedUrl()), this, SLOT(slotShowSession()));
    connect(m_Ui.showTargetsButton, SIGNAL(leftClickedUrl()), this, SLOT(slotShowTargets()));
    connect(m_Ui.sessionCoobserversButton, SIGNAL(clicked()), this, SLOT(slotObserverManager()));
    connect(m_Ui.equipmentButton, SIGNAL(clicked()), m_Ks, SLOT(slotEquipmentWriter()));
    connect(m_Ui.observationEndCheckBox, SIGNAL(toggled(bool)), this, SLOT(slotEndChecked(bool)));
    connect(m_Ui.observationFaintestStarCheckBox, SIGNAL(toggled(bool)), this, SLOT(slotFaintestStarChecked(bool)));
    connect(m_Ui.observationMagnificationCheckBox, SIGNAL(toggled(bool)), this, SLOT(slotMagnificationChecked(bool)));
    connect(m_Ui.observationSkyQualityCheckBox, SIGNAL(toggled(bool)), this, SLOT(slotSkyQualityChecked(bool)));
}

void Execute::init()
{
    //initialize geo to current location of the ObservingList
    m_CurrentLocation = m_Ks->observingList()->geoLocation();
    m_Ui.sessionLocationButton->setText( m_CurrentLocation->fullName() );

    //set the date time to the dateTime from the OL
    m_Ui.sessionBeginDateTime->setDateTime( m_Ks->observingList()->dateTime().dateTime() );

    //load Sessions
    loadSessions();

    //load Targets
    loadTargets();
}

void Execute::showObservation(Observation *observation)
{
    // Prepare widgets on Observation page
    refreshObservationPage();

    // *** GENERAL *** page
    // Set Target
    ObservationTarget *target = m_LogObject->findTargetById(observation->target());
    if(target) {
        m_Ui.observationTargetComboBox->setCurrentIndex(m_LogObject->targetList()->indexOf(target));
    } else {
        m_Ui.observationTargetComboBox->setCurrentIndex(0);
    }

    // Set Observer
    Observer *observer = m_LogObject->findObserverById(observation->observer());
    if(observer) {
        m_Ui.observationObserverComboBox->setCurrentIndex(m_LogObject->observerList()->indexOf(observer));
    } else {
        m_Ui.observationObserverComboBox->setCurrentIndex(0);
    }

    // Set Site
    Site *site = m_LogObject->findSiteById(observation->site());
    if(site) {
        m_Ui.observationSiteComboBox->setCurrentIndex(m_LogObject->siteList()->indexOf(site) + 1);
    } else {
        m_Ui.observationSiteComboBox->setCurrentIndex(0);
    }

    // Set Session
    Session *session = m_LogObject->findSessionById(observation->session());
    if(session) {
        m_Ui.observationSessionComboBox->setCurrentIndex(m_LogObject->sessionList()->indexOf(session) + 1);
    } else {
        m_Ui.observationSessionComboBox->setCurrentIndex(0);
    }

    // Set Begin
    m_Ui.observationBeginDateTime->setDateTime(observation->begin().dateTime());

    // Set End
    if(observation->isEndDefined()) {
        m_Ui.observationEndDateTime->setEnabled(true);
        m_Ui.observationEndDateTime->setDateTime(observation->end().dateTime());
        m_Ui.observationEndCheckBox->setChecked(true);
    }


    // *** EQUIPMENT *** page
    // Set Scope
    Scope *scope = m_LogObject->findScopeById(observation->scope());
    if(scope) {
        m_Ui.observationScopeComboBox->setCurrentIndex(m_LogObject->scopeList()->indexOf(scope) + 1);
    } else {
        m_Ui.observationScopeComboBox->setCurrentIndex(0);
    }

    // Set Eyepiece
    Eyepiece *eyepiece = m_LogObject->findEyepieceById(observation->eyepiece());
    if(eyepiece) {
        m_Ui.observationEyepieceComboBox->setCurrentIndex(m_LogObject->eyepieceList()->indexOf(eyepiece) + 1);
    } else {
        m_Ui.observationEyepieceComboBox->setCurrentIndex(0);
    }

    // Set Lens
    Lens *lens = m_LogObject->findLensById(observation->lens());
    if(lens) {
        m_Ui.observationLensComboBox->setCurrentIndex(m_LogObject->lensList()->indexOf(lens) + 1);
    } else {
        m_Ui.observationLensComboBox->setCurrentIndex(0);
    }

    // Set Filter
    Filter *filter = m_LogObject->findFilterById(observation->filter());
    if(filter) {
        m_Ui.observationFilterComboBox->setCurrentIndex(m_LogObject->filterList()->indexOf(filter) + 1);
    } else {
        m_Ui.observationFilterComboBox->setCurrentIndex(0);
    }

    // Set Imager

    // Set Magnification
    if(observation->isMagnificationDefined()) {
        m_Ui.observationMagnificationSpinBox->setEnabled(true);
        m_Ui.observationMagnificationSpinBox->setValue(observation->magnification());
        m_Ui.observationMagnificationCheckBox->setChecked(true);
    }

    // Set Accessories
    m_Ui.observationAccessoriesTextEdit->setPlainText(observation->accessories());

    m_Ui.observationTabWidget->setCurrentIndex(0);
    m_Ui.stackedWidget->setCurrentIndex(OBSERVATION_PAGE);
}

void Execute::showTarget(ObservationTarget *target)
{
    m_Ui.targetNameLineEdit->setText(target->name());
    m_TargetAliasesModel->setStringList(target->aliases());
    m_Ui.targetSourceComboBox->setCurrentIndex(target->fromDatasource() ? 0 : 1);
    if(target->fromDatasource()) {
        m_Ui.targetDatasourceLabel->show();
        m_Ui.targetDatasourceLineEdit->show();
        m_Ui.targetObserverLabel->hide();
        m_Ui.targetObserverComboBox->hide();
        m_Ui.targetDatasourceLineEdit->setText(target->datasource());
    } else {
        m_Ui.targetDatasourceLabel->hide();
        m_Ui.targetDatasourceLineEdit->hide();
        m_Ui.targetObserverLabel->show();
        m_Ui.targetObserverComboBox->show();
        m_Ui.targetObserverComboBox->setCurrentIndex(0);
    }
    m_Ui.targetConstellationLineEdit->setText(target->constellation());
    m_Ui.targetNotesTextEdit->setPlainText(target->notes());

    m_Ui.targetTabWidget->setCurrentIndex(0);
    m_Ui.stackedWidget->setCurrentIndex(TARGET_PAGE);
}

void Execute::showSession(OAL::Session *session)
{
    m_Ui.sessionBeginDateTime->setDateTime(session->end().dateTime());
    m_Ui.sessionEndDateTime->setDateTime(session->end().dateTime());
    m_Ui.sessionWeatherTextEdit->setPlainText(session->weather());
    m_Ui.sessionEquipmentTextEdit->setPlainText(session->equipment());
    m_Ui.sessionCommentsTextEdit->setPlainText(session->comments());
    m_Ui.sessionLanguageLineEdit->setText(session->lang());
}

void Execute::slotLocation()
{
    QPointer<LocationDialog> ld = new LocationDialog(this);
    if(ld->exec() == QDialog::Accepted) {
        m_CurrentLocation = ld->selectedCity();
        m_Ui.sessionLocationButton->setText(m_CurrentLocation -> fullName());
    }
    delete ld;
}

void Execute::slotSaveLog()
{
    KUrl fileURL = KFileDialog::getSaveUrl( QDir::homePath(), "*.xml" );

    if(fileURL.isEmpty()) {
        // Save URL dialog was cancelled
        return;
    }

    //Warn user if file exists!
    if(QFile::exists(fileURL.path())) {
        int r=KMessageBox::warningContinueCancel(parentWidget(),
                                                 i18n("A file named \"%1\" already exists. Overwrite it?", fileURL.fileName()),
                                                 i18n("Overwrite File?"),
                                                 KStandardGuiItem::overwrite());
        if(r == KMessageBox::Cancel)
            return;
    }

    if(fileURL.isValid()) {
        QFile f(fileURL.path());
        if(!f.open(QIODevice::WriteOnly)) {
            QString message = i18n("Could not open file %1", f.fileName());
            KMessageBox::sorry(this, message, i18n("Could Not Open File"));
            return;
        }

        QTextStream ostream(&f);
        ostream<< m_LogObject->writeLog(false);
        f.close();
    }
}

void Execute::slotSetTargetOrObservation(QModelIndex idx)
{
    ObservationTreeItem *itm = dynamic_cast<ObservationTreeItem*>(m_ObservationsModel->itemFromIndex(idx));
    if(!itm) {
        return;
    }

    if(itm->holdsObservation()) {
        showObservation(itm->getObservation());
    } else {
        showTarget(itm->getTarget());
    }
}

void Execute::slotSetSession(int idx)
{
    if(idx < 0 || idx >= m_LogObject->sessionList()->size()) {
        return;
    } else {
        showSession(m_LogObject->sessionList()->at(idx));
    }
}

void Execute::slotObserverManager()
{
    //saveSession();
//    m_Ks->getObserverManager()->showCoobserverColumn(true, i18n("session_") + QString::number(m_NextSession - 1));
    m_Ks->getObserverManager()->show();
}

void Execute::slotAddObject()
{
//   QPointer<FindDialog> fd = new FindDialog( m_Ks );
//   if ( fd->exec() == QDialog::Accepted ) {
//       SkyObject *o = fd->selectedObject();
//       if( o != 0 ) {
//           m_Ks->observingList()->slotAddObject( o, true );
//           init();
//       }
//   }

//   delete fd;
}

void Execute::slotShowSession()
{
    m_Ui.sessionsListWidget->show();
    m_Ui.targetsTreeView->hide();
    m_Ui.stackedWidget->setCurrentIndex(SESSION_PAGE);
    m_Ui.addTargetButton->hide();
}

void Execute::slotShowTargets()
{
    m_Ui.targetsTreeView->show();
    m_Ui.sessionsListWidget->hide();
    m_Ui.addTargetButton->show();
    m_Ui.stackedWidget->setCurrentIndex(TARGET_PAGE);
}

void Execute::slotMagnificationChecked(bool enabled)
{
    m_Ui.observationMagnificationSpinBox->setEnabled(enabled);
}

void Execute::slotSkyQualityChecked(bool enabled)
{
    m_Ui.observationSkyQualitySpinBox->setEnabled(enabled);
}

void Execute::slotEndChecked(bool enabled)
{
    m_Ui.observationEndDateTime->setEnabled(enabled);
}

void Execute::slotFaintestStarChecked(bool enabled)
{
    m_Ui.observationFaintestStarCheckBox->setEnabled(enabled);
}

void Execute::loadSessions()
{
    m_Ui.sessionsListWidget->clear();
    foreach( OAL::Session *s, *m_Ks->data()->logObject()->sessionList() ) {
        m_Ui.sessionsListWidget->addItem( s->id() );
    }
}

void Execute::loadTargets()
{
    if(m_ObservationsModel) {
        delete m_ObservationsModel;
    }
    m_ObservationsModel = new QStandardItemModel(0, 1, this);

    foreach(OAL::ObservationTarget *o, *m_LogObject->targetList()) {
        ObservationTreeItem *target = new ObservationTreeItem(o);
        m_ObservationsModel->appendRow(target);
        foreach(Observation *obs, *o->observationsList()) {
            ObservationTreeItem *observation = new ObservationTreeItem(obs);
            target->appendRow(observation);
        }
    }

    m_Ui.targetsTreeView->setModel(m_ObservationsModel);
}

void Execute::refreshObservationPage()
{
    // *** GENERAL page ***
    // Targets
    m_Ui.observationTargetComboBox->clear();
    foreach(ObservationTarget *target, *m_LogObject->targetList()) {
        QString targetStr = target->name();
        if(!target->constellation().isEmpty()) {
            targetStr += ", " + target->constellation();
        }
        m_Ui.observationTargetComboBox->addItem(targetStr);
    }

    // Observers
    m_Ui.observationObserverComboBox->clear();
    foreach(Observer *observer, *m_LogObject->observerList()) {
        m_Ui.observationObserverComboBox->addItem(observer->name() + " " + observer->surname());
    }

    // Sites
    m_Ui.observationSiteComboBox->clear();
    m_Ui.observationSiteComboBox->addItem(i18n("Undefined"));
    foreach(Site *site, *m_LogObject->siteList()) {
        m_Ui.observationSiteComboBox->addItem(site->name());
    }

    // Session
    m_Ui.observationSessionComboBox->clear();
    m_Ui.observationSessionComboBox->addItem(i18n("Undefined"));
    foreach(Session *session, *m_LogObject->sessionList()) {
        m_Ui.observationSessionComboBox->addItem(session->id());
    }

    // Begin
    m_Ui.observationBeginDateTime->setDateTime(QDateTime());

    // End
    m_Ui.observationEndDateTime->setDateTime(QDateTime());
    m_Ui.observationEndCheckBox->setChecked(false);

    // Faintest star
    m_Ui.observationFaintestStarSpinBox->setValue(0);
    m_Ui.observationFaintestStarCheckBox->setChecked(false);

    // Sky quality
    m_Ui.observationSkyQualitySpinBox->setValue(0);
    m_Ui.observationSQUnitComboBox->setCurrentIndex(0);
    m_Ui.observationSkyQualityCheckBox->setChecked(false);
    m_Ui.observationSkyQualitySpinBox->setEnabled(false);
    m_Ui.observationSQUnitComboBox->setEnabled(false);

    // Seeing
    m_Ui.observationSeeingComboBox->setCurrentIndex(0);


    // *** EQUIPMENT page ***
    // Scopes
    m_Ui.observationScopeComboBox->clear();
    m_Ui.observationScopeComboBox->addItem(i18n("Undefined"));
    foreach(Scope *scope, *m_LogObject->scopeList()) {
        m_Ui.observationScopeComboBox->addItem(scope->name() + " " + scope->model());
    }

    // Eyepieces
    m_Ui.observationEyepieceComboBox->clear();
    m_Ui.observationEyepieceComboBox->addItem(i18n("Undefined"));
    foreach(Eyepiece *eyepiece, *m_LogObject->eyepieceList()) {
        m_Ui.observationEyepieceComboBox->addItem(eyepiece->name() + " " + eyepiece->model());
    }

    // Lenses
    m_Ui.observationLensComboBox->clear();
    m_Ui.observationLensComboBox->addItem(i18n("Undefined"));
    foreach(Lens *lens, *m_LogObject->lensList()) {
        m_Ui.observationLensComboBox->addItem(lens->name() + " " + lens->model());
    }

    // Filters
    m_Ui.observationFilterComboBox->clear();
    m_Ui.observationFilterComboBox->addItem(i18n("Undefined"));
    foreach(Filter *filter, *m_LogObject->filterList()) {
        m_Ui.observationFilterComboBox->addItem(filter->name() + " " + filter->model());
    }

    // Imagers
    m_Ui.observationImagerComboBox->clear();
    m_Ui.observationImagerComboBox->addItem(i18n("Undefined"));
    // TODO - add support for Imagers

    // Magnification
    m_Ui.observationMagnificationSpinBox->setValue(1);
    m_Ui.observationMagnificationCheckBox->setChecked(false);
    m_Ui.observationMagnificationSpinBox->setEnabled(false);

    // Accessories
    m_Ui.observationAccessoriesTextEdit->clear();


    // *** FINDINGS page ***
    // Results
    m_Ui.observationResultsTextEdit->clear();
}
