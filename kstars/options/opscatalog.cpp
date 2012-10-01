/***************************************************************************
                          opscatalog.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 29  2004
    copyright            : (C) 2004 by Jason Harris
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

#include "opscatalog.h"

#include <QList>
#include <QListWidgetItem>
#include <QTextStream>

#include <kfiledialog.h>
#include <kactioncollection.h>
#include <kconfigdialog.h>

#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "dialogs/addcatdialog.h"
#include "widgets/magnitudespinbox.h"
#include "skycomponents/catalogcomponent.h"
#include "skycomponents/skymapcomposite.h"

OpsCatalog::OpsCatalog( KStars *_ks )
        : QFrame( _ks ), ksw(_ks)
{
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists( "settings" );

    //Populate CatalogList
    populateInbuiltCatalogs();

    m_ShowMessier = Options::showMessier();
    m_ShowMessImages = Options::showMessierImages();
    m_ShowNGC = Options::showNGC();
    m_ShowIC = Options::showIC();

    //    kcfg_MagLimitDrawStar->setValue( Options::magLimitDrawStar() );
    kcfg_StarDensity->setValue( Options::starDensity() );
    //    kcfg_MagLimitDrawStarZoomOut->setValue( Options::magLimitDrawStarZoomOut() );
    //    m_MagLimitDrawStar = kcfg_MagLimitDrawStar->value();
    m_StarDensity = kcfg_StarDensity->value();
    //    m_MagLimitDrawStarZoomOut = kcfg_MagLimitDrawStarZoomOut->value();

    //    kcfg_MagLimitDrawStar->setMinimum( Options::magLimitDrawStarZoomOut() );
    //    kcfg_MagLimitDrawStarZoomOut->setMaximum( 12.0 );

    kcfg_MagLimitDrawDeepSky->setMaximum( 16.0 );
    kcfg_MagLimitDrawDeepSkyZoomOut->setMaximum( 16.0 );

    //disable star-related widgets if not showing stars
    if ( ! kcfg_ShowStars->isChecked() ) slotStarWidgets(false);

    //Add custom catalogs, if necessary
    /*
     * 1) Get the list from DB and add it as unchecked
     * 2) If the showCatalogNames list has any of the items, check it
    */
    m_CustomCatalogFile = ksw->data()->catalogdb()->Catalogs();
    m_CheckedCatalogNames = Options::showCatalogNames();
    populateCustomCatalogs();

    connect( CatalogList, SIGNAL( itemClicked( QListWidgetItem* ) ), this, SLOT( updateCustomCatalogs() ) );
    connect( CatalogList, SIGNAL( itemSelectionChanged() ), this, SLOT( selectCatalog() ) );
    connect( AddCatalog, SIGNAL( clicked() ), this, SLOT( slotAddCatalog() ) );
    connect( LoadCatalog, SIGNAL( clicked() ), this, SLOT( slotLoadCatalog() ) );
    connect( RemoveCatalog, SIGNAL( clicked() ), this, SLOT( slotRemoveCatalog() ) );

    /*
    connect( kcfg_MagLimitDrawStar, SIGNAL( valueChanged(double) ),
             SLOT( slotSetDrawStarMagnitude(double) ) );
    connect( kcfg_MagLimitDrawStarZoomOut, SIGNAL( valueChanged(double) ),
             SLOT( slotSetDrawStarZoomOutMagnitude(double) ) );
    */
    connect( kcfg_ShowStars, SIGNAL( toggled(bool) ), SLOT( slotStarWidgets(bool) ) );
    connect( kcfg_ShowDeepSky, SIGNAL( toggled(bool) ), SLOT( slotDeepSkyWidgets(bool) ) );
    connect( kcfg_ShowDeepSkyNames, SIGNAL( toggled(bool) ), kcfg_DeepSkyLongLabels, SLOT( setEnabled(bool) ) );
    connect( m_ConfigDialog, SIGNAL( applyClicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog, SIGNAL( okClicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog, SIGNAL( cancelClicked() ), SLOT( slotCancel() ) );
}

//empty destructor
OpsCatalog::~OpsCatalog() {}


void OpsCatalog::updateCustomCatalogs() {
    m_ShowMessier = showMessier->checkState();
    m_ShowMessImages = showMessImages->checkState();
    m_ShowNGC = showNGC->checkState();
    m_ShowIC = showIC->checkState();

    int limit = m_CustomCatalogFile->size();
    for ( int i=0; i < limit; ++i ) {
        QString name = m_CustomCatalogFile->at(i);
        QList<QListWidgetItem*> l = CatalogList->findItems( name, Qt::MatchExactly );

        /*
         * Options::CatalogNames contains the list of those custom catalog
         * names which are to be checked.
         * For every checked item, we check if the option CatalogNames has
         * the name. If not, we add it.
         * For every unchecked item, we check if the option CatalogNames does
         * not contain the name. If it does, we remove it.
        */
        if (l.count() == 0) continue;  // skip the name if no match found
        if ( l[0]->checkState()==Qt::Checked ) {
            if (!m_CheckedCatalogNames.contains(name))
                m_CheckedCatalogNames.append(name);
        } else if ( l[0]->checkState()==Qt::Unchecked ){
            if (m_CheckedCatalogNames.contains(name))
                m_CheckedCatalogNames.removeAll(name);
        }
    }

    m_ConfigDialog->enableButtonApply( true );
}


void OpsCatalog::selectCatalog() {
    //If selected item is a custom catalog, enable the remove button (otherwise, disable it)
    RemoveCatalog->setEnabled( false );

    if ( ! CatalogList->currentItem() ) return;

    foreach ( SkyComponent *sc, ksw->data()->skyComposite()->customCatalogs() ) {
        CatalogComponent *cc = (CatalogComponent*)sc;
        if ( CatalogList->currentItem()->text() == cc->name() ) {
            RemoveCatalog->setEnabled( true );
            break;
        }
    }
}


void OpsCatalog::slotAddCatalog() {
    QPointer<AddCatDialog> ac = new AddCatDialog( ksw );
    if ( ac->exec()==QDialog::Accepted )
        ksw->data()->catalogdb()->AddCatalogContents( ac->filename() );
        refreshCatalogList();
    delete ac;
}


void OpsCatalog::slotLoadCatalog() {
    //Get the filename from the user
    QString filename = KFileDialog::getOpenFileName( QDir::homePath(), "*");
    if ( ! filename.isEmpty() ) {
        ksw->data()->catalogdb()->AddCatalogContents(filename);
        refreshCatalogList();
    }
}


void OpsCatalog::refreshCatalogList() {
    ksw->data()->catalogdb()->Catalogs();
    populateCustomCatalogs();
}


void OpsCatalog::slotRemoveCatalog() {
    if (KMessageBox::warningYesNo(0,
                     i18n("The selected database will be removed. "
                           "This action cannot be reversed! Delete Catalog?"),
                          i18n("Delete Catalog?") )
            == KMessageBox::No) {
            KMessageBox::information(0, "Catalog deletion cancelled.");
            return;
    }
    //Ask DB to remove catalog
    ksw->data()->catalogdb()->RemoveCatalog( CatalogList->currentItem()->text() );

    //Remove entry in the QListView
    QListWidgetItem *todelete = CatalogList->takeItem( CatalogList->row( CatalogList->currentItem() ) );
    delete todelete;
    refreshCatalogList();
//     m_ConfigDialog->enableButtonApply( true );
}

/*
void OpsCatalog::slotSetDrawStarMagnitude(double newValue) {
    m_MagLimitDrawStar = newValue;
    kcfg_MagLimitDrawStarZoomOut->setMaximum( newValue );
    m_ConfigDialog->enableButtonApply( true );
}

void OpsCatalog::slotSetDrawStarZoomOutMagnitude(double newValue) {
    m_MagLimitDrawStarZoomOut = newValue;
    kcfg_MagLimitDrawStar->setMinimum( newValue );
    m_ConfigDialog->enableButtonApply( true );
}
*/

void OpsCatalog::slotApply() {
    Options::setStarDensity( kcfg_StarDensity->value() );
    //    Options::setMagLimitDrawStarZoomOut( kcfg_MagLimitDrawStarZoomOut->value() );

    //FIXME: need to add the ShowDeepSky meta-option to the config dialog!
    //For now, I'll set showDeepSky to true if any catalog options changed
    if ( m_ShowMessier != Options::showMessier() || m_ShowMessImages != Options::showMessierImages()
         || m_ShowNGC != Options::showNGC() || m_ShowIC != Options::showIC() ) {
        Options::setShowDeepSky( true );
    }

    updateCustomCatalogs();

    Options::setShowMessier( m_ShowMessier );
    Options::setShowMessierImages( m_ShowMessImages );
    Options::setShowNGC( m_ShowNGC );
    Options::setShowIC( m_ShowIC );

    Options::setShowCatalogNames(m_CheckedCatalogNames);

    // update time for all objects because they might be not initialized
    // it's needed when using horizontal coordinates
    ksw->data()->setFullTimeUpdate();
    ksw->updateTime();
    ksw->map()->forceUpdate();
}


void OpsCatalog::slotCancel() {
    //Revert all local option placeholders to the values in the global config object
    //    m_MagLimitDrawStar = Options::magLimitDrawStar();
    m_StarDensity = Options::starDensity();
    //    m_MagLimitDrawStarZoomOut = Options::magLimitDrawStarZoomOut();

    m_ShowMessier = Options::showMessier();
    m_ShowMessImages = Options::showMessierImages();
    m_ShowNGC = Options::showNGC();
    m_ShowIC = Options::showIC();

    m_ShowCustomCatalog = Options::showCatalog();

}


void OpsCatalog::slotStarWidgets(bool on) {
    //    LabelMagStars->setEnabled(on);
    LabelStarDensity->setEnabled(on);
    //    LabelMagStarsZoomOut->setEnabled(on);
    LabelDensity->setEnabled(on);
    //    LabelMag1->setEnabled(on);
    //    LabelMag2->setEnabled(on);
    //    kcfg_MagLimitDrawStar->setEnabled(on);
    kcfg_StarDensity->setEnabled(on);
    LabelStarDensity->setEnabled( on );
    //    kcfg_MagLimitDrawStarZoomOut->setEnabled(on);
    kcfg_StarLabelDensity->setEnabled(on);
    kcfg_ShowStarNames->setEnabled(on);
    kcfg_ShowStarMagnitudes->setEnabled(on);
}


void OpsCatalog::slotDeepSkyWidgets(bool on) {
    CatalogList->setEnabled( on );
    AddCatalog->setEnabled( on );
    LoadCatalog->setEnabled( on );
    LabelMagDeepSky->setEnabled( on );
    LabelMagDeepSkyZoomOut->setEnabled( on );
    kcfg_MagLimitDrawDeepSky->setEnabled( on );
    kcfg_MagLimitDrawDeepSkyZoomOut->setEnabled( on );
    kcfg_ShowDeepSkyNames->setEnabled( on );
    kcfg_ShowDeepSkyMagnitudes->setEnabled( on );
    kcfg_DeepSkyLabelDensity->setEnabled( on );
    kcfg_DeepSkyLongLabels->setEnabled( on );
    LabelMag3->setEnabled( on );
    LabelMag4->setEnabled( on );
    if ( on ) {
        //Enable RemoveCatalog if the selected catalog is custom
        selectCatalog();
    } else {
        RemoveCatalog->setEnabled( on );
    }
}


void OpsCatalog::populateInbuiltCatalogs() {
    showIC = new QListWidgetItem( i18n( "Index Catalog (IC)" ), CatalogList );
    showIC->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
    showIC->setCheckState( Options::showIC() ?  Qt::Checked : Qt::Unchecked );

    showNGC = new QListWidgetItem( i18n( "New General Catalog (NGC)" ), CatalogList );
    showNGC->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
    showNGC->setCheckState( Options::showNGC() ?  Qt::Checked : Qt::Unchecked );

    showMessImages = new QListWidgetItem( i18n( "Messier Catalog (images)" ), CatalogList );
    showMessImages->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
    showMessImages->setCheckState( Options::showMessierImages()  ?  Qt::Checked : Qt::Unchecked );

    showMessier = new QListWidgetItem( i18n( "Messier Catalog (symbols)" ), CatalogList );
    showMessier->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
    showMessier->setCheckState( Options::showMessier() ?  Qt::Checked : Qt::Unchecked );

}

void OpsCatalog::populateCustomCatalogs() {
    QStringList toggleNames = Options::showCatalogNames();
    QStringList customList = *m_CustomCatalogFile;  // Create a copy
    QStringListIterator catalogIter(customList);

    while ( catalogIter.hasNext() ) {
        QString catalogname = catalogIter.next();
        //Skip already existing items
        if (CatalogList->findItems( catalogname, Qt::MatchExactly ).length() > 0)
          continue;

        //Allocate new catalog list item
        QListWidgetItem *newItem = new QListWidgetItem( catalogname, CatalogList );
        newItem->setFlags( Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );

        if ( toggleNames.contains( catalogname ) ) {
          newItem->setCheckState( Qt::Checked );
        } else {
          newItem->setCheckState( Qt::Unchecked );
        }
    }

}


#include "opscatalog.moc"
