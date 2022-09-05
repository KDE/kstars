/*
    SPDX-FileCopyrightText: 2022 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opticaltrains.h"

#include <indi/indifilterwheel.h>
#include <indi/indifocuser.h>

#include <QDialog>
#include <QSqlDatabase>
#include <QQueue>
#include <QPointer>

class QSqlTableModel;
class ComboDelegate;
class NotEditableDelegate;
class DoubleDelegate;
class IntegerDelegate;
class ToggleDelegate;
class ProfileInfo;

namespace Ekos
{

class OpticalTrainManager : public QDialog, public Ui::OpticalTrain
{
        Q_OBJECT
        Q_PROPERTY(QStringList trainNames READ getTrainNames)

    public:

        static OpticalTrainManager *Instance();
        static void release();

        void setProfile(const QSharedPointer<ProfileInfo> &profile);        

        const QVariantMap getOpticalTrain(uint8_t id) const;
        const QVariantMap getOpticalTrain(const QString &name) const;
        const QList<QVariantMap> &getOpticalTrains() const
        {
            return m_OpticalTrains;
        }
        const QStringList &getTrainNames() const
        {
            return m_TrainNames;
        }

        void addOpticalTrain(const QJsonObject &value);

        /**
         * @brief setOpticalTrainValue Set specific field of optical train
         * @param name Name of optical field
         * @param field Name of field
         * @param value Value of field
         * @return True if set is successful, false otherwise.
         */
        bool setOpticalTrainValue(const QString &name, const QString &field, const QVariant &value);

        void refreshModel();
        void refreshTrains();
        void refreshOpticalElements();
        /**
         * @brief syncDevices Sync delegates and then update model accordingly.
         */
        void syncDevices();

        int id(const QString &name);
        ISD::Mount *getMount(const QString &name);
        ISD::DustCap *getDustCap(const QString &name);
        ISD::LightBox *getLightBox(const QString &name);
        QJsonObject getScope(const QString &name);
        double getReducer(const QString &name);
        ISD::Rotator *getRotator(const QString &name);
        ISD::Focuser *getFocuser(const QString &name);
        ISD::FilterWheel *getFilterWheel(const QString &name);
        ISD::Camera *getCamera(const QString &name);
        ISD::Guider *getGuider(const QString &name);
        ISD::AdaptiveOptics *getAdaptiveOptics(const QString &name);

    signals:
        void updated();
        void configurationRequested();

    protected:
        void initModel();

        // Delegates
        QPointer<ComboDelegate> m_MountDelegate;
        QPointer<ComboDelegate> m_DustCapDelegate;
        QPointer<ComboDelegate> m_LightBoxDelegate;
        QPointer<ComboDelegate> m_ScopeDelegate;
        QPointer<DoubleDelegate> m_ReducerDelegate;
        QPointer<ComboDelegate> m_RotatorDelegate;
        QPointer<ComboDelegate> m_FocuserDelegate;
        QPointer<ComboDelegate> m_FilterWheelDelegate;
        QPointer<ComboDelegate> m_CameraDelegate;
        QPointer<ComboDelegate> m_GuiderDelegate;

        // Columns
        enum
        {
            ID,
            Profile,
            Name,
            Mount,
            DustCap,
            LightBox,
            Scope,
            Reducer,
            Rotator,
            Focuser,
            FilterWheel,
            Camera,
            Guider,
        };

    private:

        OpticalTrainManager();
        static OpticalTrainManager *m_Instance;

        /**
         * @brief generateOpticalTrains Automatically generate optical trains based on the current profile information.
         * This happens when users use the tool for the first time.
         */
        void generateOpticalTrains();

        void addOpticalTrain(bool main, const QString &name);

        /**
           * @brief syncDelegatesToDevices Parses INDI devices and updates delegates accordingly.
           */
        void syncDelegatesToDevices();

        QSharedPointer<ProfileInfo> m_Profile;
        QList<QVariantMap> m_OpticalTrains;

        // Table model
        QSqlTableModel *m_OpticalTrainsModel = { nullptr };

        QStringList m_TrainNames;
};

}
