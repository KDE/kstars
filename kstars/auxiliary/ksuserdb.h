/*
    SPDX-FileCopyrightText: 2012 Rishab Arora <ra.rishab@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "auxiliary/profileinfo.h"
#include "skymapview.h"
#ifndef KSTARS_LITE
#include "oal/oal.h"
#endif
#include <oal/filter.h>

#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QStringList>
#include <QVariantMap>
#include <QXmlStreamReader>

#include <memory>

class LineList;
class ArtificialHorizonEntity;
class ImageOverlay;
class ImagingPlannerDBEntry;


/**
 * @brief Single class to delegate all User database I/O
 *
 * usage: Call QSqlDatabase::removeDatabase("userdb"); after the object
 * of this class is deallocated
 * @author Rishab Arora
 * @author Jasem Mutlaq
 * @version 1.2
 **/
// cppcheck-suppress noConstructor
class KSUserDB
{
    public:
        ~KSUserDB();

        /**
         * Initialize KStarsDB while running splash screen
         * @return true on success
         */
        bool Initialize();

        const QString &connectionName() const
        {
            return m_ConnectionName;
        }

        /************************************************************************
         ********************************* Drivers ******************************
         ************************************************************************/

        int AddProfile(const QString &name);

        bool DeleteProfile(const QSharedPointer<ProfileInfo> &pi);

        // Delete profile and all related settings.
        bool PurgeProfile(const QSharedPointer<ProfileInfo> &pi);

        bool SaveProfile(const QSharedPointer<ProfileInfo> &pi);

        /**
         * @brief GetAllProfiles Return all profiles in a QList
         * @return QMap with the keys as profile names and values are profile ids.
         */
        bool GetAllProfiles(QList<QSharedPointer<ProfileInfo> > &profiles);

        /************************************************************************
         ******************************* Dark Library****************************
         ************************************************************************/

        bool AddDarkFrame(const QVariantMap &oneFrame);
        bool UpdateDarkFrame(const QVariantMap &oneFrame);
        bool DeleteDarkFrame(const QString &filename);
        bool GetAllDarkFrames(QList<QVariantMap> &darkFrames);

        /************************************************************************
         ******************************* Effective FOVs *************************
         ************************************************************************/

        bool AddEffectiveFOV(const QVariantMap &oneFOV);
        bool DeleteEffectiveFOV(const QString &id);
        bool GetAllEffectiveFOVs(QList<QVariantMap> &effectiveFOVs);

        /************************************************************************
         ******************************* Driver Alias *************************
         ************************************************************************/

        bool AddCustomDriver(const QVariantMap &oneDriver);
        bool DeleteCustomDriver(const QString &id);
        bool GetAllCustomDrivers(QList<QVariantMap> &CustomDrivers);

        /************************************************************************
         *********************************** HiPS *******************************
         ************************************************************************/

        bool AddHIPSSource(const QMap<QString, QString> &oneSource);
        bool DeleteHIPSSource(const QString &ID);
        bool GetAllHIPSSources(QList<QMap<QString, QString>> &HIPSSources);

        /************************************************************************
         *********************************** DSLR *******************************
         ************************************************************************/

        bool AddDSLRInfo(const QMap<QString, QVariant> &oneInfo);
        bool DeleteDSLRInfo(const QString &model);
        bool DeleteAllDSLRInfo();
        bool GetAllDSLRInfos(QList<QMap<QString, QVariant>> &DSLRInfos);

        /************************************************************************
         ******************************* Observers ******************************
         ************************************************************************/

        /** @brief Adds a new observer into the database **/
        bool AddObserver(const QString &name, const QString &surname, const QString &contact);

