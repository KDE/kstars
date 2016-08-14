/***************************************************************************
                          finddialoglite.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Jul 29 2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef FINDDIALOGLITE_H_
#define FINDDIALOGLITE_H_

#include <QKeyEvent>
#include <QDialog>

#include "skyobjects/skyobject.h"

class QTimer;
class QStringListModel;
class QSortFilterProxyModel;
class SkyObjectListModel;

/** @class FindDialogLite
 * A backend of dialog declared in QML.
 *
 * @short Backend for Find Object Dialog in QML
 * @author Artem Fedoskin, Jason Harris
 * @version 1.0
 */
class FindDialogLite : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList filterModel READ getFilterModel NOTIFY filterModelChanged)
public:
    explicit FindDialogLite();

    /** Destructor */
    virtual ~FindDialogLite();

    Q_INVOKABLE void selectObject(int index);

    QStringList getFilterModel() { return m_filterModel; }

    /** @short pre-filter the list of objects according to the
     * selected object type.
     */
    Q_INVOKABLE void filterByType(uint typeIndex);
signals:
    void filterModelChanged();

public slots:
    /**When Text is entered in the QLineEdit, filter the List of objects
     * so that only objects which start with the filter text are shown.
     */
    Q_INVOKABLE void filterList(QString searchQuery);

    //FIXME: Still valid for QDialog?  i.e., does QDialog have a slotOk() ?
    /**
     *Overloading the Standard QDialogBase slotOk() to show a "sorry" message
     *box if no object is selected when the user presses Ok.  The window is
     *not closed in this case.
     */
    void slotOk();

private slots:
    void enqueueSearch();

    void slotDetails();
private:

    /** @short Do some post processing on the search text to interpret what the user meant
     * This could include replacing text like "m93" with "m 93"
     */
    QString processSearchText(QString text);

    QStringList m_filterModel;
    SkyObjectListModel *fModel;
    QSortFilterProxyModel* m_sortModel;
    QTimer* timer;
    bool listFiltered;
};

#endif
