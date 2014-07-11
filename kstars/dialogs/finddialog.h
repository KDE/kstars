/***************************************************************************
                          finddialog.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Jul 4 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef FINDDIALOG_H_
#define FINDDIALOG_H_

#include <QKeyEvent>
#include <QDialog>

#include "ui_finddialog.h"
#include "skyobjects/skyobject.h"

class QTimer;
class QStringListModel;
class QSortFilterProxyModel;

class FindDialogUI : public QFrame, public Ui::FindDialog {
    Q_OBJECT
public:
    explicit FindDialogUI( QWidget *parent=0 );
};

/**@class FindDialog
 * Dialog window for finding SkyObjects by name.  The dialog contains
 * a QListBox showing the list of named objects, a QLineEdit for filtering
 * the list by name, and a QCombobox for filtering the list by object type.
 *
 * @short Find Object Dialog
 * @author Jason Harris
 * @version 1.0
 */
class FindDialog : public QDialog {
    Q_OBJECT
public:
    /**Constructor. Creates all widgets and packs them in QLayouts.  Connects
     * Signals and Slots.  Runs initObjectList().
     */
    explicit FindDialog( QWidget* parent = 0 );

    /** Destructor */
    virtual ~FindDialog();

    /** @return the currently-selected item from the listbox of named objects */
    SkyObject* selectedObject() const;

public slots:
    /**When Text is entered in the QLineEdit, filter the List of objects
     * so that only objects which start with the filter text are shown.
     */
    void filterList();

    //FIXME: Still valid for QDialog?  i.e., does QDialog have a slotOk() ?
    /**
     *Overloading the Standard QDialogBase slotOk() to show a "sorry" message
     *box if no object is selected when the user presses Ok.  The window is
     *not closed in this case.
     */
    void slotOk();

private slots:
    /** Init object list after opening dialog. */
    void init();

    /** Set the selected item to the first item in the list */
    void initSelection();

    void enqueueSearch();

    void slotDetails();

protected:
    /**Process Keystrokes.  The Up and Down arrow keys are used to select the
     * Previous/Next item in the listbox of named objects.  The Esc key closes
     * the window with no selection, using reject().
     * @param e The QKeyEvent pointer
     */
    void keyPressEvent( QKeyEvent *e );

private:

    /**@short Do some post processing on the search text to interpret what the user meant
     * This could include replacing text like "m93" with "m 93"
     */
     QString processSearchText();

    /**@short pre-filter the list of objects according to the
     * selected object type.
     */
    void filterByType();

    FindDialogUI* ui;
    QStringListModel *fModel;
    QSortFilterProxyModel* sortModel;
    QTimer* timer;
    bool listFiltered;
};

#endif
