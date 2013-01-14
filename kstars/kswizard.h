/***************************************************************************
                          kswizard.h  -  description
                             -------------------
    begin                : Wed 28 Jan 2004
    copyright            : (C) 2004 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSWIZARD_H_
#define KSWIZARD_H_

#include <kdialog.h>

#include "ui_wizwelcome.h"
#include "ui_wizlocation.h"
#include "ui_wizdownload.h"

class GeoLocation;
class QStackedWidget;

class WizWelcomeUI : public QFrame, public Ui::WizWelcome {
    Q_OBJECT
public:
    explicit WizWelcomeUI( QWidget *parent=0 );
};

class WizLocationUI : public QFrame, public Ui::WizLocation {
    Q_OBJECT
public:
    explicit WizLocationUI( QWidget *parent=0 );
};

class WizDownloadUI : public QFrame, public Ui::WizDownload {
    Q_OBJECT
public:
    explicit WizDownloadUI( QWidget *parent=0 );
};

/**@class KSWizard
 * The Setup Wizard will be automatically opened when KStars runs 
 * for the first time.  It allows the user to set up some basic parameters:
 * @li Geographic Location
 * @li Download extra data files
 * @author Jason Harris
 * @version 1.0
 */
class KSWizard : public KDialog
{
    Q_OBJECT
public:
    /** Constructor
     * @p parent pointer to the parent widget
     */
    explicit KSWizard( QWidget* parent = 0 );

    /**Destructor */
    virtual ~KSWizard();

    /** @return pointer to the geographic location selected by the user */
    const GeoLocation* geo() const { return Geo; }

private slots:
    void slotNextPage();
    void slotPrevPage();

    /**Set the geo pointer to the user's selected city, and display
     * its longitude and latitude in the window.
     * @note called when the highlighted city in the list box changes
     */
    void slotChangeCity();

    /**Display only those cities which meet the user's search criteria 
     * in the city list box.
     * @note called when one of the name filters is modified
     */
    void slotFilterCities();

    void slotDownload();

private:
    /**@short Initialize the geographic location page.
     * Populate the city list box, and highlight the current location in the list.
     */
    void initGeoPage();

    /**@short set enabled/disable state of Next/Prev buttins based on current page */
    void setButtonsEnabled();

    QStackedWidget *wizardStack;
    WizLocationUI  *location;

    GeoLocation *Geo;
    QList<GeoLocation*> filteredCityList;
};

#endif
