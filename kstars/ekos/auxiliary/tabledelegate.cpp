/*  Common Table View Delegates

    Collection of delegates assigned to each individual column
    in the table view.

    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "tabledelegate.h"
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QApplication>

QWidget * ToggleDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
    QCheckBox *editor = new QCheckBox(parent);
    return editor;
}


void ToggleDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    int value = index.model()->data(index, Qt::EditRole).toInt();
    dynamic_cast<QCheckBox*>(editor)->setChecked(value == 1);
}

void ToggleDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    QStyleOptionButton checkboxstyle;
    QRect checkbox_rect = QApplication::style()->subElementRect(QStyle::SE_CheckBoxIndicator, &checkboxstyle);

    //center
    checkboxstyle.rect = option.rect;
    checkboxstyle.rect.setLeft(option.rect.x() + option.rect.width() / 2 - checkbox_rect.width() / 2);

    editor->setGeometry(checkboxstyle.rect);
}

void ToggleDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    int value = dynamic_cast<QCheckBox*>(editor)->isChecked() ? 1 : 0;
    model->setData(index, value, Qt::EditRole);
}

void ToggleDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionButton checkboxstyle;
    QRect checkbox_rect = QApplication::style()->subElementRect(QStyle::SE_CheckBoxIndicator, &checkboxstyle);

    QRect rect(option.rect.x() + option.rect.width() / 2 - checkbox_rect.width() / 2,
               option.rect.y() + option.rect.height() / 2 - checkbox_rect.height() / 2,
               checkbox_rect.width(), checkbox_rect.height());

    drawCheck(painter, option, rect, index.data().toBool() ? Qt::Checked : Qt::Unchecked);
}

bool ToggleDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &,
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
// Double Delegate
/////////////////////////////////////////////////////////

QWidget * DoubleDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
    QDoubleSpinBox *editor = new QDoubleSpinBox(parent);
    editor->setFrame(false);
    editor->setDecimals(3);

    if (step > 0)
    {
        editor->setMinimum(min);
        editor->setMaximum(max);
        editor->setSingleStep(step);
    }
    else
    {
        editor->setMinimum(0.001);
        editor->setMaximum(120);
        editor->setSingleStep(1);
    }
    return editor;
}

void DoubleDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    double value = index.model()->data(index, Qt::EditRole).toDouble();
    dynamic_cast<QDoubleSpinBox*>(editor)->setValue(value);
}

void DoubleDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    double value = dynamic_cast<QDoubleSpinBox*>(editor)->value();
    model->setData(index, value, Qt::EditRole);
}

void DoubleDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}

/////////////////////////////////////////////////////////
// Integer Delegate
/////////////////////////////////////////////////////////

QWidget * IntegerDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
    QSpinBox *editor = new QSpinBox(parent);
    editor->setFrame(false);
    if (step > 0)
    {
        editor->setMinimum(min);
        editor->setMaximum(max);
        editor->setSingleStep(step);
    }
    else
    {
        editor->setMinimum(-10000);
        editor->setMaximum(10000);
        editor->setSingleStep(100);
    }
    return editor;
}


void IntegerDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    int value = index.model()->data(index, Qt::EditRole).toInt();
    dynamic_cast<QSpinBox*>(editor)->setValue(value);
}

void IntegerDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    int value = dynamic_cast<QSpinBox*>(editor)->value();
    model->setData(index, value, Qt::EditRole);
}

void IntegerDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}

/////////////////////////////////////////////////////////
// Combo Delegate
/////////////////////////////////////////////////////////

ComboDelegate::ComboDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
    m_Values.insert(0, "--");
}

QWidget * ComboDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
    QComboBox *editor = new QComboBox(parent);
    editor->addItems(m_Values);
    return editor;
}


void ComboDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QString currentCombo = index.model()->data(index, Qt::EditRole).toString();
    dynamic_cast<QComboBox*>(editor)->setCurrentText(currentCombo);
}

void ComboDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QString value = dynamic_cast<QComboBox*>(editor)->currentText();
    model->setData(index, value, Qt::EditRole);
}

void ComboDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}

void ComboDelegate::setValues(const QStringList &list)
{
    m_Values = list;
    m_Values.insert(0, "--");
}

/////////////////////////////////////////////////////////
// Datetime Delegate
/////////////////////////////////////////////////////////

QWidget * DatetimeDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
    QDateTimeEdit *editor = new QDateTimeEdit(parent);
    editor->setDisplayFormat(format);
    editor->setMaximumDateTime(QDateTime::fromString(maxDT));
    editor->setMinimumDateTime(QDateTime::fromString(minDT));
    editor->setCalendarPopup(calPopup);
    editor->setFrame(false);
    return editor;
}

void DatetimeDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QDateTime value = index.model()->data(index, Qt::EditRole).toDateTime();
    dynamic_cast<QDateTimeEdit*>(editor)->setDateTime(value);
}

void DatetimeDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QDateTime value = dynamic_cast<QDateTimeEdit*>(editor)->dateTime();
    model->setData(index, value.toString(format), Qt::EditRole);
}

void DatetimeDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &) const
{
    editor->setGeometry(option.rect);
}
