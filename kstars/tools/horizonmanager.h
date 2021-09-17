/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_horizonmanager.h"

#include <QDialog>

#include <memory>

class QStandardItem;
class QStandardItemModel;

class ArtificialHorizonComponent;
class ArtificialHorizonEntity;
class LineList;
class SkyPoint;

class HorizonManagerUI : public QFrame, public Ui::HorizonManager
{
        Q_OBJECT

    public:
        /** @short Constructor */
        explicit HorizonManagerUI(QWidget *parent);
};

/**
 * @class HorizonManager
 * @short Manages adding/removing and editing regions and points associated with
 * user-customized artificial horizons.
 *
 * @version 1.0
 * @author Jasem Mutlaq
 */
class HorizonManager : public QDialog
{
        Q_OBJECT
    public:
        /** @short Constructor */
        explicit HorizonManager(QWidget *ks);

        /** @short Destructor */
        virtual ~HorizonManager() override = default;

        void showRegion(const int regionID);

        bool validate(int regionID);

        void deleteRegion(int regionID);

    protected:
        void closeEvent(QCloseEvent *event) override;
        void showEvent(QShowEvent *event) override;

    public slots:
        /** @short Add region */
        void slotAddRegion();

        /** @short Delete region */
        void slotRemoveRegion();

        void slotToggleCeiling();

        void addSkyPoint(SkyPoint *skypoint);
        void slotAddPoint();
        void slotRemovePoint();
        void slotClosed();

        void clearPoints();

        void setSelectPoints(bool);
        void slotCurrentPointChanged(const QModelIndex &current, const QModelIndex &previous);

    private slots:
        void verifyItemValue(QStandardItem *item);
        void slotSaveChanges();
        void slotSetShownRegion(QModelIndex idx);

    private:
        void addPoint(SkyPoint *skyPoint);
        void terminateLivePreview();
        void setPointSelection(bool enable);
        void removeEmptyRows(int regionID);
        void setupLivePreview(QStandardItem *item);
        void setupValidation(int regionID);

        HorizonManagerUI *ui { nullptr };

        QStandardItemModel *m_RegionsModel { nullptr };
        ArtificialHorizonComponent *horizonComponent { nullptr };

        std::shared_ptr<LineList> livePreview;
        bool selectPoints { false };

        friend class TestArtificialHorizon;
};
