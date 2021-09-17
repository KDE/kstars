/*
    SPDX-FileCopyrightText: 2008 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QStringList>

/**
 * @class CultureList
 * A list of all cultures
 * FIXME: Why not use a QStringList?
 */
class CultureList
{
  public:
    /** @short Create culture list and load its content from file */
    CultureList();

    /** @short Return the current sky culture */
    QString current() const { return m_CurrentCulture; }

    /** @short Set the current culture name */
    void setCurrent(QString newName);

    /** @short Return a sorted list of cultures */
    QStringList getNames() const { return m_CultureList; }

    /**
     * @short Return the name of the culture at index.
     * @return null string if is index is out of range
     * */
    QString getName(int index) const;

  private:
    QString m_CurrentCulture;
    // List of all available cultures. It's assumed that list is sorted.
    QStringList m_CultureList;
};
