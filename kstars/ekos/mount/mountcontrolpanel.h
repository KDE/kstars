/*
    SPDX-FileCopyrightText: 2024 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDialog>
#include "ui_mountcontrolpanel.h"

namespace Ekos
{
class MountControlPanel : public QDialog, public Ui::MountControlPanel
{
    Q_OBJECT

    Q_PROPERTY(bool isJ2000 MEMBER m_isJ2000)

    public:
        MountControlPanel(QWidget *parent);

    protected:
        void keyPressEvent(QKeyEvent *event);
        void keyReleaseEvent(QKeyEvent *event);

    private:
        void findTarget();
        void processSlew();
        void processSync();
        bool processCoords(dms &ra, dms &de);
        bool m_isJ2000 {false};

    signals:
        void newMotionCommand(int command, int NS, int WE);
        void newSlewRate(int rate);
        void aborted();
        void park();
        void unpark();
        void center();
        void sync(double RA, double DE);
        void slew(double RA, double DE);
        void updownReversed(bool enable);
        void leftrightReversed(bool enable);


    friend class Mount;

};
}

