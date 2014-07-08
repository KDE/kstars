/***************************************************************************
                          skyguideslistview.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2014/04/07
    copyright            : (C) 2014 by Gioacchino Mazzurco
    email                : gmazzurco89@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SKYGUIDESLISTVIEW_H
#define SKYGUIDESLISTVIEW_H

#include <QDeclarativeView>

#include "skyguideslistmodel.h"

class QWidget;


/**
  * \class SkyGuidesListView
  * \brief Manages the QML user interface for Sky Guides.
  * SkyGuidesListView is used to display the QML UI using a QDeclarativeView.
  * It acts on all signals emitted by the UI and manages the data
  * sent to the UI for display.
  * \author Gioacchino Mazzurco
  */
class SkyGuidesListView : public QWidget
{
	Q_OBJECT

public:
	/**
	 * \brief Constructor
	 */
	SkyGuidesListView(const SkyGuidesListModel & guidesModel, QWidget * parent = NULL);

	/**
	 * \brief Destructor
	 */
	~SkyGuidesListView() {};
	
	/**
	 * \brief Displays the user interface
	 */
	inline void show() { mBaseView.show(); };

private:
	QDeclarativeView mBaseView;
};

#endif // SKYGUIDESLISTVIEW_H
