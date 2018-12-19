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

#pragma once

#include "ui_finddialog.h"

#include <QDialog>
#include <QKeyEvent>

class QTimer;
class QComboBox;
class QStringListModel;
class QSortFilterProxyModel;
class SkyObjectListModel;
class SkyObject;

class FindDialogUI : public QFrame, public Ui::FindDialog
{
    Q_OBJECT
  public:
    explicit FindDialogUI(QWidget *parent = nullptr);
};

/**
 * @class FindDialog
 * Dialog window for finding SkyObjects by name.  The dialog contains
 * a QListBox showing the list of named objects, a QLineEdit for filtering
 * the list by name, and a QCombobox for filtering the list by object type.
 *
 * 2018-12 JM: The dialog is a singleton since we need a single instance in KStars.
 * @short Find Object Dialog
 * @author Jason Harris
 * @author Jasem Mutlaq
 * @version 1.1
 */
class FindDialog : public QDialog
{
    Q_OBJECT
  public:
    static FindDialog *Instance();

    /**
     * @return the target object (need not be the same as currently selected object!)
     *
     * @note Avoid using selectedObject()
     */
    inline SkyObject *targetObject() { return m_targetObject; }

  public slots:
    /**
     * When Text is entered in the QLineEdit, filter the List of objects
     * so that only objects which start with the filter text are shown.
     */
    void filterList();

    // FIXME: Still valid for QDialog?  i.e., does QDialog have a slotOk() ?
    /**
     * Overloading the Standard QDialogBase slotOk() to show a "sorry"
     * message box if no object is selected and internet resolution was
     * disabled/failed when the user presses Ok.  The window is not
     * closed in this case.
     */
    void slotOk();

    /**
     * @short This slot resolves the object on the internet, ignoring the selection on the list
     */
    void slotResolve();

  private slots:
    /** Init object list after opening dialog. */
    void init();

    /** Set the selected item to the first item in the list */
    void initSelection();

    void enqueueSearch();

    void slotDetails();

  protected:
    /**
     * Process Keystrokes.  The Up and Down arrow keys are used to select the
     * Previous/Next item in the listbox of named objects.  The Esc key closes
     * the window with no selection, using reject().
     * @param e The QKeyEvent pointer
     */
    void keyPressEvent(QKeyEvent *e) override;

    void showEvent(QShowEvent *e) override;

    /** @return the currently-selected item from the listbox of named objects */
    SkyObject *selectedObject() const;

  private:
    /**
     * Constructor. Creates all widgets and packs them in QLayouts. Connects
     * Signals and Slots. Runs initObjectList().
     */
    explicit FindDialog(QWidget *parent = nullptr);

    static FindDialog *m_Instance;
    /**
     * @short Do some post processing on the search text to interpret what the user meant
     * This could include replacing text like "m93" with "m 93"
     */
    QString processSearchText();

    /** @short Finishes the processing towards closing the dialog initiated by slotOk() or slotResolve() */
    void finishProcessing(SkyObject *selObj = nullptr, bool resolve = true);

    /** @short pre-filter the list of objects according to the selected object type. */
    void filterByType();

    FindDialogUI *ui { nullptr };
    SkyObjectListModel *fModel { nullptr };
    QSortFilterProxyModel *sortModel { nullptr };
    QTimer *timer { nullptr };
    bool listFiltered { false };
    QPushButton *okB { nullptr };
    SkyObject *m_targetObject { nullptr };

    // History
    QComboBox *m_HistoryCombo { nullptr};
    QList<SkyObject*> m_HistoryList;
};

