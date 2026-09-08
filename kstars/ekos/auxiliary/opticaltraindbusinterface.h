/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QDBusConnection>

namespace Ekos
{

/**
 * @class OpticalTrainDBusInterface
 * @brief Per-train DBus object that exposes a single optical train on the session bus.
 *
 * One instance is created for every optical train managed by OpticalTrainManager.
 * Each instance registers itself at the unique path:
 *   /KStars/Ekos/OpticalTrain/{id}
 *
 * where {id} is the database identifier of the train.
 *
 * The interface mirrors the org.kde.kstars.Ekos.OpticalTrain DBus interface
 * declared in org.kde.kstars.Ekos.OpticalTrain.xml.
 */
class OpticalTrainDBusInterface : public QObject
{
        Q_OBJECT

        // Identity
        Q_PROPERTY(QString name       READ name)
        Q_PROPERTY(int     id         READ id)

        // Device assignments
        Q_PROPERTY(QString mount       READ mount)
        Q_PROPERTY(QString camera      READ camera)
        Q_PROPERTY(QString focuser     READ focuser)
        Q_PROPERTY(QString filterWheel READ filterWheel)
        Q_PROPERTY(QString rotator     READ rotator)
        Q_PROPERTY(QString dustCap     READ dustCap)
        Q_PROPERTY(QString lightBox    READ lightBox)
        Q_PROPERTY(QString scope       READ scope)
        Q_PROPERTY(double  reducer     READ reducer)
        Q_PROPERTY(QString guider      READ guider)

    public:
        /**
         * @brief Construct a DBus interface for the optical train identified by @p trainID.
         *
         * The object registers itself on the session bus at
         *   /KStars/Ekos/OpticalTrain/{trainID}
         *
         * @param trainID  Database ID of the optical train.
         * @param parent   Parent QObject (typically OpticalTrainManager).
         */
        explicit OpticalTrainDBusInterface(int trainID, QObject *parent = nullptr);
        ~OpticalTrainDBusInterface() override;

        /** @return The DBus object path this interface is registered at. */
        QString dbusObjectPath() const
        {
            return m_DBusObjectPath;
        }

        // ── Property readers ────────────────────────────────────────────────
        QString name()       const;
        int     id()         const
        {
            return m_TrainID;
        }
        QString mount()      const;
        QString camera()     const;
        QString focuser()    const;
        QString filterWheel()const;
        QString rotator()    const;
        QString dustCap()    const;
        QString lightBox()   const;
        QString scope()      const;
        double  reducer()    const;
        QString guider()     const;

    public Q_SLOTS:
        // ── Setter methods (exposed over DBus) ──────────────────────────────
        bool setMount(const QString &name);
        bool setCamera(const QString &name);
        bool setFocuser(const QString &name);
        bool setFilterWheel(const QString &name);
        bool setRotator(const QString &name);
        bool setDustCap(const QString &name);
        bool setLightBox(const QString &name);
        bool setScope(const QString &name);
        bool setReducer(double value);
        bool setGuider(const QString &name);

        /** @brief Remove this train from the database. */
        bool remove();

        /**
         * @brief Remove stored device references that are not currently connected.
         * @return The number of entries removed.
         */
        int cleanup();

        /**
         * @brief Notify DBus clients that this train's configuration has changed.
         * Called by OpticalTrainManager whenever the train data is updated.
         */
        void notifyUpdated();

    Q_SIGNALS:
        /** Emitted when the train configuration changes. */
        void updated();

        /** Emitted just before this train is removed. */
        void removed();

    private:
        /** @brief Helper to read one field from the manager for this train. */
        QVariant field(const QString &key) const;

        /** @brief Helper to write one field through the manager for this train. */
        bool setField(const QString &key, const QVariant &value);

        int     m_TrainID { -1 };
        QString m_DBusObjectPath;
};

} // namespace Ekos
