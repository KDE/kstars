/*  Ekos Filter Manager
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <QDialog>
#include <indi/indistd.h>
#include <indi/indifocuser.h>
#include <oal/filter.h>
#include <QSqlDatabase>

#include "ui_filtersettings.h"

class QSqlTableModel;

class FilterManager : public QDialog, public Ui::FilterSettings
{
public:
    FilterManager();
    void refreshFilterData();

    QStringList getFilterLabels(ISD::GDInterface *filter=nullptr);

    bool setFilterPosition(uint8_t position, ISD::GDInterface *filter=nullptr);
    int getFilterPosition(ISD::GDInterface *filter=nullptr);

    void addFilter(ISD::GDInterface *filter);
    void setCurrentFilter(ISD::GDInterface *filter);

    void addFocuser(ISD::GDInterface *focuser);
    void setCurrentFocuser(ISD::GDInterface *focuser);

public slots:
    void updateFilterNames();
    void updateFilterPosition();

signals:
    // Emitted only when there is a change in the filter slot number
    void currentFilterPositionChanged(int);
    // Emitted when filter change operation completed successfully including any focus offsets or auto-focus operation
    void currentFilterPositionCompleted(int);
    // Emitted when filter labels are updated for the current filter
    void currentFilterLabelsChanged(QStringList);

private slots:
    // Apply changes to database
    void apply();
    void processText(ITextVectorProperty *tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processSwitch(ISwitchVectorProperty *svp);

private:

    QStringList getFilterLabelsForDevice(ISD::GDInterface *filter);
    int getFilterPositionForDevice(ISD::GDInterface *filter);

    // Filter Wheel Devices
    QList<ISD::GDInterface*> m_filterDevices;
    ISD::GDInterface *m_currentFilterDevice = { nullptr };
    QStringList m_currentFilterList;
    int m_currentFilterPosition = { -1 };

    // Filter Structure
    QList<OAL::Filter *> m_ActiveFilters;
    QList<OAL::Filter *> m_FilterList;

    // Focusers
    QList<ISD::Focuser *> m_focuserDevices;
    ISD::Focuser *m_currentFocuserDevice = { nullptr };

    int16_t lastFilterOffset { 0 };

    // Table model
    QSqlTableModel *filterModel = { nullptr };

    // INDI Properties of current active filter
    ITextVectorProperty *m_currentFilterName { nullptr };
    INumberVectorProperty *m_currentFilterSlot { nullptr };
};
