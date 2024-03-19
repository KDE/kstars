/*
    SPDX-FileCopyrightText: 2003 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VIEWSDIALOG_H_
#define VIEWSDIALOG_H_

#include <QPaintEvent>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QStringList>
#include <QStringListModel>
#include <optional>
#include "skymapview.h"

#include "ui_viewsdialog.h"
#include "ui_newview.h"

class ViewsDialogUI : public QFrame, public Ui::ViewsDialog
{
        Q_OBJECT
    public:
        explicit ViewsDialogUI(QWidget *parent = nullptr);
};

class ViewsDialogStringListModel : public QStringListModel
{
    public:
        explicit ViewsDialogStringListModel(QObject* parent = nullptr)
            : QStringListModel(parent) {}

        Qt::ItemFlags flags(const QModelIndex &index) const override;
};

/**
 * @class ViewsDialog
 * @brief ViewsDialog is dialog to select a Sky Map View (or create a new one)
 *
 * A sky map view is a collection of settings that defines the
 * orientation and scale of the sky map and how it changes as the user
 * pans around.
 *
 * @author Akarsh Simha
 * @version 1.0
 */
class ViewsDialog : public QDialog
{
        Q_OBJECT
    public:
        explicit ViewsDialog(QWidget *parent = nullptr);

    private slots:
        void slotNewView();
        void slotEditView();
        void slotRemoveView();
        void slotSelectionChanged(const QModelIndex &, const QModelIndex &);

    private:

        /** Sync the model from the view manager */
        void syncModel();

        /** Sync the model to the view manager */
        void syncFromModel();

        QStringListModel* m_model;
        unsigned int currentItem() const;
        ViewsDialogUI *ui;
        static int viewID;
};

/**
 * @class NewView
 * Dialog for defining a new View
 * @author Akarsh Simha
 * @version 1.0
 */
class NewView : public QDialog, private Ui::NewView
{
        Q_OBJECT
    public:
        /** Create new dialog
             * @param parent parent widget
             * @param view to copy data from. If it's empty will create empty one.
             */
        explicit NewView(QWidget *parent = nullptr, std::optional<SkyMapView> view = std::nullopt);
        ~NewView() override;

        /** Return the view struct. */
        const SkyMapView getView() const;

    public slots:
        void updateViewingAnglePreviews();
        virtual void done(int r) override;

    private:
        QString m_originalName;
        QPixmap *m_observerPixmap; // Icon for an observer
        QPixmap *m_topPreview, *m_bottomPreview;
};

#endif
