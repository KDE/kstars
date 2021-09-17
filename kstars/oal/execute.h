/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan*@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_execute.h"
#include "oal/oal.h"
#include "oal/session.h"
#include "skyobjects/skyobject.h"

#include <QDialog>

class GeoLocation;
class SkyObject;

/**
 * @class Execute
 *
 * Executes an observation session.
 */
class Execute : public QDialog
{
    Q_OBJECT

  public:
    /** @short Default constructor */
    Execute();

    /**
     * @short This initializes the combo boxes, and sets up the
     * dateTime and geolocation from the OL
     */
    void init();

  public slots:
    /**
     * @short Function to handle the UI when the 'next' button is pressed
     * This calls the corresponding functions based on the currentIndex
     */
    void slotNext();

    /** Function to Save the session details */
    bool saveSession();

    /**
     * @short Function to save the user notes set for the current object in the target combo box
     */
    void addTargetNotes();

    void slotObserverAdd();

    /** @short Function to add the current observation to the observation list */
    bool addObservation();

    /**
     * @short Function to handle the state of current observation, and hiding the execute window
     */
    void slotEndSession();

    /** @short Opens the location dialog for setting the current location */
    void slotLocation();

    /** @short Loads the sessionlist from the OL into the target combo box */
    void loadTargets();

    /** @short Sorts the target list using the scheduled time */
    void sortTargetList();

    /**
     * @short set the currentTarget when the user selection is changed in the target combo box
     */
    void slotSetTarget(const QString &name);

    /** @short loads the equipment list from the global logObject into the comboBoxes */
    void loadEquipment();

    /** @short loads the observer list from the global logObject into the comboBoxes */
    void loadObservers();

    /** @short loads the observation edit page
         */
    void loadObservationTab();

    /**
     * @short get object name. If star has no name, generate a name based on catalog number.
     * @param o sky object
     * @param translated set to true if the translated name is required.
     */
    QString getObjectName(const SkyObject *o, bool translated = true);

    void selectNextTarget();

    void loadCurrentItems();

    void slotSetCurrentObjects();

    void slotSlew();

    void slotShowSession();

    void slotShowTargets();

    int findIndexOfTarget(QString);

    void slotAddObject();

    void slotRemoveObject();

  private:
    Ui::Execute ui;
    OAL::Session *currentSession { nullptr };
    OAL::Log *logObject { nullptr };
    OAL::Observer *currentObserver { nullptr };
    OAL::Scope *currentScope { nullptr };
    OAL::Eyepiece *currentEyepiece { nullptr };
    OAL::Lens *currentLens { nullptr };
    OAL::Filter *currentFilter { nullptr };
    GeoLocation *geo { nullptr };
    SkyObject *currentTarget { nullptr };
    int nextSession { 0 };
    int nextObservation { 0 };
    int nextSite { 0 };
};
