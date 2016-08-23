/***************************************************************************
                          finddialoglite.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Jul 29 2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "finddialoglite.h"

#include "kstarsdata.h"
#include "Options.h"
#include "skyobjects/skyobject.h"
#include "skycomponents/starcomponent.h"
#include "skycomponents/skymapcomposite.h"
#include "skymaplite.h"
#include "kstarslite.h"

#include "solarsystemcomposite.h"
//Resolver
#include "tools/nameresolver.h"
#include "skycomponents/syncedcatalogcomponent.h"

#include <QSortFilterProxyModel>
#include "skyobjectlistmodel.h"
#include <QTimer>
#include <QQmlContext>

FindDialogLite::FindDialogLite( ) :
    timer(0)
{
    m_filterModel.append( i18n ("Any") );
    m_filterModel.append( i18n ("Stars") );
    m_filterModel.append( i18n ("Solar System") );
    m_filterModel.append( i18n ("Open Clusters") );
    m_filterModel.append( i18n ("Globular Clusters") );
    m_filterModel.append( i18n ("Gaseous Nebulae") );
    m_filterModel.append( i18n ("Planetary Nebulae") );
    m_filterModel.append( i18n ("Galaxies") );
    m_filterModel.append( i18n ("Comets") );
    m_filterModel.append( i18n ("Asteroids") );
    m_filterModel.append( i18n ("Constellations") );
    m_filterModel.append( i18n ("Supernovae") );
    m_filterModel.append( i18n ("Satellites") );
    emit filterModelChanged();

    fModel = new SkyObjectListModel( this );
    m_sortModel = new QSortFilterProxyModel(this);
    m_sortModel->setFilterCaseSensitivity( Qt::CaseInsensitive );
    m_sortModel->setSourceModel( fModel );
    m_sortModel->setSortRole(SkyObjectListModel::NameRole);
    m_sortModel->setFilterRole(SkyObjectListModel::NameRole);
    m_sortModel->setDynamicSortFilter(true);
    KStarsLite::Instance()->qmlEngine()->rootContext()->setContextProperty("SortModel", m_sortModel);
    m_sortModel->sort( 0 );

    listFiltered = false;
}

FindDialogLite::~FindDialogLite() { }

void FindDialogLite::filterByType(uint typeIndex) {
    KStarsData *data = KStarsData::Instance();

    switch ( typeIndex ) {
    case 0: // All object types
    {
        QVector<QPair<QString, const SkyObject *>> allObjects;
        foreach( int type, data->skyComposite()->objectLists().keys() ) {
            allObjects.append(data->skyComposite()->objectLists(SkyObject::TYPE(type)));
        }
        fModel->setSkyObjectsList( allObjects );
        break;
    }
    case 1: //Stars
    {
        QVector<QPair<QString, const SkyObject *>> starObjects;
        starObjects.append(data->skyComposite()->objectLists(SkyObject::STAR));
        starObjects.append(data->skyComposite()->objectLists(SkyObject::CATALOG_STAR));
        fModel->setSkyObjectsList( starObjects );
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
        fModel->setSkyObjectsList( data->skyComposite()->objectLists( SkyObject::OPEN_CLUSTER ) );
        break;
    case 4: //Globular Clusters
        fModel->setSkyObjectsList( data->skyComposite()->objectLists( SkyObject::GLOBULAR_CLUSTER ) );
        break;
    case 5: //Gaseous nebulae
        fModel->setSkyObjectsList( data->skyComposite()->objectLists( SkyObject::GASEOUS_NEBULA ) );
        break;
    case 6: //Planetary nebula
        fModel->setSkyObjectsList( data->skyComposite()->objectLists( SkyObject::PLANETARY_NEBULA ) );
        break;
    case 7: //Galaxies
        fModel->setSkyObjectsList( data->skyComposite()->objectLists( SkyObject::GALAXY ) );
        break;
    case 8: //Comets
        fModel->setSkyObjectsList( data->skyComposite()->objectLists( SkyObject::COMET ) );
        break;
    case 9: //Asteroids
        fModel->setSkyObjectsList( data->skyComposite()->objectLists( SkyObject::ASTEROID ) );
        break;
    case 10: //Constellations
        fModel->setSkyObjectsList( data->skyComposite()->objectLists( SkyObject::CONSTELLATION ) );
        break;
    case 11: //Supernovae
        fModel->setSkyObjectsList( data->skyComposite()->objectLists( SkyObject::SUPERNOVA ) );
        break;
    case 12: //Satellites
        fModel->setSkyObjectsList( data->skyComposite()->objectLists( SkyObject::SATELLITE ) );
        break;
    }
}

void FindDialogLite::filterList(QString searchQuery) {
    QString SearchText = processSearchText(searchQuery);
    m_sortModel->setFilterFixedString( SearchText );
    listFiltered = true;
}

void FindDialogLite::selectObject(int index) {
    QVariant sObj = m_sortModel->data(m_sortModel->index(index, 0), SkyObjectListModel::SkyObjectRole);
    SkyObject *skyObj = (SkyObject *) sObj.value<void *>();
    SkyMapLite::Instance()->slotSelectObject(skyObj);
}

// Process the search box text to replace equivalent names like "m93" with "m 93"
QString FindDialogLite::processSearchText(QString text) {
    QRegExp re;
    QString searchtext = text;

    re.setCaseSensitivity( Qt::CaseInsensitive );

    // If it is an NGC/IC/M catalog number, as in "M 76" or "NGC 5139", check for absence of the space
    re.setPattern("^(m|ngc|ic)\\s*\\d*$");
    if(text.contains(re)) {
        re.setPattern("\\s*(\\d+)");
        searchtext.replace( re, " \\1" );
        re.setPattern("\\s*$");
        searchtext.remove( re );
        re.setPattern("^\\s*");
        searchtext.remove( re );
    }

    // TODO after KDE 4.1 release:
    // If it is a IAU standard three letter abbreviation for a constellation, then go to that constellation
    // Check for genetive names of stars. Example: alp CMa must go to alpha Canis Majoris

    return searchtext;
}

void FindDialogLite::resolveInInternet(QString searchQuery) {
    SkyObject *selObj = 0;
    CatalogEntryData cedata;
    cedata = NameResolver::resolveName( processSearchText(searchQuery) );
    DeepSkyObject *dso = 0;
    if( ! std::isnan( cedata.ra ) && ! std::isnan( cedata.dec ) ) {
        dso = KStarsData::Instance()->skyComposite()->internetResolvedComponent()->addObject( cedata );
        if( dso )
            qDebug() << dso->ra0().toHMSString() << ";" << dso->dec0().toDMSString();
        selObj = dso;
    }
    if ( selObj == 0 ) {
        /*QString message = i18n( "No object named %1 found.", ui->SearchBox->text() );
        KMessageBox::sorry( 0, message, i18n( "Bad object name" ) );*/
    }
}