        /**
         * @brief Returns the unique id of the user with given name & surname
         *
         * @return true if found, false otherwise
         **/
        bool FindObserver(const QString &name, const QString &surname);
        /**
         * @brief Removes the user with unique id as given by FindObserver
         * Returns false if the user is not found
         *
         * @return bool
         **/
        bool DeleteObserver(const QString &id);

#ifndef KSTARS_LITE
        /**
         * @brief Updates the passed reference of observer_list with all observers
         * The original content of the list is cleared.
         *
         * @return true if database read was successfull, false otherwise.
         **/
        bool GetAllObservers(QList<OAL::Observer *> &observer_list);
#endif
        /************************************************************************
         ********************************* Horizon ******************************
         ************************************************************************/

        /** @brief Deletes all artificial horizon rows from the database **/
        bool DeleteAllHorizons();

        /** @brief Adds a new artificial horizon row into the database **/
        bool AddHorizon(ArtificialHorizonEntity *horizon);

        /** @brief Gets all the artificial horizon rows from the database **/
        bool GetAllHorizons(QList<ArtificialHorizonEntity *> &horizonList);

        /************************************************************************
         ****************************** ImageOverlay ****************************
         ************************************************************************/

        /** @brief Deletes all image overlay rows from the database **/
        bool DeleteAllImageOverlays();

        /** @brief Adds a new image overlay row into the database **/
        bool AddImageOverlay(const ImageOverlay &overlay);

        /** @brief Gets all the image overlay rows from the database **/
        bool GetAllImageOverlays(QList<ImageOverlay> *imageOverlayList);

        /************************************************************************
         **************************** Imaging Planner ***************************
         ************************************************************************/

        /** @brief Deletes all Imaging Planner rows from the database **/
        bool DeleteAllImagingPlannerEntries();

        /** @brief Adds a new Imaging Planner row into the database **/
        bool AddImagingPlannerEntry(const ImagingPlannerDBEntry &entry);

        /** @brief Gets all the Imaging Planner rows from the database **/
        bool GetAllImagingPlannerEntries(QList<ImagingPlannerDBEntry> *entryList);

        /************************************************************************
         ****************************** Sky Map Views ***************************
         ************************************************************************/

        /** @brief Deletes all the sky map views stored in the database */
        bool DeleteAllSkyMapViews();

        /** @brief Adds a new sky map view row in the database */
        bool AddSkyMapView(const SkyMapView &view);

        /** @brief Gets all the sky map view rows from the database */
        bool GetAllSkyMapViews(QList<SkyMapView> &skyMapViewList);

        /************************************************************************
         ********************************* Flags ********************************
         ************************************************************************/

        /**
         * @brief Erases all the flags from the database
         *
         * @return void
         **/
        bool DeleteAllFlags();

        /**
         * @brief Add a new Flag with given parameters
         *
         * @param ra Right Ascension
         * @param dec Declination
         * @param epoch Epoch
         * @param image_name Name of the image used
         * @param label Content of display label on screen
         * @param labelColor Color of the label (name or hex code) eg #00FF00
         * @return True if database transaction is successful, false otherwise
         **/
        bool AddFlag(const QString &ra, const QString &dec, const QString &epoch, const QString &image_name,
                     const QString &label, const QString &labelColor);
        /**
         * @brief Returns a QList populated with all stored flags
         * Order: const QString &ra, const QString &dec, const QString &epoch,
         *        const QString &imageName, const QString &label, const QString &labelColor
         * @return
         **/
        bool GetAllFlags(QList<QStringList> &flagList);

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
        bool DeleteEquipment(const QString &type, const QString &id);
        /**
         * @brief Erases the whole equipment table of given type
         *
         * @param type Equipment type (same as table name)
         * @return void
         **/
        bool DeleteAllEquipment(const QString &type);

        /************************************************************************
         ********************************** Scope *******************************
         ************************************************************************/

