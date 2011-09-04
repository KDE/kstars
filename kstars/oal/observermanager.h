/***************************************************************************
                          observermanager.h  -  K Desktop Planetarium

                             -------------------
    begin                : Sunday August 14, 2011
    copyright            : (C) 2011 by Lukasz Jaskiewicz
    email                : lucas.jaskiewicz@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#ifndef OBSERVERMANAGER_H
#define OBSERVERMANAGER_H

#include "ui_observermanager.h"

class KStars;
class QStandardItemModel;

/**
  * Observer Manager UI
  * \author Lukasz Jaskiewicz
  */
class ObserverManagerUi : public QFrame, public Ui::ObserverManager
{
    Q_OBJECT

public:
    explicit ObserverManagerUi(QWidget *parent = 0);
};

/**
  * Dialog used for managing observers (creating/editing/deleting).
  * \author Lukasz Jaskiewicz
  */
class ObserverManager : public KDialog
{
    Q_OBJECT
public:
    /**
      * Constructor.
      * \param parent Pointer to parent QWidget.
      */
    ObserverManager(QWidget *parent = 0);

    /**
      * Shows or hides column selecting coobservers.
      * \param show Show column for selection of coobservers?
      */
    void showEnableColumn(bool show, const QString &session = QString());

    /**
      * Load observers from observerlist.xml file.
      */
    void loadFromFile();

    /**
      * Save current observers to observerlist.xml file.
      */
    void saveToFile();

public slots:
    /**
      * Add observer.
      */
    void addObserver();

    /**
      * Save changes to observer.
      */
    void saveChanges();

    /**
      * Delete highlighted observer.
      */
    void deleteObserver();

    /**
      * Show observer (by model index).
      */
    void showObserver(QModelIndex idx);

    /**
      *
      */
    void setCoobservers();

private:
    void createModel();
    void saveChangesToObsList();

    ObserverManagerUi *mUi;
    QString mCurrentSession;

    KStars *mKstars;
    QStandardItemModel *mModel;
};

#endif // OBSERVERMANAGER_H
