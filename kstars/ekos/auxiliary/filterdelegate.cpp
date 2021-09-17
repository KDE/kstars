/*  Ekos Filter Delegates

    Collection of delegates assigned to each individual column
    in the table view.

    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "filterdelegate.h"
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QApplication>

QWidget * UseAutoFocusDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
    cb = new QCheckBox(parent);
    return cb;
}


void UseAutoFocusDelegate::setEditorData(QWidget *, const QModelIndex &index) const
{
    int value = index.model()->data(index, Qt::EditRole).toInt();
    cb->setChecked(value == 1);
}

void UseAutoFocusDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    QStyleOptionButton checkboxstyle;
    QRect checkbox_rect = QApplication::style()->subElementRect(QStyle::SE_CheckBoxIndicator, &checkboxstyle);

    //center
    checkboxstyle.rect = option.rect;
    checkboxstyle.rect.setLeft(option.rect.x() + option.rect.width()/2 - checkbox_rect.width()/2);

    editor->setGeometry(checkboxstyle.rect);
}

void UseAutoFocusDelegate::setModelData(QWidget *, QAbstractItemModel *model, const QModelIndex &index) const
{
    int value = cb->isChecked() ? 1 : 0;
    model->setData(index, value, Qt::EditRole);
}

void UseAutoFocusDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionButton checkboxstyle;
    QRect checkbox_rect = QApplication::style()->subElementRect(QStyle::SE_CheckBoxIndicator, &checkboxstyle);

    QRect rect(option.rect.x() + option.rect.width()/2 - checkbox_rect.width()/2,
               option.rect.y() + option.rect.height()/2 - checkbox_rect.height()/2,
               checkbox_rect.width(), checkbox_rect.height());

    drawCheck(painter, option, rect, index.data().toBool() ? Qt::Checked : Qt::Unchecked);
    //drawFocus(painter, option, checkboxstyle.rect);
}

bool UseAutoFocusDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &,
                                   const QModelIndex &index)
{
    if (event->type() == QEvent::MouseButtonRelease)
    {
        bool checked = index.model()->data(index, Qt::DisplayRole).toBool();
        // Toggle value
        int value = checked ? 0 : 1;
        return model->setData(index, value, Qt::EditRole);
    }

    return false;
}

/////////////////////////////////////////////////////////
// Exposure Delegate
/////////////////////////////////////////////////////////

QWidget * ExposureDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
    spinbox = new QDoubleSpinBox(parent);
    spinbox->setFrame(false);
    spinbox->setDecimals(3);
    spinbox->setMinimum(0.001);
    spinbox->setMaximum(120);
    spinbox->setSingleStep(1);
    return spinbox;
}


void ExposureDelegate::setEditorData(QWidget *, const QModelIndex &index) const
{
    double value = index.model()->data(index, Qt::EditRole).toDouble();
    spinbox->setValue(value);
}

void ExposureDelegate::setModelData(QWidget *, QAbstractItemModel *model, const QModelIndex &index) const
{
    double value = spinbox->value();
    model->setData(index, value, Qt::EditRole);
}

void ExposureDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}

/////////////////////////////////////////////////////////
// Offset Delegate
/////////////////////////////////////////////////////////

QWidget * OffsetDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
    spinbox = new QSpinBox(parent);
    spinbox->setFrame(false);
    spinbox->setMinimum(-10000);
    spinbox->setMaximum(10000);
    return spinbox;
}


void OffsetDelegate::setEditorData(QWidget *, const QModelIndex &index) const
{
    int value = index.model()->data(index, Qt::EditRole).toInt();
    spinbox->setValue(value);
}

void OffsetDelegate::setModelData(QWidget *, QAbstractItemModel *model, const QModelIndex &index) const
{
    int value = spinbox->value();
    model->setData(index, value, Qt::EditRole);
}

void OffsetDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}

/////////////////////////////////////////////////////////
// Lock Delegate
/////////////////////////////////////////////////////////

LockDelegate::LockDelegate(QStringList filterList, QObject *parent) : QStyledItemDelegate(parent)
{
    m_FilterList = filterList;
    m_FilterList.insert(0, "--");
}

QWidget * LockDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
    lockbox = new QComboBox(parent);
    lockbox->addItems(m_FilterList);
    return lockbox;
}


void LockDelegate::setEditorData(QWidget *, const QModelIndex &index) const
{
    QString currentLockFilter = index.model()->data(index, Qt::EditRole).toString();
    lockbox->setCurrentText(currentLockFilter);
}

void LockDelegate::setModelData(QWidget *, QAbstractItemModel *model, const QModelIndex &index) const
{
    QString value = lockbox->currentText();
    model->setData(index, value, Qt::EditRole);
}

void LockDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}

void LockDelegate::setCurrentFilterList(const QStringList &list)
{
     m_FilterList = list;
     m_FilterList.insert(0, "--");
}
