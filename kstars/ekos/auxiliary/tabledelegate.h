/*  Optical Train Delegates

    Collection of delegates assigned to each individual column
    in the table view.

    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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

class ToggleDelegate : public QItemDelegate
{
        Q_OBJECT
    public:
        explicit ToggleDelegate(QObject *parent = nullptr) : QItemDelegate(parent) {}

        QWidget * createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        void setEditorData(QWidget *editor, const QModelIndex &index) const override;
        void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &, const QModelIndex &index) override;
};

class DoubleDelegate : public QStyledItemDelegate
{
        Q_OBJECT
    public:
        explicit DoubleDelegate(QObject *parent = nullptr, double min = -1, double max = -1, double step = -1) : QStyledItemDelegate(parent)
    {
        this->min = min;
        this->max = max;
        this->step = step;
    }

        QWidget * createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        void setEditorData(QWidget *editor, const QModelIndex &index) const override;
        void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
        void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    protected:
        double min {-1}, max {-1}, step {-1};
};

class IntegerDelegate : public QStyledItemDelegate
{
        Q_OBJECT
    public:
        explicit IntegerDelegate(QObject *parent = nullptr, int min = -1, int max = -1, int step = -1) : QStyledItemDelegate(parent)
    {
        this->min = min;
        this->max = max;
        this->step = step;
    }

        QWidget * createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        void setEditorData(QWidget *editor, const QModelIndex &index) const override;
        void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
        void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        void setMinMaxStep(int min, int max, int step);
    protected:
        int min {-1}, max {-1}, step {-1};
};

class ComboDelegate : public QStyledItemDelegate
{
        Q_OBJECT
    public:
        explicit ComboDelegate(QObject *parent = nullptr);

        QWidget * createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        void setEditorData(QWidget *editor, const QModelIndex &index) const override;
        void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
        void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
        void setValues(const QStringList &values);
        const QStringList &values() const
        {
            return m_Values;
        }

    private:
        mutable QStringList m_Values;
};

