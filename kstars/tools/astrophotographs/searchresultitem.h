/***************************************************************************
                          astrophotographsbrowser.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2014/19/05
    copyright            : (C) 2014 by Vijay Dhameliya
    email                : vijay.atwork13@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SEARCHRESULTITEM_H
#define SEARCHRESULTITEM_H

#include <QObject>
#include <QString>


/**
  *\class DetailItem
  *\brief Constructs item for each detail downloaded along with
  * with image from astrobin API, to be added in QML Listview on detail page
  *\author Vijay Dhameliya
  */
class DetailItem : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString item READ getItem WRITE setItem NOTIFY itemChanged)

public:
    explicit DetailItem(QString item, QObject *parent = 0);

    QString getItem(){ return item; }

    void setItem(QString newItem);

signals:

    void itemChanged();

private:
    QString item;
};

/**
  *\class SearchResultItem
  *\brief Constructs item from downloaded thumbnail image to be added in QML Listview
  *\author Vijay Dhameliya
  */
class SearchResultItem : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString path READ getPath WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(QString name READ getName WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString date READ getDate WRITE setDate NOTIFY dateChanged)

public:
    explicit SearchResultItem(QString path, QString name, QString date, QObject *parent = 0);

    QString getPath(){ return path; }

    QString getName(){ return name; }

    QString getDate(){ return date; }

    void setPath(QString newPath);

    void setName(QString newName);

    void setDate(QString newDate);

signals:

    void pathChanged();

    void nameChanged();

    void dateChanged();

private:
    QString path;
    QString name;
    QString date;
    
};

#endif // SEARCHRESULTITEM_H
