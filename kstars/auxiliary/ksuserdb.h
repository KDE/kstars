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

#ifndef KSTARS_KSUSERDB_H
#define KSTARS_KSUSERDB_H
#define KSTARS_USERDB "data/userdb.sqlite"

#include <KLocalizedString>
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
#include <QXmlStreamReader>
#include "skyobjects/skyobject.h"
#include "oal/oal.h"
#include "auxiliary/profileinfo.h"

class LineList;
class ArtificialHorizonEntity;

/**
 * @brief Single class to delegate all User database I/O
 * 
 * usage: Call QSqlDatabase::removeDatabase("userdb"); after the object
 * of this class is deallocated
 * @author Rishab Arora
 * @author Jasem Mutlaq
 * @version 1.1
 **/
class KSUserDB {
 public:
    /** Initialize KStarsDB while running splash screen
     * @return true on success
     */
    bool Initialize();
    ~KSUserDB();

    QSqlDatabase GetDatabase();

    /************************************************************************
     ********************************* Drivers ******************************
     ************************************************************************/

    int AddProfile(const QString &name);

    bool DeleteProfile(ProfileInfo *pi);

    void SaveProfile(ProfileInfo *pi);

    /**
     * @brief GetAllProfiles Return all profiles in a QList
     * @return QMap with the keys as profile names and values are profile ids.
     */
    //QMap<int, QStringList> GetAllProfiles();
    QList<ProfileInfo *> GetAllProfiles();

    /************************************************************************
     ******************************* Observers ******************************
     ************************************************************************/

    /**
     * @brief Adds a new observer into the database
     **/
    void AddObserver(const QString &name, const QString &surname, const QString &contact);

     /* @brief Returns the unique id of the user with given name & surname
     *
     * @return int
     **/
    int FindObserver(const QString &name, const QString &surname);
    /**
     * @brief Removes the user with unique id as given by FindObserver
     * Returns false if the user is not found
     *
     * @return bool
     **/
    bool DeleteObserver(const QString &id);
    /**
     * @brief Updates the passed reference of observer_list with all observers
     * The original content of the list is cleared.
     *
     * @return void
     **/
    void GetAllObservers(QList<OAL::Observer *> &observer_list);

    /************************************************************************
     ********************************* Horizon ******************************
     ************************************************************************/

    // Jasem: Add API doc
    void DeleteAllHorizons();
    void AddHorizon(ArtificialHorizonEntity * horizon);
    QList<ArtificialHorizonEntity *> GetAllHorizons();

    /************************************************************************
     ********************************* Flags ********************************
     ************************************************************************/

    /**
     * @brief Erases all the flags from the database
     *
     * @return void
     **/
    void DeleteAllFlags();
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
    void AddFlag(const QString &ra, const QString &dec, const QString &epoch,
                 const QString &image_name, const QString &label, const QString &labelColor);
    /**
     * @brief Returns a QList populated with all stored flags
     * Order: const QString &ra, const QString &dec, const QString &epoch,
     *        const QString &imageName, const QString &label, const QString &labelColor
     * @return QList< QStringList >
     **/
    QList<QStringList> GetAllFlags();

    /************************************************************************
     ******************************* Equipment ******************************
     ************************************************************************/

    /**
     * @brief Erase the equipment with given type and unique id
     * Valid equipment types: "telescope","lens","filter"
     *
     * @param type Equipment type (same as table name)
     * @param id Unique id (same as row number)
     * @return void
     **/
    void DeleteEquipment(const QString &type, const int &id);
    /**
     * @brief Erases the whole equipment table of given type
     *
     * @param type Equipment type (same as table name)
     * @return void
     **/
    void DeleteAllEquipment(const QString &type);

    /************************************************************************
     ********************************** Scope *******************************
     ************************************************************************/

