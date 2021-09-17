/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_observeradd.h"

#include <QDialog>

/**
 * @class ObserverAdd
 *
 * Dialog to add new observers.
 */
class ObserverAdd : public QDialog
{
    Q_OBJECT
  public:
    /** The default constructor */
    ObserverAdd();

    /** @short function to load the list of observers from the file */
    void loadObservers();

  public slots:
    /**
     * @short function to add the new observer
     * to the observerList of the global logObject
     */
    void auxSlot();
    void checkObserverInfo();
    void slotAddObserver();
    void slotRemoveObserver();
    void slotUpdateModel();

  private:
    Ui::ObserverAdd ui;
};
