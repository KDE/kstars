/***************************************************************************
                          locationdialog.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
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

#ifndef LOCATIONDIALOG_H_
#define LOCATIONDIALOG_H_

/** @class LocationDialog
	*Dialog for changing the geographic location of the observer.  The
	*dialog is divided into two sections.
	*
	*The top section allows the location to be selected from a database
	*of 2000 cities.  It contains a MapCanvas (showing map of the globe
	*with cities overlaid, with a handler for mouse clicks), a QListBox
	*containing the names of cities in the database, and three QLineEdit
	*widgets, which allow the user to filter the List by the name of the
	*City, Province, and Country.  In addition, the List
	*can be filtered by location, by clicking anywhere in the MapCanvas.
	*Doing so will display cities within 2 degrees of the clicked position.
	*
	*The bottom section allows the location to be specified manually.
	*The Longitude, Latitude, City name, Province/State name, and Country name
	*are entered into KLineEdits.  There is also a QPushButton for adding the
	*location to the custom Cities database.  If the user selects "Add" without
	*filling in all of the manual entry fields, an error message is displayed.
	*@short Geographic Location dialog
	*@author Jason Harris
	*@version 1.0
	*/

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTimer>
#include <QDialog>

#include "geolocation.h"

#include "ui_locationdialog.h"

class LocationDialogUI : public QFrame, public Ui::LocationDialog {
    Q_OBJECT
public:
    explicit LocationDialogUI( QWidget *parent=0 );
};


class LocationDialog : public QDialog {
    Q_OBJECT

public:

    typedef enum { CITY_ADD, CITY_UPDATE, CITY_REMOVE } CityOperation;

    /**Constructor.  Create all widgets, and pack them into QLayouts.
     * Connect Signals to Slots.  Run initCityList().
     */
    explicit LocationDialog( QWidget* parent );

    /**Initialize list of cities.  Note that the database is not read in here,
     * that is done in the KStars constructor.  This simply loads the local QListBox
     * with the names of the cities from the kstarsData object.
     */
    void initCityList( void );

    /** @return pointer to the highlighted city in the List. */
    GeoLocation* selectedCity() const { return SelectedCity; }

    /** @return pointer to the List of filtered city pointers. */
    QList<GeoLocation*> filteredList() { return filteredCityList; }

    /**
     * @short Show only cities within 3 degrees of point specified by arguments
     * @param longitude the longitude of the search point (int)
     * @param latitude the latitude of the search point (int)
     */
    void findCitiesNear( int longitude, int latitude );

    /** @return the city name of the selected location. */
    QString selectedCityName( void ) const { return SelectedCity->translatedName(); }

    /** @return the province name of the selected location. */
    QString selectedProvinceName( void ) const { return SelectedCity->translatedProvince(); }

    /** @return the country name of the selected location. */
    QString selectedCountryName( void ) const { return SelectedCity->translatedCountry(); }

    /** @return true if the AddCityBUtton is enabled */
    bool addCityEnabled();

public slots:
    /**
     * When text is entered in the City/Province/Country Filter
     * KLineEdits, the List of cities is trimmed to show only cities
     * beginning with the entered text.  Also, the QMemArray of ID
     * numbers is kept in sync with the filtered list.
     */
    void filterCity();

    /**
     * @short Filter by city / province / country only after a few milliseconds
     */
    void enqueueFilterCity();

    /**When the selected city in the QListBox changes, repaint the MapCanvas
     * so that the crosshairs icon appears on the newly selected city.
     */
    void changeCity();

    /**When the "Add new city" QPushButton is clicked, add the manually-entered
     * city information to the user's custom city database.
     * @return true on success
     */
    bool addCity();

    /**When the "Update City" QPushButton is clicked, update the city
     * information in the user's custom city database.
     * @return true on success
     */
    bool updateCity();

    /**When the "Remove City" QPushButton is clicked, remove the
     * city information from the user's custom city database.
     * @return true on success
     */
    bool removeCity();

    /**
     * @brief updateCity Adds, updates, or removes a city from the user's database.
     * @param operation Add, update, or remove city
     * @return true on success
     */
    bool updateCity(CityOperation operation);


    void clearFields();
    void showTZRules();
    void nameChanged();
    void dataChanged();
    void slotOk();

private:
    /** Make sure Longitude and Latitude values are valid. */
    bool checkLongLat( void );

    bool dataModified, nameModified;

    LocationDialogUI *ld;
    GeoLocation *SelectedCity;
    QList<GeoLocation*> filteredCityList;
    QTimer *timer;
};

#endif
