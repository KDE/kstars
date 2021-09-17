/*
    SPDX-FileCopyrightText: 2018 Valentin Boettcher <valentin@boettcher.cf>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once
#include <QFrame>
#include <QHash>
#include <QAbstractTableModel>
#include "eclipsetool/lunareclipsehandler.h"

namespace Ui
{
class EclipseTool;
}

class KSPlanetBase;
class QComboBox;
class GeoLocation;

/**
 * @brief The EclipseModel class
 * @short A simple model to contain EclipseEvents.
 */
class EclipseModel : public QAbstractTableModel
{
        Q_OBJECT
    public:
        // This is reimplemented boilerplate from QAbstractModel
        explicit EclipseModel(QObject * parent = nullptr);
        int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        int columnCount(const QModelIndex &parent = QModelIndex()) const override
        {
            Q_UNUSED(parent);
            return COLUMNS;
        }
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    public slots:
        /**
         * @brief reset
         * @short resets the data and the model
         */
        void reset();

        /**
         * @brief slotAddEclipse
         * @param eclipse
         * @short add an eclipse to the model
         */
        void slotAddEclipse(EclipseEvent_s eclipse);

        /**
         * @brief exportAsCsv
         * @short Export data as CSV (plus dialog)
         */
        void exportAsCsv();

        /**
         * @brief getEvent
         * @param index
         * @short retrieve an event
         */
        EclipseEvent_s getEvent(int index)
        {
            return m_eclipses.at(index);
        }

    private slots:
        // reimplemented to clear the data
        void resetInternalData()
        {
            m_eclipses.clear();
        }

    private:
        EclipseHandler::EclipseVector m_eclipses;
        const int COLUMNS { 5 };
};

/**
 * @brief The EclipseTool class
 * @short The UI for the Eclipsetool.
 * Currently only lunar eclipses are implemented.
 * Others will follow.
 */
class EclipseTool : public QFrame
{
        Q_OBJECT

    public:
        explicit EclipseTool(QWidget *parent = nullptr);
        ~EclipseTool();

    private slots:
        /**
         * @brief slotLocation
         * @short slot for setting the geolocation
         */
        void slotLocation();

        /**
         * @brief slotCompute
         * @short slot to start the computation
         */
        void slotCompute();

        /**
         * @brief slotFinished
         * @short slot to clear any residuals
         */
        void slotFinished();

        /**
         * @brief slotContextMenu
         * @short show context menu for an eclipse
         * @param pos
         */
        void slotContextMenu(QPoint pos);

        /**
         * @brief slotView
         * @param event
         * @short view an eclipse on the SkyMap
         */
        void slotView(EclipseEvent_s event);

    private:
        Ui::EclipseTool *ui;

        QVector<std::pair<int, QString>> m_object1List;
        QVector<std::pair<int, QString>> m_object2List;

        GeoLocation * m_geoLocation { nullptr };
        EclipseModel m_model;
};
