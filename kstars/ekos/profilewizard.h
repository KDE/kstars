/*  Ekos Profile Wizard

    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef PROFILEWIZARD_H
#define PROFILEWIZARD_H

#include "ui_profilewizard.h"

#include <QProgressDialog>
#include <QHostInfo>
#include <QPointer>

namespace Ekos
{
class Manager;
}

class ProfileWizard : public QDialog, public Ui::ProfileWizard
{
        Q_OBJECT

    public:
        enum
        {
            INTRO,
            EQUIPMENT_LOCATION,
            REMOTE_EQUIPMENT_SELECTION,
            GENERIC_EQUIPMENT,
            PI_EQUIPMENT,
            WINDOWS_LOCAL,
            MAC_LOCAL,
            CREATE_PROFILE
        };
        typedef enum { INTERNAL_GUIDER, PHD2_GUIDER, LIN_GUIDER } GuiderType;

        ProfileWizard();

        QStringList selectedAuxDrivers();
        int selectedExternalGuider();

    protected slots:
        void reset();
        void processLocalEquipment();
        void processRemoteEquipmentSelection();
        void processRemoteEquipment();
        void processPiEquipment();
        void processLocalMac();
        void createProfile();
        void detectStellarMate();
        void processHostInfo(QHostInfo info);
        void detectStellarMateTimeout();

    private:
        bool useInternalServer   = true;
        bool useWebManager       = false;
        bool useJoystick         = false;
        bool useRemoteAstrometry = false;
        bool useWatchDog         = false;
        bool useSkySafari        = false;
        GuiderType useGuiderType = INTERNAL_GUIDER;

        QString profileName, host, port;
        QPointer<QProgressDialog> stellarMateDetectDialog;

        friend class Ekos::Manager;
};

#endif
