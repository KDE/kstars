/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "geolocation.h"
#include "ui_locationdialog.h"

#include <QPointer>
#ifdef HAVE_GEOCLUE2
#include <QGeoPositionInfo>
#include <QGeoPositionInfoSource>
#endif
#include <QDialog>
#include <QList>

class QTimer;
class QNetworkAccessManager;
class QNetworkReply;

class LocationDialogUI : public QFrame, public Ui::LocationDialog
{
    Q_OBJECT
  public:
    explicit LocationDialogUI(QWidget *parent = nullptr);
};

/**
 * @class LocationDialog
 * Dialog for changing the geographic location of the observer.  The
 * dialog is divided into two sections.
 *
 * The top section allows the location to be selected from a database
 * of 2000 cities.  It contains a MapCanvas (showing map of the globe
 * with cities overlaid, with a handler for mouse clicks), a QListBox
 * containing the names of cities in the database, and three QLineEdit
 * widgets, which allow the user to filter the List by the name of the
 * City, Province, and Country.  In addition, the List
 * can be filtered by location, by clicking anywhere in the MapCanvas.
 * Doing so will display cities within 2 degrees of the clicked position.
 *
 * The bottom section allows the location to be specified manually.
 * The Longitude, Latitude, City name, Province/State name, and Country name
 * are entered into KLineEdits.  There is also a QPushButton for adding the
 * location to the custom Cities database.  If the user selects "Add" without
 * filling in all of the manual entry fields, an error message is displayed.
 *
 * The user can fetch current geographic location from QtPosition system. The actual
 * underlying location source depends on the OS and what modules are currently available, if any.
 *
 * @short Geographic Location dialog
 * @author Jason Harris
 * @author Jasem Mutlaq
 * @author Artem Fedoskin
 * @version 1.1
 */
class LocationDialog : public QDialog
{
    Q_OBJECT

  public:
    typedef enum { CITY_ADD, CITY_UPDATE, CITY_REMOVE } CityOperation;

    /**
     * Constructor.  Create all widgets, and pack them into QLayouts.
     * Connect Signals to Slots.  Run initCityList().
     */
    explicit LocationDialog(QWidget *parent);

    /**
     * Initialize list of cities.  Note that the database is not read in here,
     * that is done in the KStars constructor.  This simply loads the local QListBox
     * with the names of the cities from the kstarsData object.
     */
    void initCityList(void);

    /** @return pointer to the highlighted city in the List. */
    GeoLocation *selectedCity() const { return SelectedCity; }

    /** @return pointer to the List of filtered city pointers. */
    QList<GeoLocation *> filteredList() { return filteredCityList; }

    /**
     * @short Show only cities within 3 degrees of point specified by arguments
     * @param longitude the longitude of the search point (int)
     * @param latitude the latitude of the search point (int)
     */
    void findCitiesNear(int longitude, int latitude);

    /** @return the city name of the selected location. */
    QString selectedCityName() const { return SelectedCity->translatedName(); }

    /** @return the province name of the selected location. */
    QString selectedProvinceName() const { return SelectedCity->translatedProvince(); }

    /** @return the country name of the selected location. */
    QString selectedCountryName() const { return SelectedCity->translatedCountry(); }

  public slots:
    /**
     * When text is entered in the City/Province/Country Filter KLineEdits, the List of cities is
     * trimmed to show only cities beginning with the entered text.  Also, the QMemArray of ID
     * numbers is kept in sync with the filtered list.
     */
    void filterCity();

    /**
     * @short Filter by city / province / country only after a few milliseconds
     */
    void enqueueFilterCity();

    /**
     * When the selected city in the QListBox changes, repaint the MapCanvas
     * so that the crosshairs icon appears on the newly selected city.
     */
    void changeCity();

    /**
     * When the "Add new city" QPushButton is clicked, add the manually-entered
     * city information to the user's custom city database.
     * @return true on success
     */
    bool addCity();

    /**
     * When the "Update City" QPushButton is clicked, update the city
     * information in the user's custom city database.
     * @return true on success
     */
    bool updateCity();

    /**
     * When the "Remove City" QPushButton is clicked, remove the
     * city information from the user's custom city database.
     * @return true on success
     */
    bool removeCity();

    /**
     * @brief updateCity Adds, updates, or removes a city from the user's database.
     * @param operation Add, update, or remove city
     * @return true on success
     */
    bool updateCity(LocationDialog::CityOperation operation);

    // FIXME Disable this until Qt5 works with Geoclue2
    #ifdef HAVE_GEOCLUE_2
    /**
     * @brief getNameFromCoordinates Given the current latitude and longitude, use Google Location API services to reverse lookup
     * the city, province, and country located at the requested position.
     * @param latitude Latitude in degrees
     * @param longitude Longitude is degrees
     */
    void getNameFromCoordinates(double latitude, double longitude);
    #endif

    void clearFields();
    void showTZRules();
    void nameChanged();
    void dataChanged();
    void slotOk();

protected slots:
    // FIXME Disable this until Qt5 works with Geoclue2
    #ifdef HAVE_GEOCLUE_2
    void processLocationNameData(QNetworkReply *rep);
    void requestUpdate();
    void positionUpdated(const QGeoPositionInfo &info);
    void positionUpdateError(QGeoPositionInfoSource::Error error);
    void positionUpdateTimeout();
    #endif

  private:
    /** Make sure Longitude and Latitude values are valid. */
    bool checkLongLat();

    bool dataModified { false };
    bool nameModified { false };

    LocationDialogUI *ld { nullptr };
    GeoLocation *SelectedCity { nullptr };
    QList<GeoLocation *> filteredCityList;
    QTimer *timer { nullptr };
    //Retrieve the name of city

    #ifdef HAVE_GEOCLUE_2
    QNetworkAccessManager *nam { nullptr };
    QPointer<QGeoPositionInfoSource> source;
    #endif
};
