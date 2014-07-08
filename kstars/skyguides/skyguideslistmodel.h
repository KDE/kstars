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

#ifndef SKYGUIDESLISTMODEL_H
#define SKYGUIDESLISTMODEL_H

#include <QList>

class QObject;
class SkyObject;

/**
  * \class SkyGuidesListModel
  * \brief Access Sky Guides data.
  * \author Gioacchino Mazzurco
  */
class SkyGuidesListModel : public QList<QObject*>
{
public:
	/**
	 * \brief Constructor.
	 */
	SkyGuidesListModel(const SkyObject * obj);

	/**
	 * \brief Copy Constructor
	 */
	SkyGuidesListModel(const SkyGuidesListModel & skglm);

	/**
	 * \brief Default Constructor
	 */
	SkyGuidesListModel();

	/**
	 * \brief Destructor
	 */
	~SkyGuidesListModel();
	
	/**
	 * \brief Set SkyObject for the model
	 * Set SkyObject for the model
	 * Guides will be scanned to look for guides matching with the current SkyObject
	 */
	void updateSkyObject(const SkyObject * obj);

private:
	SkyObject * actualSkyObject;
};

Q_DECLARE_METATYPE(SkyGuidesListModel)

#endif // SKYGUIDESLISTMODEL_H