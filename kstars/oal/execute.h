/***************************************************************************
                          execute.h  -  description

                             -------------------
    begin                : Friday July 21, 2009
    copyright            : (C) 2009 by Prakash Mohan
    email                : prakash.mohan*@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef EXECUTE_H_
#define EXECUTE_H_

#include "ui_execute.h"

#include <QWidget>
#include <QDialog>

#include "kstars.h"
#include "geolocation.h"
#include "oal/oal.h"
#include "oal/session.h"
#include "skyobjects/skyobject.h"

class KStars;

/**
 * @class Execute
 *
 * Executes an observation session.
 */
class Execute : public QDialog
{
    Q_OBJECT
  public:
    /** @short Default constructor
         */
    Execute();

    /** @short This initializes the combo boxes, and sets up the
         * dateTime and geolocation from the OL
         */
    void init();

  public slots:
    /** @short Function to handle the UI when the 'next' button is pressed
         * This calls the corresponding functions based on the currentIndex
         */
    void slotNext();

    /*Function to Save the session details*/
    bool saveSession();

    /** @short Function to save the user notes set for
         * the current object in the target combo box
         */
    void addTargetNotes();

    //
    void slotObserverAdd();
    //

    /** @short Function to add the current observation to the observation list
         */
    bool addObservation();

    /** @short Function to handle the state of current observation,
         * and hiding the execute window
         */
    void slotEndSession();

    /** @short Opens the location dialog for setting the current location
         */
    void slotLocation();

    /** @short Loads the sessionlist from the OL
         * into the target combo box
         */
    void loadTargets();

    /** @short Sorts the target list using the scheduled time
         */
    void sortTargetList();

    /** @short set the currentTarget when the user selection
         * is changed in the target combo box
         */
    void slotSetTarget(QString name);

    /** @short loads the equipment list from
         * the global logObject into the comboBoxes
         */
    void loadEquipment();

    /** @short loads the observer list from
         * the global logObject into the comboBoxes
         */
    void loadObservers();

    /** @short loads the observation edit page
         */
    void loadObservationTab();

    /** @short get object name. If star has no name, generate a name based on catalog number.
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
    OAL::Session *currentSession;
    OAL::Log *logObject;
    OAL::Observer *currentObserver;
    OAL::Scope *currentScope;
    OAL::Eyepiece *currentEyepiece;
    OAL::Lens *currentLens;
    OAL::Filter *currentFilter;
    GeoLocation *geo;
    SkyObject *currentTarget;
    int nextSession, nextObservation, nextSite;
};

#endif