        /**
         * @brief Appends the scope with given details in the database
         *
         * @return void
         **/
        bool AddScope(const QString &model, const QString &vendor, const QString &type,
                      const double &aperture, const double &focalLength);
        /**
         * @brief Replaces the scope with given ID with provided content
         *
         * @return void
         **/
        bool AddScope(const QString &model, const QString &vendor, const QString &type, const double &aperture,
                      const double &focalLength, const QString &id);
#ifndef KSTARS_LITE
        /**
         * @brief updates the scope list with all scopes from database
         * List is cleared and then filled with content.
         *
         * @param m_scopeList Reference to list to be updated
         * @return void
         **/
        bool GetAllScopes(QList<OAL::Scope *> &m_scopeList);
#endif

        /************************************************************************
         ******************************* Optical Elements ***********************
         ************************************************************************/
        bool getOpticalElementByID(int id, QJsonObject &element);
        bool getOpticalElementByName(const QString &name, QJsonObject &element);
        /**
         * @brief getLastOpticalElement Return last inserted scope or lens
         * @param element JSON object to fill with scope or lens metadata
         * @return True if found, false if none found.
         */
        bool getLastOpticalElement(QJsonObject &element);
        QStringList getOpticalElementNames();

        /************************************************************************
         ******************************* Eye Piece ******************************
         ************************************************************************/

        /**
         * @brief Add new eyepiece to database
         *
         * @return void
         **/
        bool AddEyepiece(const QString &vendor, const QString &model, const double &focalLength, const double &fov,
                         const QString &fovunit);
        /**
         * @brief Replace eyepiece at position (ID) with new content
         *
         * @return void
         **/
        bool AddEyepiece(const QString &vendor, const QString &model, const double &focalLength, const double &fov,
                         const QString &fovunit, const QString &id);
#ifndef KSTARS_LITE
        /**
         * @brief Populate the reference passed with all eyepieces
         *
         * @param m_eyepieceList Reference to list of eyepieces
         * @return void
         **/
        bool GetAllEyepieces(QList<OAL::Eyepiece *> &m_eyepieceList);
#endif
        /************************************************************************
         ********************************** Lens ********************************
         ************************************************************************/

        /**
         * @brief Add a new lens to the database
         *
         * @return void
         **/
        bool AddLens(const QString &vendor, const QString &model, const double &factor);
        /**
         * @brief Replace a lens at given ID with new content
         *
         * @return void
         **/
        bool AddLens(const QString &vendor, const QString &model, const double &factor, const QString &id);
#ifndef KSTARS_LITE
        /**
         * @brief Populate the reference passed with all lenses
         *
         * @param m_lensList Reference to list of lenses
         * @return void
         **/
        bool GetAllLenses(QList<OAL::Lens *> &m_lensList);
#endif

        /************************************************************************
         ********************************** DSLR Lens ***************************
         ************************************************************************/

        /**
         * @brief Appends the DSLR lens with given details in the database
         *
         * @return void
         **/
        bool AddDSLRLens(const QString &model, const QString &vendor, const double focalLength, const double focalRatio);
        /**
         * @brief Replaces the scope with given ID with provided content
         *
         * @return void
         **/
        bool AddDSLRLens(const QString &model, const QString &vendor, const double focalLength, const double focalRatio,
                         const QString &id);
#ifndef KSTARS_LITE
        /**
         * @brief updates the dslr list with all DSLR lenses from database
         * List is cleared and then filled with content.
         *
         * @param dslrlens_list Reference to list to be updated
         * @return void
         **/
        bool GetAllDSLRLenses(QList<OAL::DSLRLens *> &dslrlens_list);
#endif

        /************************************************************************
         ******************************** Filters *******************************
         ************************************************************************/

        /**
         * @brief Add a new filter to the database
         *
         * @return void
         **/
        bool AddFilter(const filterProperties *fp);
        /**
         * @brief Replace a filter at given ID with new content
         *
         * @return void
         **/
        bool AddFilter(const filterProperties *fp, const QString &id);
        /**
         * @brief Populate the reference passed with all filters
         *
         * @param m_filterList Reference to list of filters
         * @return void
         **/
        bool GetAllFilters(QList<OAL::Filter *> &m_filterList);

        /************************************************************************
         ******************************** Optical Trains ************************
         ************************************************************************/

