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

#ifndef LOCATIONDIALOG_H
#define LOCATIONDIALOG_H

/**
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
	*are entered into QLineEdits.  There is also a QPushButton for adding the
	*location to the custom Cities database.  If the user selects "Add" without
	*filling in all of the manual entry fields, an error message is displayed.
	*@short Geographic Location dialog
  *@author Jason Harris
	*@version 0.9
  */
#include <kdialogbase.h>
#include <qlineedit.h>
#include <qpushbutton.h>

#include "mapcanvas.h"
#include "dmsbox.h"

#include <qglobal.h>
#if (QT_VERSION > 299)
#include <qmemarray.h>
#endif

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QGroupBox;
class QLabel;
class QListBox;
class QListBoxItem;
class QPushButton;
class QComboBox;

class LocationDialog : public KDialogBase {
	Q_OBJECT

public: 
/**
	*Constructor.  Create all widgets, and pack them into QLayouts.
	*Connect Signals to Slots.  Run initCityList().
	*/
	LocationDialog( QWidget* parent = 0 );
/**
	*Destructor (empty)
	*/
	~LocationDialog();

/**
	*Initialize list of cities.  Note that the database is not read in here,
	*that is done in the KStars constructor.  This simply loads the local QListBox
	*with the names of the cities from the kstarsData object.
	*/
  void initCityList( void );

/**
	*Retrieve ID number of the highlighted City in the QListBox.
	*Because the list can be filtered, the city's position in the List
	*cannot be used to directly identify the city in the database.
	*So, we maintain an integer QArray that contains the database ID of each
	*city in the list.  This function uses the List position of the highlighted
	*city to select the corresponding database ID.
	*@returns the position of the highlighted city in the List of cities.
	*/
  int getCityIndex( void );

/**@short Show only cities within 3 degrees of point specified by arguments
	*@param longitude the longitude of the search point (int)
	*@param latitude the latitude of the search point (int)
	*/
	void findCitiesNear( int longitude, int latitude );

/**@param i the index of GeoID to examine
	*@returns the GeoID of the city at position i
	*/
	int cityIDAt( int i ) { return GeoID[i]; }

/**@returns the name of the selected city.
	*/
	QString selectedCityName( void ) { return NewCityName->text(); }

/**@returns true if the AddCityBUtton is enabled
	*/
	bool addCityEnabled() { return AddCityButton->isEnabled(); }

  QListBox *GeoBox;

public slots:
/**
	*When text is entered in the City/Province/Country Filter QLineEdits,
	*the List of cities is trimmed to show only cities beginning with the entered text.
	*Also, the QArray of ID numbers is kept in sync with the filtered list.
	*/
  void filterCity( void );

/**
	*When the selected city in the QListBox changes, repaint the MapCanvas
	*so that the crosshairs icon appears on the newly selected city.
	*/
	void changeCity( void );

/**
	*When the "Add new city" QPushButton is clicked, add the manually-entered
	*city information to the user's custom city database.
	*/
	void addCity( void );

/**
	*Make sure a valid Longitude value was entered.
	*/
	void checkLong( void );

/**
	*Make sure a valid Latitude value was entered.
	*/
	void checkLat( void );

	void clearFields( void );
	void showTZRules( void );
	void nameChanged( void );
	void dataChanged( void );

private:
  int newCity;
	bool dataModified, nameModified;
	QGridLayout *glay, *glay2;
	QHBoxLayout *hlay, *hlayCoord, *hlayTZ, *hlayButtons, *hlay3;
	QVBoxLayout *RootLay, *CityLay, *CoordLay, *vlay;
	QGroupBox *CityBox, *CoordBox;
	QLabel *CityFiltLabel, *ProvinceFiltLabel, *CountryFiltLabel;
	QLabel *NewCityLabel, *NewProvinceLabel, *NewCountryLabel;
	QLabel *LongLabel, *LatLabel, *CountLabel;
	QLabel *TZLabel, *TZRuleLabel;
	QLineEdit *NewCityName, *NewProvinceName, *NewCountryName;
	dmsBox *NewLong, *NewLat;
	QLineEdit *CityFilter, *ProvinceFilter, *CountryFilter;
	QComboBox *TZBox, *TZRuleBox;
	QPushButton *AddCityButton, *ClearFields, *ShowTZRules;
	MapCanvas *MapView;
  QArray<int> GeoID;

};

#endif
