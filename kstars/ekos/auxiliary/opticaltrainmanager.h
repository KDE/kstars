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

        typedef enum
        {
            Mount,
            Camera,
            Rotator,
            GuideVia,
            DustCap,
            Scope,
            FilterWheel,
            Focuser,
            Reducer,
            LightBox
        } Role;

        void setProfile(const QSharedPointer<ProfileInfo> &profile);
        void checkOpticalTrains();

        bool exists(uint8_t id) const;
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
         * @brief Select an optical train and fill the field values in the train editor with the
         *        appropriate values of the selected optical train.
         * @param item optical train list item
         * @return true if optical train found
         */
        bool selectOpticalTrain(QListWidgetItem *item);

        /**
         * @brief findTrainContainingDevice Search optical trains for device match a specific role.
         * @param name Device Name
         * @param role Device Role
         * @return Train containing device name matching the specified role
         */
        QString findTrainContainingDevice(const QString &name, Role role);

        /**
         * @brief Select an optical train and fill the field values in the train editor with the
         *        appropriate values of the selected optical train.
         * @param name optical train name
         * @return true if optical train found
         */
        bool selectOpticalTrain(const QString &name);

        /**
         * @brief Show the dialog and select an optical train for editing.
         * @param name optical train name
         */
        void openEditor(const QString &name);

        /**
         * @brief setOpticalTrainValue Set specific field of optical train
         * @param name Name of optical field
         * @param field Name of field
         * @param value Value of field
         * @return True if set is successful, false otherwise.
         */
        bool setOpticalTrainValue(const QString &name, const QString &field, const QVariant &value);

        /**
         * @brief Change the name of the currently selected optical train to a new value
         * @param name new train name
         */
        void renameCurrentOpticalTrain(const QString &name);

        /**
         * @brief setOpticalTrain Replaces optical train matching the name of the passed train.
         * @param train Train information, including name and database id
         * @return True if train is successfully updated in the database.
         */
        bool setOpticalTrain(const QJsonObject &train);

        /**
         * @brief removeOpticalTrain Remove optical train from database and all associated settings
         * @param name name if the optical train to remove
         * @return True if successful, false if id is not found.
         */
        bool removeOpticalTrain(const QString &name);

        void refreshModel();
        void refreshTrains();
        void refreshOpticalElements();
        /**
         * @brief syncDevices Sync delegates and then update model accordingly.
         */
        void syncDevices();

        /**
         * @brief syncActiveDevices Syncs ACTIVE_DEVICES in INDI as per the optical train settings.
         */
        void syncActiveDevices();

        /**
         * @brief getGenericDevice Find Generic Device matching given Role in the given train
         * @param train Train Name
         * @param role Device Role
         * @param generic Generic Device. If found, the pointer is pointing to GenericDevice
         * @return True if found, false if not found.
         */
        bool getGenericDevice(const QString &train, Role role, QSharedPointer<ISD::GenericDevice> &generic);

        /**
         * @brief syncActiveProperties Since ACTIVE_DEVICE properties
         * @param train Train map
         * @param device Generic device
         */
        void syncActiveProperties(const QVariantMap &train, const QSharedPointer<ISD::GenericDevice> &device);

        /**
         * @brief id Get database ID for a given train
         * @param name Name of train
         * @return ID if exists, or -1 if not found.
         */
        int id(const QString &name) const;

        /**
         * @brief name Get database name for a given name
         * @param id database ID for the train to get
         * @return Train name, or empty string if not found.
         */
        QString name(int id) const;

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
        void configurationRequested(bool show);

    protected:
        void initModel();
        QStringList getMissingDevices() const;

    private slots:
        /**
         * @brief Update an element value in the currently selected optical train
         * @param cb combo box holding the new value
         * @param element element name
         */
        void updateOpticalTrainValue(QComboBox *cb, const QString &element);
        /**
         * @brief Update an element value in the currently selected optical train
         * @param value the new value
         * @param element element name
         */
        void updateOpticalTrainValue(double value, const QString &element);

    private:

        OpticalTrainManager();
        static OpticalTrainManager *m_Instance;

        /**
         * @brief generateOpticalTrains Automatically generate optical trains based on the current profile information.
         * This happens when users use the tool for the first time.
         */
        void generateOpticalTrains();

        /**
         * @brief Add a new optical train with the given name
         * @param index train index (0, 1, 2..etc)
         * @param name train name
         * @return unique train name
         */
        QString addOpticalTrain(uint8_t index, const QString &name);

        void checkMissingDevices();

        /**
           * @brief syncDelegatesToDevices Parses INDI devices and updates delegates accordingly.
           * @return True if all devices synced. False if no new devices synced.
           */
        bool syncDelegatesToDevices();

        /**
         * @brief Create a unique train name
         * @param name original train name
         * @return name, eventually added (i) to make the train name unique
         */
        QString uniqueTrainName(QString name);

        QSharedPointer<ProfileInfo> m_Profile;
        QList<QVariantMap> m_OpticalTrains;
        QTimer m_CheckMissingDevicesTimer;
        QTimer m_DelegateTimer;
        QVariantMap *m_CurrentOpticalTrain = nullptr;

        // make changes persistent
        bool m_Persistent = false;

        // Table model
        QSqlTableModel *m_OpticalTrainsModel = { nullptr };

        QStringList m_TrainNames;
        // Device names
        QStringList m_MountNames;
        QStringList m_DustCapNames;
        QStringList m_LightBoxNames;
        QStringList m_ScopeNames;
        QStringList m_ReducerNames;
        QStringList m_RotatorNames;
        QStringList m_FocuserNames;
        QStringList m_FilterWheelNames;
        QStringList m_CameraNames;
        QStringList m_GuiderNames;
};

}
