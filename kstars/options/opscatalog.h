/***************************************************************************
                          opscatalog.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 29 2004
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

#ifndef OPSCATALOG_H_
#define OPSCATALOG_H_

#include "ui_opscatalog.h"

class KStars;
class QListWidgetItem;
class KConfigDialog;

/**
 *@class OpsCatalog
 *The Catalog page for the Options window.  This page allows the user
 *to modify display of the major object catalogs in KStars:
 *@li Hipparcos/Tycho Star Catalog
 *@li Messier Catalog
 *@li NGC/IC Catalogs
 *@li Any Custom catalogs added by the user.
 *
 *@short Catalog page of the Options window.
 *@author Jason Harris
 *@version 1.0
 */
class OpsCatalog : public QFrame, public Ui::OpsCatalog
{
    Q_OBJECT

public:
    OpsCatalog( KStars *_ks );
    ~OpsCatalog();

private slots:
    void updateCustomCatalogs();
    void selectCatalog();
    void slotAddCatalog();
    void slotLoadCatalog();
    void slotRemoveCatalog();
    /*
    void slotSetDrawStarMagnitude(double newValue);
    void slotSetDrawStarZoomOutMagnitude(double newValue);
    */
    void slotStarWidgets(bool on);
    void slotDeepSkyWidgets(bool on);
    void slotApply();
    void slotCancel();

private:
    void insertCatalog( const QString & filename );
    void refreshCatalogList();
    void populateInbuiltCatalogs();
    void populateCustomCatalogs();
    QString getCatalogName( const QString &filename );

    QListWidgetItem *showMessier, *showMessImages, *showNGC, *showIC;
    KStars *ksw;

    KConfigDialog *m_ConfigDialog;
    QStringList *m_CustomCatalogFile;
    QStringList m_CheckedCatalogNames;
    QList<int> m_ShowCustomCatalog;
    float m_StarDensity;
    bool m_ShowMessier, m_ShowMessImages, m_ShowNGC, m_ShowIC;
};

#endif  //OPSCATALOG_H_

