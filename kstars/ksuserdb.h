/***************************************************************************
                          ksuserdb.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed May 2 2012
    copyright            : (C) 2012 by Rishab Arora
    email                : ra.rishab@gmail.com
 ***************************************************************************/


/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSUSERDB_H_
#define KSUSERDB_H_
#define KSTARS_USERDB "data/userdb.sqlite"
#include <QSqlDatabase>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QString>
#include <QHash>
#include <QSqlError>
#include <QVariant>
#include <QFile>
#include "skyobjects/skyobject.h"
#include <kstandarddirs.h>
#include "oal/oal.h"


class KSUserDB
{
public:
    /** Initialize KStarsDB while running splash screen
     * @return true on success
     */
    bool Initialize();
    ~KSUserDB();

    /**
     * @brief Adds a new observer into the database
     **/
    void AddObserver(QString name, QString surname, QString contact);
    /**
     * @brief Returns the unique id of the user with given name & surname
     *
     * @return int
     **/
    int FindObserver(QString name, QString surname);    
    /**
     * @brief Removes the user with unique id as given by FindObserver
     * Returns false if the user is not found
     *
     * @return bool
     **/
    bool DeleteObserver(QString id);
    /**
     * @brief Updates the passed reference of observer_list with all observers
     * The original content of the list is cleared.
     *
     * @return void
     **/
    void GetAllObservers(QList<OAL::Observer *> &observer_list);

    /**
     * @brief Erases all the flags from the database
     *
     * @return void
     **/
    void EraseAllFlags();
    /**
     * @brief Add a new Flag with given parameters
     *
     * @param ra Right Ascension
     * @param dec Declination
     * @param epoch Epoch
     * @param image_name Name of the image used
     * @param label Content of display label on screen
     * @param labelColor Color of the label (name or hex code) eg #00FF00
     * @return void
     **/
    void AddFlag(QString ra, QString dec, QString epoch, 
                 QString image_name, QString label, QString labelColor);
    /**
     * @brief Returns a QList populated with all stored flags
     *
     * @return QList< QStringList >
     **/
    QList<QStringList> ReturnAllFlags();

    /**
     * @brief Erase the equipment with given type and unique id
     * Valid equipment types: "telescope","lens","filter"
     *
     * @param type Equipment type (same as table name)
     * @param id Unique id (same as row number)
     * @return void
     **/
    void EraseEquipment(QString type, int id);
    /**
     * @brief Erases the whole equipment table of given type
     *
     * @param type Equipment type (same as table name)
     * @return void
     **/
    void EraseAllEquipment(QString type); 

    /**
     * @brief Appends the scope with given details in the database
     *
     * @return void
     **/
    void AddScope(QString model, QString vendor, QString driver,
                  QString type, double focalLength, double aperture);
    /**
     * @brief Replaces the scope with given ID with provided content
     *
     * @return void
     **/
    void AddScope(QString model, QString vendor, QString driver,
                  QString type, double focalLength, double aperture, 
                  QString id);
    /**
     * @brief updates the scope list with all scopes from database
     * List is cleared and then filled with content.
     *
     * @param m_scopeList Reference to list to be updated
     * @return void
     **/
    void GetAllScopes(QList<OAL::Scope *> &m_scopeList);

    /**
     * @brief Add new eyepiece to database
     *
     * @return void
     **/
    void AddEyepiece(QString vendor, QString model, double focalLength, 
                           double fov, QString fovunit);
    /**
     * @brief Replace eyepiece at position (ID) with new content
     *
     * @return void
     **/
    void AddEyepiece(QString vendor, QString model, double focalLength, 
                           double fov, QString fovunit, QString id);
    /**
     * @brief Populate the reference passed with all eyepieces
     *
     * @param m_eyepieceList Reference to list of eyepieces
     * @return void
     **/
    void GetAllEyepieces(QList<OAL::Eyepiece *> &m_eyepieceList);

    /**
     * @brief Add a new lens to the database
     *
     * @return void
     **/
    void AddLens(QString vendor, QString model, double factor);
    /**
     * @brief Replace a lens at given ID with new content 
     *
     * @return void
     **/
    void AddLens(QString vendor, QString model, double factor, QString id);
    /**
     * @brief Populate the reference passed with all lenses
     *
     * @param m_lensList Reference to list of lenses
     * @return void
     **/
    void GetAllLenses(QList<OAL::Lens *>& m_lensList);
    
    /**
     * @brief Add a new filter to the database
     *
     * @return void
     **/
    void AddFilter(QString vendor, QString model, QString type, QString color);
    /**
     * @brief Replace a filter at given ID with new content 
     *
     * @return void
     **/
    void AddFilter(QString vendor, QString model, QString type, QString color, 
                   QString id);
    /**
     * @brief Populate the reference passed with all filters
     *
     * @param m_filterList Reference to list of filters
     * @return void
     **/
    void GetAllFilters(QList<OAL::Filter *>& m_filterList);
private:
    /**
     * @brief This function initializes a new database in the user's directory.
     * To be run only when a new db is needed. Should not be run over existing 
     * database file.
     *
     * @return bool
     **/
    bool FirstRun();
    /**
     * @brief Linked to the user database _once_.
     **/
    QSqlDatabase userdb_;
    /**
     * @brief Function to return the last error encountered by SQLite
     *
     * @return QSqlError
     **/
    QSqlError LastError();
};

#endif // KSUSERDB_H_
