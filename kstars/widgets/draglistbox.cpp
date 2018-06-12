/***************************************************************************
                          draglistbox.cpp  -  description
                             -------------------
    begin                : Sun May 29 2005
    copyright            : (C) 2005 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "draglistbox.h"

#include <KLocalizedString>

#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>

DragListBox::DragListBox(QWidget *parent, const char *name) : QListWidget(parent)
{
    if (name)
    {
        setObjectName(name);
    }
    setAcceptDrops(true);
}

bool DragListBox::contains(const QString &s) const
{
    QList<QListWidgetItem *> foundList = findItems(s, Qt::MatchExactly);
    if (foundList.isEmpty())
        return false;
    else
        return true;
}

void DragListBox::dragEnterEvent(QDragEnterEvent *evt)
{
    if (evt->mimeData()->hasText())
    {
        evt->acceptProposedAction();
    }
    else
    {
        evt->ignore();
    }
}

void DragListBox::dragMoveEvent(QDragMoveEvent *evt)
{
    if (evt->mimeData()->hasText())
    {
        evt->acceptProposedAction();
    }
    else
    {
        evt->ignore();
    }
}

void DragListBox::dropEvent(QDropEvent *evt)
{
    QString text;

    if (evt->mimeData()->hasText())
    {
        text = evt->mimeData()->text();

        //Copy an item dragged from FieldPool to FieldList
        //If we dragged an "Ignore" item from the FieldPool to the FieldList, then we don't
        //need to insert the item, because FieldPool already has a persistent Ignore item.
        if (!(text == i18n("Ignore") && QString(evt->source()->objectName()) == "FieldList" && evt->source() != this))
        {
            QListWidgetItem *lwi = itemAt(evt->pos());
            if (lwi == nullptr && evt->pos().y() > visualItemRect(item(count() - 1)).bottom())
            {
                addItem(text);
            }
            else
            {
                int i = row(itemAt(evt->pos()));
                insertItem(i, text);
            }
        }

        //Remove an item dragged from FieldList to FieldPool.
        //If we dragged the "Ignore" item from FieldList to FieldPool, then we don't
        //want to remove the item from the FieldPool
        if (!(text == i18n("Ignore") && QString(evt->source()->objectName()) == "FieldPool" && evt->source() != this))
        {
            DragListBox *fp = (DragListBox *)evt->source();
            delete fp->takeItem(fp->currentRow());
        }
    }
    evt->acceptProposedAction();
}

void DragListBox::mousePressEvent(QMouseEvent *evt)
{
    QListWidget::mousePressEvent(evt);

    if (evt->button() == Qt::LeftButton)
    {
        leftButtonDown = true;
    }
}

void DragListBox::mouseReleaseEvent(QMouseEvent *evt)
{
    QListWidget::mouseReleaseEvent(evt);

    if (evt->button() == Qt::LeftButton)
    {
        leftButtonDown = false;
    }
}

void DragListBox::mouseMoveEvent(QMouseEvent *evt)
{
    if (leftButtonDown)
    {
        leftButtonDown = false; //Don't create multiple QDrag objects!

        QDrag *drag         = new QDrag(this);
        QMimeData *mimeData = new QMimeData;
        mimeData->setText(currentItem()->text());
        drag->setMimeData(mimeData);

        drag->exec();
        evt->accept();
    }
}
