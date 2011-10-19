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
#include <QModelIndex>
#include <kdialog.h>

#include "kstars.h"
#include "geolocation.h"
#include "oal.h"
#include "session.h"
#include "skyobjects/skyobject.h"

class KStars;
class QStandardItemModel;
class QStringListModel;

class Execute : public KDialog
{
    Q_OBJECT

public:
    /**
      * \brief Enumeration of dialog pages.
      */
    enum DIALOG_PAGES
    {
        SESSION_PAGE = 0,
        TARGET_PAGE = 1,
        OBSERVATION_PAGE = 2
    };

    /**
      * \brief Constructor.
      */
    Execute();

    /**
      * \brief Initialize basic widgets etc.
      */
    void init();

    /**
      * \brief Show observation represented by passed pointer.
      * \param observation Pointer to observation which should be viewed.
      */
    void showObservation(OAL::Observation *observation);

    /**
      * \brief Show target represented by passed pointer.
      * \param target Pointer to observation target which should be viewed.
      */
    void showTarget(OAL::ObservationTarget *target);

    /**
      * \brief Show session represented by passed pointer.
      * \param session Pointer to session which should be viewed.
      */
    void showSession(OAL::Session *session);

private slots:
    void slotSetTargetOrObservation(QModelIndex idx);
    void slotSetSession(int idx);
    void slotObserverManager();
    void slotAddObject();
    void slotShowSession();
    void slotShowTargets();

    /**
      * \brief Open the location dialog for setting of current location.
      */
    void slotLocation();

    /**
      * \brief Save log to file in OAL 2.0 format.
      */
    void slotSaveLog();

private:
    /**
      * \brief Load sessions into sessions list.
      */
    void loadSessions();

    /**
      * \brief Load targets and observations from log object into tree's model.
      */
    void loadTargets();

    KStars *m_Ks;
    Ui::Execute m_Ui;
    OAL::Log *m_LogObject;
    GeoLocation *m_CurrentLocation;
    QStandardItemModel *m_ObservationsModel;
    QStringListModel *m_TargetAliasesModel;
};

#endif
