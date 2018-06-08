/*  Non Linear Spin Box
    Copyright (C) 2017 Robert Lancaster <rlancaste@gmail.com>

    Based on an idea discussed in the QT Centre: http://www.qtcentre.org/threads/47535-QDoubleSpinBox-with-nonlinear-values

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <QDoubleSpinBox>

class NonLinearDoubleSpinBox : public QDoubleSpinBox
{
    Q_OBJECT
    public:
        explicit NonLinearDoubleSpinBox(QWidget *parent = Q_NULLPTR);

        void stepBy(int steps);
        void setRecommendedValues(QList<double> values);
        void addRecommendedValue(double v);
        QList<double> getRecommendedValues();
        QString getRecommendedValuesString();

    private:
        QList<double> m_Values;
        int m_idx { -1 };
        void updateRecommendedValues();
};