    /**
     * @brief Appends the scope with given details in the database
     *
     * @return void
     **/
    void AddScope(const QString &model, const QString &vendor, const QString &driver,
                  const QString &type, const double &focalLength, const double &aperture);
    /**
     * @brief Replaces the scope with given ID with provided content
     *
     * @return void
     **/
    void AddScope(const QString &model, const QString &vendor, const QString &driver,
                  const QString &type, const double &focalLength, const double &aperture,
                  const QString &id);
    /**
     * @brief updates the scope list with all scopes from database
     * List is cleared and then filled with content.
     *
     * @param m_scopeList Reference to list to be updated
     * @return void
     **/
    void GetAllScopes(QList<OAL::Scope *> &m_scopeList);

    /************************************************************************
     ******************************* Eye Piece ******************************
     ************************************************************************/

    /**
     * @brief Add new eyepiece to database
     *
     * @return void
     **/
    void AddEyepiece(const QString &vendor, const QString &model, const double &focalLength,
                     const double &fov, const QString &fovunit);
    /**
     * @brief Replace eyepiece at position (ID) with new content
     *
     * @return void
     **/
    void AddEyepiece(const QString &vendor, const QString &model, const double &focalLength,
                           const double &fov, const QString &fovunit, const QString &id);
    /**
     * @brief Populate the reference passed with all eyepieces
     *
     * @param m_eyepieceList Reference to list of eyepieces
     * @return void
     **/
    void GetAllEyepieces(QList<OAL::Eyepiece *> &m_eyepieceList);

    /************************************************************************
     ********************************** Lens ********************************
     ************************************************************************/

    /**
     * @brief Add a new lens to the database
     *
     * @return void
     **/
    void AddLens(const QString &vendor, const QString &model, const double &factor);
    /**
     * @brief Replace a lens at given ID with new content
     *
     * @return void
     **/
    void AddLens(const QString &vendor, const QString &model, const double &factor, const QString &id);
    /**
     * @brief Populate the reference passed with all lenses
     *
     * @param m_lensList Reference to list of lenses
     * @return void
     **/
    void GetAllLenses(QList<OAL::Lens *>& m_lensList);

    /************************************************************************
     ******************************** Filters *******************************
     ************************************************************************/

    /**
     * @brief Add a new filter to the database
     *
     * @return void
     **/
    void AddFilter(const QString &vendor, const QString &model, const QString &type, const QString &color);
    /**
     * @brief Replace a filter at given ID with new content
     *
     * @return void
     **/
    void AddFilter(const QString &vendor, const QString &model, const QString &type, const QString &color,
                   const QString &id);
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
    bool RebuildDB();
    /**
     * @brief Rebuilds the User DB from scratch using RebuildDB.
     * Also, loads any previous user data into the DB.
     *
     * @return bool
     **/    
    bool FirstRun();

#if 0
    /**
     * @brief Imports flags data from previous format
     *
     * @return bool
     **/    
    bool ImportFlags();
    /**
     * @brief Imports users from previous format
     *
     * @return bool
     **/    
    bool ImportUsers();
    /**
     * @brief Imports equipment from previous format
     *
     * @return bool
     **/    
    bool ImportEquipment();

#endif
    /**
     * @brief Helper functions
     *
     * @return void
     **/    
    void readScopes();
    void readScope();
    void readEyepieces();
    void readEyepiece();
    void readLenses();
    void readLens();
    void readFilters();
    void readFilter();

    void DeleteProfileDrivers(ProfileInfo *pi);
    void GetProfileDrivers(ProfileInfo *pi);
    //void GetProfileCustomDrivers(ProfileInfo *pi);


    /**
     * @brief Linked to the user database _once_.
     **/
    QSqlDatabase userdb_;
    /**
     * @brief XML reader for importing old formats
     **/
    QXmlStreamReader *reader_;
    /**
     * @brief Function to return the last error encountered by SQLite
     *
     * @return QSqlError
     **/
    inline QSqlError LastError();
};

#endif  // KSTARS_KSUSERDB_H
