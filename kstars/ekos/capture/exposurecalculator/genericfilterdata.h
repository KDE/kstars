/*
    SPDX-FileCopyrightText: 2023 Joseph McGee <joseph.mcgee@sbcglobal.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractItemModel>

namespace OptimalExposure
{

class GenericFilterData
{
    public:
        GenericFilterData();

        const QString &getFilterId() const;
        void setFilterId(const QString &newFilterId);
        const QString &getFilterDescription() const;
        void setFilterDescription(const QString &newFilterDescription);
        int getFilterBandPassWidth() const;
        void setFilterBandPassWidth(int newFilterBandPassWidth);

        double getFilterCompensation() const;

    private:
        QString filterId;
        QString filterDescription;
        int filterBandPassWidth;
};
}


