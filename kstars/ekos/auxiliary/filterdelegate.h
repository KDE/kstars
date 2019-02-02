/*  Ekos Filter Delegates

    Collection of delegates assigned to each individual column
    in the table view.

    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <QStyledItemDelegate>
#include <QItemDelegate>

class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;

class NotEditableDelegate : public QStyledItemDelegate
{
        Q_OBJECT
    public:
        explicit NotEditableDelegate(QObject *parent = nullptr)
            : QStyledItemDelegate(parent)
        {}

    protected:
        bool editorEvent(QEvent *, QAbstractItemModel *, const QStyleOptionViewItem &, const QModelIndex &) override
        {
            return false;
        }
        QWidget* createEditor(QWidget *, const QStyleOptionViewItem &, const QModelIndex &) const override
        {
            return nullptr;
        }
};

class UseAutoFocusDelegate : public QItemDelegate
{
        Q_OBJECT
    public:
        explicit UseAutoFocusDelegate(QObject *parent = nullptr) : QItemDelegate(parent) {}

        QWidget * createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        void setEditorData(QWidget *, const QModelIndex &index) const override;
        void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &, const QModelIndex &index) override;

    private:
        mutable QCheckBox *cb = { nullptr };
};

class ExposureDelegate : public QStyledItemDelegate
{
        Q_OBJECT
    public:
        explicit ExposureDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

        QWidget * createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        void setEditorData(QWidget *, const QModelIndex &index) const override;
        void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
        void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    private:
        mutable QDoubleSpinBox *spinbox = { nullptr };
};

class OffsetDelegate : public QStyledItemDelegate
{
        Q_OBJECT
    public:
        explicit OffsetDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

        QWidget * createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        void setEditorData(QWidget *, const QModelIndex &index) const override;
        void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
        void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    private:
        mutable QSpinBox *spinbox = { nullptr };
};

class LockDelegate : public QStyledItemDelegate
{
        Q_OBJECT
    public:
        explicit LockDelegate(QStringList filterList, QObject *parent = nullptr);

        QWidget * createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        void setEditorData(QWidget *, const QModelIndex &index) const override;
        void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
        void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        void setCurrentFilterList(const QStringList &list);

    private:
        mutable QComboBox *lockbox = { nullptr };
        QStringList m_FilterList;
};

