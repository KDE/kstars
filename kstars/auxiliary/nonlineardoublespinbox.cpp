/*  NonLinear Double Spin Box
    Copyright (C) 2017 Robert Lancaster <rlancaste@gmail.com>

    Based on an idea discussed in the QT Centre: http://www.qtcentre.org/threads/47535-QDoubleSpinBox-with-nonlinear-values

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "auxiliary/nonlineardoublespinbox.h"

NonLinearDoubleSpinBox::NonLinearDoubleSpinBox(QWidget *parent) : QDoubleSpinBox(parent)
{
    _values << 0.01 << 0.02 << 0.05 << 0.1 << 0.2 << 0.25 << 0.5 << 1 << 1.5 << 2 << 2.5 << 3 << 5 << 6 << 7 << 8 << 9 << 10 << 20 << 30 << 40 << 50 << 60 << 120 << 180 << 300 << 600 << 900;
    setRange(_values.first() , _values.last());
    //This will give -1 if not in the list, and the index if it is in the list.
    connect(this, QOverload<double>::of(&QDoubleSpinBox::valueChanged),[=](double d){ _idx = _values.indexOf(d);});
}

void NonLinearDoubleSpinBox::stepBy(int steps)
    {
        // If the current value is not currently in the list, it will find where the value falls in the list and set it to the
        // appropriate one whether going up or down.
        if(_idx == -1)
        {
            int i = 0;
            while(value() > _values.at(i))
                i++;
            if(steps > 0 )
                _idx = i;
            else
                _idx = i - 1;
        }
        else
            _idx += steps;

        //This makes sure that it doesn't go past the ends of the list.
        if(_idx<0)
            _idx=0;
        else if(_idx >= _values.count())
            _idx = _values.count()-1;

        //This sets the value to the value at that index in the list.
        setValue( _values.at(_idx) );
    }

void NonLinearDoubleSpinBox::setRecommendedValues(QList<double> values){
    _values = values;
    _idx = _values.indexOf(value());  //This way it will search for current value in the new list or set it to negative 1 if it isn't in the list.
    setRange(_values.first() , _values.last());
}
