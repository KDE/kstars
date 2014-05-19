#include "searchresultitem.h"

SearchResultItem::SearchResultItem(QString path, QString name, QString date, QObject *parent) :
    path(path), name(name), date(date), QObject(parent)
{
}

void SearchResultItem::setPath(QString newPath)
{
    path = newPath;
    emit pathChanged();
}

void SearchResultItem::setName(QString newName)
{
    name = newName;
    emit nameChanged();
}

void SearchResultItem::setDate(QString newDate)
{
    date = newDate;
    emit dateChanged();
}
