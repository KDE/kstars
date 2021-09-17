/*
    SPDX-FileCopyrightText: 2017 Robert Lancaster <rlancaste@gmail.com>

    Based on an idea discussed in the QT Centre: https://www.qtcentre.org/threads/47535-QDoubleSpinBox-with-nonlinear-values

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDoubleSpinBox>

class NonLinearDoubleSpinBox : public QDoubleSpinBox
{
        Q_OBJECT
    public:
        explicit NonLinearDoubleSpinBox(QWidget *parent = Q_NULLPTR);

        void stepBy(int steps) override;
        void setRecommendedValues(QList<double> values);
        void addRecommendedValue(double v);
        QList<double> getRecommendedValues();
        QString getRecommendedValuesString();

    private:
        QList<double> m_Values;
        int m_idx { -1 };
        void updateRecommendedValues();
};
