/*  
    SPDX-FileCopyrightText: 2023 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Test for rectangleoverlap.cpp
*/

#pragma once

#include <QObject>

class TestRectangleOverlap: public QObject
{
    Q_OBJECT
public:
    explicit TestRectangleOverlap(QObject * parent = nullptr);

private slots:
    void testBasic_data();
    void testBasic();
};


