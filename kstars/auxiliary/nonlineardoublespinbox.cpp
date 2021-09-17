/*
    SPDX-FileCopyrightText: 2017 Robert Lancaster <rlancaste@gmail.com>

    Based on an idea discussed in the QT Centre: http://www.qtcentre.org/threads/47535-QDoubleSpinBox-with-nonlinear-values

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "auxiliary/nonlineardoublespinbox.h"

NonLinearDoubleSpinBox::NonLinearDoubleSpinBox(QWidget *parent) : QDoubleSpinBox(parent)
{
    m_Values << 0.01 << 0.02 << 0.05 << 0.1 << 0.2 << 0.25 << 0.5 << 1 << 1.5 << 2 << 2.5 << 3 << 5 << 6 << 7 << 8 << 9 << 10 << 20 << 30 << 40 << 50 << 60 << 120 << 180 << 300 << 600 << 900;
    setRange(m_Values.first() , m_Values.last());

    //This will update the _idx variable to the index of the new value.  It will give -1 if not in the list, and the index if it is in the list.
    connect(this, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            [=](double d){ m_idx = m_Values.indexOf(d); });
}

void NonLinearDoubleSpinBox::stepBy(int steps)
{
    // If the current value is not currently in the list, it will find where the value falls in the list and set it to the
    // appropriate one whether going up or down.
    if(m_idx == -1)
    {
        int i = 0;
        while(value() > m_Values.at(i))
            i++;
        if(steps > 0 )
            m_idx = i;
        else
            m_idx = i - 1;
    }
    else
        m_idx += steps;

    //This makes sure that it doesn't go past the ends of the list.
    if(m_idx<0)
        m_idx=0;
    else if(m_idx >= m_Values.count())
        m_idx = m_Values.count()-1;

    //This sets the value to the value at that index in the list.
    setValue( m_Values.at(m_idx) );
}

void NonLinearDoubleSpinBox::setRecommendedValues(QList<double> values)
{
    m_Values = values;
    updateRecommendedValues();
}

void NonLinearDoubleSpinBox::addRecommendedValue(double v)
{
    m_Values.append(v);  //This will be sorted into place in the next command.
    updateRecommendedValues();
}

void NonLinearDoubleSpinBox::updateRecommendedValues()
{
    std::sort(m_Values.begin(), m_Values.end());  //This will make sure they are all in order.
    m_idx = m_Values.indexOf(value());  //This will update the _idx variable to the index of the new value.  It will search for current value in the new list or set it to negative 1 if it isn't in the list.
    setRange(m_Values.first() , m_Values.last());
    //This makes sure all the values are in the range.  The range is expanded if necessary.
    if(m_Values.first() < minimum())
        setMinimum(m_Values.first());
    if(m_Values.last() > maximum())
        setMaximum(m_Values.last());
}

QList<double> NonLinearDoubleSpinBox::getRecommendedValues()
{
    return m_Values;
}

QString NonLinearDoubleSpinBox::getRecommendedValuesString()
{
    QString returnString;
    for(int i=0; i < m_Values.size() - 1; i++)
        returnString += QString::number(m_Values.at(i)) + ", ";
    returnString += QString::number(m_Values.last());
    return returnString;
}
