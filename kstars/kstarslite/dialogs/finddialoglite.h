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

#include "skyobjects/skyobject.h"

class QTimer;
class QStringListModel;
class QSortFilterProxyModel;
class SkyObjectListModel;

/** @class FindDialogLite
 * @short Backend for "Find Object" dialog in QML
 * @author Artem Fedoskin, Jason Harris
 * @version 1.0
 */
class FindDialogLite : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList filterModel READ getFilterModel NOTIFY filterModelChanged)
    //true if m_searchQuery is already in sorted list of object names
    Q_PROPERTY(bool isResolveEnabled READ getIsResolveEnabled WRITE setIsResolveEnabled NOTIFY isResolveEnabledChanged)
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

    /**
     * @short searches for the object in internet (adopted to KStars Lite FindDialog::finishProcessing()
     */
    Q_INVOKABLE void resolveInInternet(QString searchQuery);

    /**
     * @return true it at least one entry in fModel is an exact match with searchQuery
     */
    Q_INVOKABLE bool isInList(QString searchQuery);

    /** Getter for queryInList **/
    bool getIsResolveEnabled() { return m_isResolveEnabled; }

    /** Setter for queryInList **/
    void setIsResolveEnabled(bool isResolveEnabled);
signals:
    void filterModelChanged();
    void notifyMessage(QString message);
    void isResolveEnabledChanged(bool);

public slots:
    /**When Text is entered in the QLineEdit, filter the List of objects
     * so that only objects which start with the filter text are shown.
     */
    Q_INVOKABLE void filterList(QString searchQuery);

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

    /** Current query that is used to sort fModel **/
    QString m_searchQuery;
    bool m_isResolveEnabled;

    uint m_typeIndex;
};

#endif
