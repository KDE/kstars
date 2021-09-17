/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QStringList>

class QSortFilterProxyModel;
class QStringListModel;
class QTimer;

class SkyObjectListModel;

/**
 * @class FindDialogLite
 * @short Backend for "Find Object" dialog in QML
 * The way we are searching for the object is as follows:
 * Each SkyComponent in addition to QStringList of names holds QVector<QPair<QString, const SkyObject *>>.
 * SkyObjectListModel is a class that holds SkyObjects together with their names (name and longname).
 * Whenever user searches for an object we sort list of SkyObjects through QSortFilterProxyModel. The reason
 * for this way of searching is that we don't need to search for an object again, as it was done previously.
 * Instead of this, user directly selects the object in search results.
 *
 * @author Artem Fedoskin, Jason Harris
 * @version 1.0
 */
class FindDialogLite : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QStringList filterModel READ getFilterModel NOTIFY filterModelChanged)
    //true if m_searchQuery is already in sorted list of object names
    Q_PROPERTY(bool isResolveEnabled READ getIsResolveEnabled WRITE setIsResolveEnabled NOTIFY isResolveEnabledChanged)
  public:
    /**
     * @short Constructor. Initialize m_filterModel with object types and
     * initialize m_sortModel with instance of SkyObjectListModel
     */
    FindDialogLite();
    virtual ~FindDialogLite();

    /** Open context menu for object with given index from m_sortModel */
    Q_INVOKABLE void selectObject(int index);

    /**
     * @return list of object types
     */
    QStringList getFilterModel() { return m_filterModel; }

    /**
     * @short pre-filter the list of objects according to the selected object type.
     */
    Q_INVOKABLE void filterByType(uint typeIndex);

    /**
     * @short searches for the object in internet (adopted to KStars Lite version of
     * FindDialog::finishProcessing()
     */
    Q_INVOKABLE void resolveInInternet(QString searchQuery);

    /**
     * @return true it at least one entry in fModel is an exact match with searchQuery
     */
    Q_INVOKABLE bool isInList(QString searchQuery);

    /** Getter for isResolveEnabled **/
    bool getIsResolveEnabled() { return m_isResolveEnabled; }

    /** Setter for isResolveEnabled **/
    void setIsResolveEnabled(bool isResolveEnabled);
  signals:
    void filterModelChanged();
    void notifyMessage(QString message);
    void isResolveEnabledChanged(bool);

  public slots:
    /**
     * When Text is entered in the QLineEdit, filter the List of objects
     * so that only objects which start with the filter text are shown.
     */
    Q_INVOKABLE void filterList(QString searchQuery);

  private:
    /**
     * @short Do some post processing on the search text to interpret what the user meant
     * This could include replacing text like "m93" with "m 93"
     */
    QString processSearchText(QString text);

    QStringList m_filterModel;
    SkyObjectListModel *fModel { nullptr };
    QSortFilterProxyModel *m_sortModel { nullptr };
    QTimer *timer { nullptr };
    bool listFiltered { false };

    /** Current query that is used to sort fModel **/
    QString m_searchQuery;
    bool m_isResolveEnabled { false };
    uint m_typeIndex { 0 };
};
