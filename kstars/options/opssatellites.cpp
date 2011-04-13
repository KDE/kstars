/***************************************************************************
                          opssatellites.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon 21 Mar 2011
    copyright            : (C) 2011 by Jérôme SONRIER
    email                : jsid@emor3j.fr.eu.org
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "opssatellites.h"

#include <qcheckbox.h>
#include <QTreeWidgetItem>



#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymapcomposite.h"
#include "skycomponents/satellitescomponent.h"
#include "satellitegroup.h"

OpsSatellites::OpsSatellites( KStars *_ks )
        : QFrame( _ks ), ksw(_ks)
{
    setupUi( this );

    m_ConfigDialog = KConfigDialog::exists( "settings" );

    // Ppulate satellites list
    updateListView();
    
    // Signals and slots connections
    connect( UpdateTLEButton, SIGNAL( clicked() ), this, SLOT( slotUpdateTLEs() ) );
    connect( kcfg_ShowSatellites, SIGNAL( toggled( bool ) ), SLOT( slotShowSatellites( bool ) ) );
    connect( m_ConfigDialog, SIGNAL( applyClicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog, SIGNAL( okClicked() ), SLOT( slotApply() ) );
    connect( m_ConfigDialog, SIGNAL( cancelClicked() ), SLOT( slotCancel() ) );
}

OpsSatellites::~OpsSatellites() {

}

void OpsSatellites::slotUpdateTLEs()
{
    // Get new data files
    KStarsData::Instance()->skyComposite()->satellites()->updateTLEs();

    // Refresh satellites list
    updateListView();
}

void OpsSatellites::updateListView()
{
    KStarsData* data = KStarsData::Instance();

    // Clear satellites list
    SatListTreeView->clear();

    // Add each groups and satellites in the list
    foreach ( SatelliteGroup* sat_group, data->skyComposite()->satellites()->groups() ) {
        // Add the group
        QTreeWidgetItem *group_item = new QTreeWidgetItem( SatListTreeView, QStringList() << sat_group->name() );
        SatListTreeView->insertTopLevelItem( SatListTreeView->topLevelItemCount(), group_item );

        // Add all satellites of the group
        for ( int i=0; i<sat_group->count(); ++i ) {
            // Create a checkbox to (un)select satellite
            QCheckBox *sat_cb = new QCheckBox( SatListTreeView );
            sat_cb->setText( sat_group->at(i)->name() );
            if ( Options::selectedSatellites().contains( sat_group->at(i)->name() ) )
                sat_cb->setCheckState( Qt::Checked );

            // Create our item
            QTreeWidgetItem *sat_item = new QTreeWidgetItem( group_item );
            // Add checkbox to our item
            SatListTreeView->setItemWidget( sat_item, 0, sat_cb );
            // Add item to satellites list
            group_item->addChild( sat_item );
        }
    }
}

void OpsSatellites::slotApply()
{
    KStarsData* data = KStarsData::Instance();
    QString sat_name;
    QStringList selected_satellites;

    // Retrive each satellite in the list and select it if checkbox is checked
    for ( int i=0; i<SatListTreeView->topLevelItemCount(); ++i ) {
        QTreeWidgetItem *top_level_item = SatListTreeView->topLevelItem( i );
        for ( int j=0; j<top_level_item->childCount(); ++j ) {
            QTreeWidgetItem *item = top_level_item->child( j );
            QCheckBox *check_box = qobject_cast<QCheckBox*>( SatListTreeView->itemWidget( item, 0 ) );
            sat_name = check_box->text().replace("&", "");
            Satellite *sat = data->skyComposite()->satellites()->findSatellite( sat_name );
            if ( sat ) {
                if ( check_box->checkState() == Qt::Checked ) {
                    data->skyComposite()->satellites()->findSatellite( sat_name )->setSelected( true );
                    selected_satellites.append( sat_name );
                } else {
                    data->skyComposite()->satellites()->findSatellite( sat_name )->setSelected( false );
                }
            }
        }
    }

    Options::setSelectedSatellites( selected_satellites );
}

void OpsSatellites::slotCancel()
{
    // Update satellites list
    updateListView();
}

void OpsSatellites::slotShowSatellites( bool on )
{
    kcfg_ShowVisibleSatellites->setEnabled( on );
    kcfg_ShowSatellitesLabels->setEnabled( on );
    kcfg_DrawSatellitesLikeStars->setEnabled( on );
}

#include "opssatellites.moc"
