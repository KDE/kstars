/*  mock ekos modules
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef MOCKMODULES_H
#define MOCKMODULES_H

#include <QString>
#include <QList>
#include <QStringList>
#include <QtDBus/QtDBus>
#include "ekos/ekos.h"
#include "indi/inditelescope.h"

// These classes mock Ekos modules for use in testing the scheduler
// in test_ekos_scheduler_ops.cpp.  They perform minimal functions
// and are only partially implemented--that is, they only have methods
// that are actually called by the scheduler. If scheduler functionality
// is added, then more mock methods may need to be created, or methods extended.
// They communicate with the scheduler with the same dbus calls as the real modules.
//
// Note that MockEkos uses a different path and interface than the actual
// scheduler (to avoid name conflicts with the adaptor class) so it use it
// the scheduler contstructor that takes path and interface needs to be used.
// Scheduler scheduler("/KStars/MockEkos", "org.kde.kstars.MockEkos");

namespace Ekos
{

// MockFocus returns status of either FOCUS_PROGRESS or FOCUS_IDLE
// or whatever is passed to setStatus(). It is commanded by the status call.
// It signals newStatus() if the status is changed.
// Status can be changed via start(), abort(), or setStatus(status).
class MockFocus : public QObject
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.MockEkos.MockFocus")
        Q_PROPERTY(Ekos::FocusState status READ status NOTIFY newStatus)
    public:
        MockFocus();

        Q_SCRIPTABLE Q_NOREPLY void start()
        {
            fprintf(stderr, "MockFocus::start called\n");
            setStatus(Ekos::FOCUS_PROGRESS);
        }
        Q_SCRIPTABLE Q_NOREPLY void abort()
        {
            fprintf(stderr, "MockFocus::abort called\n");
            setStatus(Ekos::FOCUS_IDLE);
        }
        Q_SCRIPTABLE Q_NOREPLY bool canAutoFocus()
        {
            fprintf(stderr, "MockFocus::canAutoFocus called\n");
            return true;
        }

        // Seems like the calibrationAutoStar is the one that's used!
        Q_SCRIPTABLE Q_NOREPLY void setAutoStarEnabled(bool enable)
        {
            Q_UNUSED(enable);
            fprintf(stderr, "MockFocus::setAutoStarEnabled called\n");
        }

        Q_SCRIPTABLE Q_NOREPLY void resetFrame()
        {
            fprintf(stderr, "MockFocus::ResetFrame called\n");
            isReset = true;
        }

        bool isReset = false;

        Q_SCRIPTABLE Ekos::FocusState status()
        {
            return m_Status;
        }

        void setStatus(Ekos::FocusState status)
        {
            m_Status = status;
            emit newStatus(m_Status);
        }

        static const QString mockPath;

    public slots:

    signals:
        void newStatus(Ekos::FocusState state);
    private:
        Ekos::FocusState m_Status = Ekos::FOCUS_IDLE;
};

// MockMount returns status() of either MOUNT_SLEWING, MOUNT_IDLE
// or whatever is passed to setStatus().
// They are commanded by slew(), abort() & setStatus() and signaled by newStatus().
// It also returns parkStatus() of either PARK_PARKED or PARK_UNPARKED
// or whatever is passed to setParkStatus().
// Commanded by park(), unpark(), setParkStatus() & signaled by newParkStatus().
// It can also signal ready() when sendRead() is called.
class MockMount : public QObject
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.MockEkos.MockMount")
        Q_PROPERTY(ISD::Telescope::Status status READ status NOTIFY newStatus)
        Q_PROPERTY(ISD::ParkStatus parkStatus READ parkStatus NOTIFY newParkStatus)
        Q_PROPERTY(bool canPark READ canPark)
    public:
        MockMount();

        ISD::Telescope::Status status() const
        {
            return m_Status;
        }
        Q_INVOKABLE Q_SCRIPTABLE ISD::ParkStatus parkStatus() const
        {
            return m_ParkStatus;
        }
        Q_INVOKABLE Q_SCRIPTABLE bool slew(double RA, double DEC)
        {
            fprintf(stderr, "%d @@@MockMount::slew(%f,%f)\n", __LINE__, RA, DEC);
            setStatus(ISD::Telescope::MOUNT_SLEWING);
            return true;
        }
        Q_INVOKABLE Q_SCRIPTABLE bool abort()
        {
            fprintf(stderr, "%d @@@MockMount::abort()\n", __LINE__);
            setStatus(ISD::Telescope::MOUNT_IDLE);
            return true;
        }
        Q_INVOKABLE Q_SCRIPTABLE bool resetModel()
        {
            return true;
        }
        Q_INVOKABLE Q_SCRIPTABLE bool canPark()
        {
            return true;
        }
        Q_INVOKABLE Q_SCRIPTABLE bool park()
        {
            fprintf(stderr, "%d @@@MockMount::park\n", __LINE__);
            setParkStatus(ISD::PARK_PARKED);
            return true;
        }
        Q_INVOKABLE Q_SCRIPTABLE bool unpark()
        {
            fprintf(stderr, "%d @@@MockMount::park\n", __LINE__);
            setParkStatus(ISD::PARK_UNPARKED);
            return true;
        }

        void sendReady()
        {
            emit ready();
        }
        void setStatus(ISD::Telescope::Status status)
        {
            m_Status = status;
            emit newStatus(m_Status);
        }
        void setParkStatus(ISD::ParkStatus parkStatus)
        {
            m_ParkStatus = parkStatus;
            emit newParkStatus(parkStatus);
        }

        static const QString mockPath;

    signals:
        void newStatus(ISD::Telescope::Status status);
        void ready();
        void newParkStatus(ISD::ParkStatus status);

    public slots:
        void setMeridianFlipValues(bool activate, double hours)
        {
            Q_UNUSED(activate);
            Q_UNUSED(hours);
        }
    private:
        ISD::Telescope::Status m_Status = ISD::Telescope::MOUNT_IDLE;
        ISD::ParkStatus m_ParkStatus = ISD::PARK_UNPARKED;
};

// MockCapture returns status() of CAPTURE_CAPTURING, CAPTURE_ABORTED, CAPTURE_IDLE
// or whatever is passed to setStatus().
// They are commanded by start(), abort() & setStatus() and signaled by newStatus().
// It also has a coolerControl and targetName property.
class MockCapture : public QObject
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.MockEkos.MockCapture")
        Q_PROPERTY(Ekos::CaptureState status READ status NOTIFY newStatus)
        Q_PROPERTY(QString targetName MEMBER m_TargetName)
        Q_PROPERTY(bool coolerControl READ hasCoolerControl WRITE setCoolerControl)


    public:
        MockCapture();
        Q_SCRIPTABLE Q_NOREPLY void clearAutoFocusHFR() {}
        Q_SCRIPTABLE bool loadSequenceQueue(const QString &fileURL)
        {
            fprintf(stderr, "%d @@@MockCapture::loadSequenceQueue(%s)\n", __LINE__, fileURL.toLatin1().data());
            m_fileURL = fileURL;
            return true;
        }
        Q_SCRIPTABLE Q_NOREPLY void setCapturedFramesMap(const QString &signature, int count)
        {
            Q_UNUSED(signature);
            Q_UNUSED(count);
            fprintf(stderr, "%d @@@MockCapture::setCapturedFramesMap(%s,%d)\n", __LINE__, signature.toLatin1().data(), count);
        }
        Q_SCRIPTABLE Ekos::CaptureState status()
        {
            return m_Status;
        }

        Q_SCRIPTABLE bool hasCoolerControl()
        {
            return m_CoolerControl;
        }
        Q_SCRIPTABLE bool setCoolerControl(bool value)
        {
            m_CoolerControl = value;
            return true;
        }

        void sendReady()
        {
            emit ready();
        }
        void setStatus(Ekos::CaptureState status)
        {
            m_Status = status;
            emit newStatus(m_Status);
        }
        QString m_fileURL;

        static const QString mockPath;

    public slots:

        Q_SCRIPTABLE Q_NOREPLY void abort()
        {
            fprintf(stderr, "%d @@@MockCapture::abort()\n", __LINE__);
            setStatus(CAPTURE_ABORTED);
        }
        Q_SCRIPTABLE Q_NOREPLY void start()
        {
            fprintf(stderr, "%d @@@MockCapture::start()\n", __LINE__);
            setStatus(CAPTURE_CAPTURING);
        }

    signals:
        Q_SCRIPTABLE void newStatus(Ekos::CaptureState status);
        void ready();

    private:
        QString m_TargetName;
        Ekos::CaptureState m_Status { CAPTURE_IDLE };
        bool m_CoolerControl { false };
};

// MockAlign returns status() of ALIGN_PROGRESS or whatever is passed to setStatus().
// They are commanded by captureAndSolve(), loadAndSlew(), abort() & setStatus().
// It is signaled by newStatus().
class MockAlign : public QObject
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.MockEkos.MockAlign")
        Q_PROPERTY(Ekos::AlignState status READ status NOTIFY newStatus)

    public:
        MockAlign();

        Q_SCRIPTABLE Ekos::AlignState status()
        {
            return m_Status;
        }
        Q_SCRIPTABLE Q_NOREPLY void setSolverAction(int action)
        {
            fprintf(stderr, "%d @@@MockAlign::setSolverAction(%d)\n", __LINE__, action);
        }
        void setStatus(Ekos::AlignState status)
        {
            m_Status = status;
            emit newStatus(m_Status);
        }

        static const QString mockPath;

    public slots:
        Q_SCRIPTABLE Q_NOREPLY void abort()
        {
            fprintf(stderr, "%d @@@MockAlign::abort()\n", __LINE__);
            setStatus(ALIGN_IDLE);
        }
        Q_SCRIPTABLE bool captureAndSolve()
        {
            fprintf(stderr, "%d @@@MockAlign::captureAndSolve()\n", __LINE__);
            setStatus(ALIGN_PROGRESS);
            return true;
        }
        Q_SCRIPTABLE bool loadAndSlew(QString fileURL = QString())
        {
            Q_UNUSED(fileURL);
            fprintf(stderr, "%d @@@MockAlign::loadAndSlew(fileURL)\n", __LINE__);
            setStatus(ALIGN_PROGRESS);
            return true;
        }
        Q_SCRIPTABLE Q_NOREPLY void setTargetCoords(double ra, double dec)
        {
            fprintf(stderr, "%d @@@MockAlign::setTargetCoords(%f,%f)\n", __LINE__, ra, dec);
            targetRa = ra;
            targetDec = dec;
        }
        Q_SCRIPTABLE Q_NOREPLY void setTargetRotation(double rotation)
        {
            fprintf(stderr, "%d @@@MockAlign::setTargetRotation(%f)\n", __LINE__, rotation);
            targetRotation = rotation;
            Q_UNUSED(rotation);
        }
        bool loadAndSlew(const QByteArray &image, const QString &extension)
        {
            Q_UNUSED(image);
            Q_UNUSED(extension);
            fprintf(stderr, "%d @@@MockAlign::loadAndSlew(image,extension)\n", __LINE__);
            setStatus(ALIGN_PROGRESS);
            return true;
        }

    signals:
        void newStatus(Ekos::AlignState state);

    private:
        Ekos::AlignState m_Status {ALIGN_IDLE};
        int action { 0 };
        double targetRa { 0 };
        double targetDec { 0 };
        double targetRotation { 0 };
};

// MockGuide returns status() of GUIDE_GUIDING, GUIDE_ABORTED, or GUIDE_IDLE
// or whatever is passed to setStatus().
// They are commanded by guide(), abort() & setStatus() and  signaled by newStatus().
class MockGuide : public QObject
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.MockEkos.MockGuide")
        Q_PROPERTY(Ekos::GuideState status READ status NOTIFY newStatus)

    public:
        MockGuide();

        Q_SCRIPTABLE bool connectGuider()
        {
            fprintf(stderr, "%d @@@MockGuide::connectGuider()\n", __LINE__);
            connected = true;
            return true;
        }
        Q_SCRIPTABLE Q_NOREPLY void setCalibrationAutoStar(bool enable)
        {
            fprintf(stderr, "%d @@@MockGuide::setCalibrationAutoStar()\n", __LINE__);
            calAutoStar = enable;
            Q_UNUSED(enable);
        }
        Q_SCRIPTABLE Ekos::GuideState status()
        {
            return m_Status;
        }
        void setStatus(Ekos::GuideState status)
        {
            m_Status = status;
            emit newStatus(m_Status);
        }
        bool connected { false };

        // I believe this is really autoStar in general!
        bool calAutoStar { false };

        static const QString mockPath;

    public slots:
        Q_SCRIPTABLE Q_NOREPLY void clearCalibration() {}
        Q_SCRIPTABLE bool guide()
        {
            fprintf(stderr, "%d @@@MockGuide::guide()\n", __LINE__);
            setStatus(Ekos::GUIDE_GUIDING);
            return true;
        }
        Q_SCRIPTABLE bool abort()
        {
            setStatus(Ekos::GUIDE_ABORTED);
            return true;
        }

    signals:
        void newStatus(Ekos::GuideState status);

    private:
        Ekos::GuideState m_Status {GUIDE_IDLE};
};

// MockEkos has 2 different statuses.
// ekosStatus is commanded by start() --> Success, and stop() --> Idle and
// setEkosStatus(). It signals ekosStatusChanged.
// indiStatus is commanded by connectDevices() --> Success, and
// disconnectDevives() --> Idle and setIndiStatus().
// It signals indiStatusChanged.
// It also has an addModule() method which signals newModule().
// A scheduler using this mock should be constructed as:
// Scheduler scheduler("/KStars/MockEkos", "org.kde.kstars.MockEkos");
class MockEkos : public QObject
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.MockEkos")
        Q_PROPERTY(Ekos::CommunicationStatus indiStatus READ indiStatus NOTIFY indiStatusChanged)
        Q_PROPERTY(Ekos::CommunicationStatus ekosStatus READ ekosStatus NOTIFY ekosStatusChanged)

    public:
        MockEkos();
        Q_SCRIPTABLE void start()
        {
            fprintf(stderr, "%d @@@MockEkos::start()\n", __LINE__);
            setEkosStatus(Ekos::Success);
        }
        Q_SCRIPTABLE void stop()
        {
            fprintf(stderr, "%d @@@MockEkos::stop()\n", __LINE__);
            setEkosStatus(Ekos::Idle);
        }
        Q_SCRIPTABLE QStringList getProfiles()
        {
            fprintf(stderr, "%d @@@MockEkos::getProfiles()\n", __LINE__);
            return QStringList();
        }
        Q_SCRIPTABLE bool setProfile(const QString &profileName)
        {
            Q_UNUSED(profileName);
            fprintf(stderr, "%d @@@MockEkos::setProfile()\n", __LINE__);
            return true;
        }
        Q_SCRIPTABLE CommunicationStatus indiStatus()
        {
            fprintf(stderr, "%d @@@MockEkos::indiStatus\n", __LINE__);
            return m_INDIStatus;
        }
        Q_SCRIPTABLE CommunicationStatus ekosStatus()
        {
            fprintf(stderr, "%d @@@MockEkos::ekosStatus\n", __LINE__);
            return m_EkosStatus;
        }
        void setEkosStatus(Ekos::CommunicationStatus status)
        {
            m_EkosStatus = status;
            emit ekosStatusChanged(status);
        }
        void setIndiStatus(Ekos::CommunicationStatus status)
        {
            m_INDIStatus = status;
            emit indiStatusChanged(status);
        }
        void addModule(const QString &name)
        {
            emit newModule(name);
        }

        static const QString mockPath;

    signals:
        void ekosStatusChanged(Ekos::CommunicationStatus status);
        void indiStatusChanged(Ekos::CommunicationStatus status);
        void newModule(const QString &name);

    protected:
    public slots:
        Q_SCRIPTABLE Q_NOREPLY void connectDevices()
        {
            fprintf(stderr, "%d @@@MockEkos::connectDevices\n", __LINE__);
            setIndiStatus(Ekos::Success);
        }
        Q_SCRIPTABLE Q_NOREPLY void disconnectDevices()
        {
            fprintf(stderr, "%d @@@MockEkos::disconnectDevices\n", __LINE__);
            setIndiStatus(Ekos::Idle);
        }
    private slots:
    private:
        Ekos::CommunicationStatus m_EkosStatus {Ekos::Idle};
        Ekos::CommunicationStatus m_INDIStatus {Ekos::Idle};
};

}  // namespace Ekos

#endif // MOCKMODULES_H
