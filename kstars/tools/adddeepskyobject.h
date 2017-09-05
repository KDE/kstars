/***************************************************************************
                adddeepskyobject.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed 17 Aug 2016 20:23:05 CDT
    copyright            : (c) 2016 by Akarsh Simha
    email                : akarsh.simha@kdemail.net
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#pragma once

#include "ui_adddeepskyobject.h"

class QString;

class SyncedCatalogComponent;

/**
 * @class AddDeepSkyObject
 * @short Allows the user to add an object to a @sa SyncedCatalogComponent
 *
 * @author Akarsh Simha <akarsh@kde.org>
 */
class AddDeepSkyObject : public QDialog, public Ui::AddDeepSkyObject
{
    Q_OBJECT;

  public:
    /** @short Constructor */
    AddDeepSkyObject(QWidget *parent, SyncedCatalogComponent *catalog);

    /** @short Destructor */
    ~AddDeepSkyObject() override;

    /** @short Fills the dialog from a text by trying to guess fields */
    void fillFromText(const QString &text);

  public slots:

    /** @short Accept the dialog and add the entry to the catalog */
    bool slotOk();

    /** @short Resets the entries in the dialog */
    void resetView();

    /** @short Gathers the text and calls fillFromText() to parse the text */
    void slotFillFromText();

  private:
    SyncedCatalogComponent *m_catalog { nullptr };
    Ui::AddDeepSkyObject *ui { nullptr };
};