        /**
         * @brief Add a new optical train to the database
         * @param oneTrain optical train data
         **/
        bool AddOpticalTrain(const QVariantMap &oneTrain);

        /**
         * @brief Update an existing optical train
         * @param oneTrain optical train data
         * @param id ID of train to replace in database
         **/
        bool UpdateOpticalTrain(const QVariantMap &oneTrain, int id);

        bool DeleteOpticalTrain(int id);
        /**
         * @brief Populate the reference passed with all optical trains
         * @param opticalTrains Reference to all trains list
         **/
        bool GetOpticalTrains(uint32_t profileID, QList<QVariantMap> &opticalTrains);

        /************************************************************************
         ******************************** Profile Settings **********************
         ************************************************************************/

        /**
         * @brief Add new profile settings to the database
         * @param settings JSON settings
         **/
        void AddProfileSettings(uint32_t profile, const QByteArray &settings);

        void UpdateProfileSettings(uint32_t profileID, const QByteArray &settings);

        void DeleteProfileSettings(uint32_t profile);
        /**
         * @brief Populate the reference passed with settings for one paritcular profile
         * @param profile id of profile
         * @param settings populate settings with parsed profile settings.
         **/
        bool GetProfileSettings(uint32_t profile, QVariantMap &settings);

        /************************************************************************
         ******************************** Train Settings **********************
         ************************************************************************/

        /**
         * @brief Add new Train settings to the database
         * @param settings JSON settings
         **/
        bool AddOpticalTrainSettings(uint32_t train, const QByteArray &settings);

        bool UpdateOpticalTrainSettings(uint32_t train, const QByteArray &settings);

        bool DeleteOpticalTrainSettings(uint32_t train);
        /**
         * @brief Populate the reference passed with settings for one paritcular Train
         * @param TrainID id of Train
         * @param settings populate settings with parsed Train settings.
         **/
        bool GetOpticalTrainSettings(uint32_t train, QVariantMap &settings);

        /************************************************************************
         *********************** Collimation Overlay Elements *******************
         ************************************************************************/

        /**
         * @brief Add a new collimation overlay element to the database
         * @param oneElement collimation overlay element data
         **/
        bool AddCollimationOverlayElement(const QVariantMap &oneElement);

        /**
         * @brief Update an existing collimation overlay element
         * @param oneElement collimation overlay element data
         * @param id ID of element to replace in database
         **/
        bool UpdateCollimationOverlayElement(const QVariantMap &oneElement, int id);

        bool DeleteCollimationOverlayElement(int id);

        /**
         * @brief Populate the reference passed with all collimation overlay elements
         * @param collimationOverlayElements Reference to all elements list
         **/
        bool GetCollimationOverlayElements(QList<QVariantMap> &collimationOverlayElements);

    private:
        /**
         * @brief This function initializes a new database in the user's directory.
         * To be run only when a new db is needed. Should not be run over existing database file.
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

        /** @brief creates the image overlay table if it doesn't already exist **/
        void CreateImageOverlayTableIfNecessary();

        /** @brief creates the imaging planner table if it doesn't already exist **/
        void CreateImagingPlannerTableIfNecessary();

        /** @brief creates the image overlay table if it doesn't already exist **/
        void CreateSkyMapViewTableIfNecessary();

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
        // Helper functions
        void readScopes();
        void readScope();
        void readEyepieces();
        void readEyepiece();
        void readLenses();
        void readLens();
        void readFilters();
        void readFilter();

        bool DeleteProfileDrivers(const QSharedPointer<ProfileInfo> &pi);
        bool GetProfileDrivers(const QSharedPointer<ProfileInfo> &pi);
        //void GetProfileCustomDrivers(ProfileInfo *pi);

        /** XML reader for importing old formats **/
        QXmlStreamReader *reader_ { nullptr };

        QString m_ConnectionName;

        static const uint16_t SCHEMA_VERSION = 315;
};
