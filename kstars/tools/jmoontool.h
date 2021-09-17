/*
    SPDX-FileCopyrightText: 2003 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>

class QColor;

class KPlotWidget;
class KStars;

/**
 * @class JMoonTool
 * @short Display the positions of Jupiter's moons as a function of time
 *
 * @version 1.0
 * @author Jason Harris
 */
class JMoonTool : public QDialog
{
        Q_OBJECT

    public:
        explicit JMoonTool(QWidget *parent = nullptr);

    protected:
        virtual void keyPressEvent(QKeyEvent *e) override;

    private:
        void initPlotObjects();

        KPlotWidget *pw { nullptr };
        QColor colJp, colIo, colEu, colGn, colCa;
};
