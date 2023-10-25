/*
    Helper class of KStars UI tests

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "config-kstars.h"
#include "test_ekos.h"
#include "test_ekos_debug.h"
#include "test_ekos_simulator.h"
#include "ekos/guide/guide.h"
#include "ekos/manager/meridianflipstate.h"

#include "indi/indidevice.h"
#include "indi/indigroup.h"
#include "indi/indidome.h"
#include "indi/indiproperty.h"
#include "indi/indielement.h"
#include "indi/guimanager.h"
#include "oal/oal.h"
#include "ekos/scheduler/scheduler.h"

#include <QObject>

#pragma once

/**
  * @brief Helper function to verify the execution of a statement.
  *
  * If the result is false, this function immediately returns with false,
  * otherwise simply continues.
  * Use this function in subroutines of test cases which should return bool.
  * @param statement expression to be verified
  * @return false if statement equals false, otherwise continuing
  */
#define KVERIFY_SUB(statement) \
do {\
    if (!QTest::qVerify(static_cast<bool>(statement), #statement, "", __FILE__, __LINE__))\
        return false;\
} while (false)

/**
  * @brief Subroutine version of QVERIFY2
  * @return false if statement equals false, otherwise continuing
  */
#define KVERIFY2_SUB(statement, description) \
do {\
    if (statement) {\
        if (!QTest::qVerify(true, #statement, (description), __FILE__, __LINE__))\
            return false;\
    } else {\
        if (!QTest::qVerify(false, #statement, (description), __FILE__, __LINE__))\
            return false;\
    }\
} while (false)

/**
  * @brief Helper macro to wrap statements with test macros that returns false if
  * the given statement preliminary invokes return (due to a test failure inside).
  * @return false if statement equals false, otherwise continuing
  */
#define KWRAP_SUB(statement) \
{ bool passed = false; \
    [&]() { statement; passed = true;}(); \
    if (!passed) return false; \
} while (false);

/**
  * @brief Subroutine version of QTRY_TIMEOUT_DEBUG_IMPL
  * @return false if expression equals false, otherwise continuing
  */
#define KTRY_TIMEOUT_DEBUG_IMPL_SUB(expr, timeoutValue, step)\
    if (!(expr)) { \
        QTRY_LOOP_IMPL((expr), (2 * timeoutValue), step);\
        if (expr) { \
            QString msg = QString::fromUtf8("QTestLib: This test case check (\"%1\") failed because the requested timeout (%2 ms) was too short, %3 ms would have been sufficient this time."); \
            msg = msg.arg(QString::fromUtf8(#expr)).arg(timeoutValue).arg(timeoutValue + qt_test_i); \
            KVERIFY2_SUB(false, qPrintable(msg)); \
        } \
    }

/**
  * @brief Subroutine version of QTRY_IMPL
  * @return false if expression equals false, otherwise continuing
  */
#define KTRY_IMPL_SUB(expr, timeout)\
    const int qt_test_step = 50; \
    const int qt_test_timeoutValue = timeout; \
    QTRY_LOOP_IMPL((expr), qt_test_timeoutValue, qt_test_step); \
    KTRY_TIMEOUT_DEBUG_IMPL_SUB((expr), qt_test_timeoutValue, qt_test_step)\

/**
  * @brief Subroutine version of QTRY_VERIFY_WITH_TIMEOUT
  * @param expr expression to be verified
  * @param timeout max time until the expression must become true
  * @return false if statement equals false, otherwise continuing
  */
#define KTRY_VERIFY_WITH_TIMEOUT_SUB(expr, timeout) \
    do { \
        KTRY_IMPL_SUB((expr), timeout);\
        KVERIFY_SUB(expr); \
    } while (false)

/** @brief Helper to retrieve a gadget from a certain module view.
 * @param module KStars module that holds the checkox
 * @param klass is the class of the gadget to look for.
 * @param name is the gadget name to look for in the UI configuration.
 * @warning Fails the test if the gadget "name" of class "klass" does not exist in the Mount module
 */
#define KTRY_GADGET(module, klass, name) klass * const name = module->findChild<klass*>(#name); \
    QTRY_VERIFY2_WITH_TIMEOUT(name != nullptr, QString(#klass " '%1' does not exist and cannot be used").arg(#name).toStdString().c_str(), 10000)

/** @brief Helper to retrieve a gadget from a certain module view (subroutine version).
 * @param module KStars module that holds the checkox
 * @param klass is the class of the gadget to look for.
 * @param name is the gadget name to look for in the UI configuration.
 * @warning Fails the test if the gadget "name" of class "klass" does not exist in the Mount module
 */
#define KTRY_GADGET_SUB(module, klass, name) klass * const name = module->findChild<klass*>(#name); \
    KVERIFY2_SUB(name != nullptr, QString(#klass " '%1' does not exist and cannot be used").arg(#name).toStdString().c_str())

/** @brief Retrieve an INDI element from the INDI controller
 *  @param device INDI device name
 *  @param group INDI group within the device
 *  @param name INDI property name within the group
 *  @param property resulting INDI property (@see #INDI_P)
 */
#define KTRY_INDI_PROPERTY(device, group, name, property) INDI_P * property = nullptr; \
    do { \
    INDI_D *indi_device = GUIManager::Instance()->findGUIDevice(device); \
    QVERIFY(indi_device != nullptr); \
    INDI_G *indi_group = indi_device->getGroup(group); \
    QVERIFY(indi_group != nullptr); \
    property = indi_group->getProperty(name); \
    } while (false); \
    QVERIFY(property != nullptr);

/** @brief Helper to find a top level window with a given title
 *  @param result resulting object name
 *  @param name window name
 */
#define KTRY_FIND_WINDOW(result, name) \
    QWindow * result = nullptr; \
    for (QWindow *window : qApp->topLevelWindows()) { \
    if (window->title() == name) { \
        result = window; break; } \
    } \
    QVERIFY(result != nullptr); QTRY_VERIFY_WITH_TIMEOUT(result->isVisible(), 1000); \


/** @brief Helper to click a button from a certain module view.
 * @param module KStars module that holds the checkox
 * @param button is the gadget name of the button to look for in the UI configuration.
 * @warning Fails the test if the button is not currently enabled.
 */
#define KTRY_CLICK(module, button) do { \
    QTimer::singleShot(100, Ekos::Manager::Instance(), [&]() { \
        KTRY_GADGET(module, QPushButton, button); \
        QVERIFY2(button->isEnabled(), QString("QPushButton '%1' is disabled and cannot be clicked").arg(#button).toStdString().c_str()); \
        QTest::mouseClick(button, Qt::LeftButton); }); \
    QTest::qWait(200); } while(false)

/** @brief Helper to click a button from a certain module view (subroutine version for KTRY_CLICK).
 * @param module KStars module that holds the checkox
 * @param button is the gadget name of the button to look for in the UI configuration.
 * @warning Fails the test if the button is not currently enabled.
 */
#define KTRY_CLICK_SUB(module, button) do { \
    bool success = false; \
    QTimer::singleShot(100, Ekos::Manager::Instance(), [&]() { \
        KTRY_GADGET(module, QPushButton, button); \
        QVERIFY2(button->isEnabled(), QString("QPushButton '%1' is disabled and cannot be clicked").arg(#button).toStdString().c_str()); \
        QTest::mouseClick(button, Qt::LeftButton); success = true;}); \
        KTRY_VERIFY_WITH_TIMEOUT_SUB(success, 1000);} while(false)

/** @brief Helper to set a checkbox and verify whether it succeeded
 * @param module KStars module that holds the checkox
 * @param checkbox object name of the checkbox
 * @param value value the checkbox should be set
 */
#define KTRY_SET_CHECKBOX(module, checkbox, value) \
    KTRY_GADGET(module, QCheckBox, checkbox); checkbox->setChecked(value); QVERIFY(checkbox->isChecked() == value)

/** @brief Subroutine version of @see KTRY_SET_CHECKBOX
 * @param module KStars module that holds the checkox
 * @param checkbox object name of the checkbox
 * @param value value the checkbox should be set
 */
#define KTRY_SET_CHECKBOX_SUB(module, checkbox, value) \
    KWRAP_SUB(KTRY_SET_CHECKBOX(module, checkbox, value))

/** @brief Helper to set a radiobutton and verify whether it succeeded
 * @param module KStars module that holds the radiobutton
 * @param checkbox object name of the radiobutton
 * @param value value the radiobutton should be set
 */
#define KTRY_SET_RADIOBUTTON(module, radiobutton, value) \
    KTRY_GADGET(module, QRadioButton, radiobutton); radiobutton->setChecked(value); QVERIFY(radiobutton->isChecked() == value)

/** @brief Subroutine version of @see KTRY_SET_RADIOBUTTON
 * @param module KStars module that holds the radiobutton
 * @param checkbox object name of the radiobutton
 * @param value value the radiobutton should be set
 */
#define KTRY_SET_RADIOBUTTON_SUB(module, radiobutton, value) \
    KWRAP_SUB(KTRY_SET_RADIOBUTTON(module, radiobutton, value))

/** @brief Helper to set a spinbox and verify whether it succeeded
 * @param module KStars module that holds the spinbox
 * @param spinbox object name of the spinbox
 * @param value value the spinbox should be set
 */
#define KTRY_SET_SPINBOX(module, spinbox, x) \
    KTRY_GADGET(module, QSpinBox, spinbox); spinbox->setValue(x); QVERIFY(spinbox->value() == x)

/** @brief Subroutine version of @see KTRY_SET_SPINBOX
 * @param module KStars module that holds the spinbox
 * @param spinbox object name of the spinbox
 * @param value value the spinbox should be set
 */
#define KTRY_SET_SPINBOX_SUB(module, spinbox, x) \
    KWRAP_SUB(KTRY_SET_SPINBOX(module, spinbox, x))

/** @brief Helper to set a doublespinbox and verify whether it succeeded
 * @param module KStars module that holds the spinbox
 * @param spinbox object name of the spinbox
 * @param value value the spinbox should be set
 */
#define KTRY_SET_DOUBLESPINBOX(module, spinbox, x) \
    KTRY_GADGET(module, QDoubleSpinBox, spinbox); spinbox->setValue(x); QVERIFY((spinbox->value() - x < 0.001))

/** @brief Subroutine version of @see KTRY_SET_DOUBLESPINBOX_SUB
 * @param module KStars module that holds the spinbox
 * @param spinbox object name of the spinbox
 * @param value value the spinbox should be set
 */
#define KTRY_SET_DOUBLESPINBOX_SUB(module, spinbox, x) \
    KWRAP_SUB(KTRY_SET_DOUBLESPINBOX(module, spinbox, x))

/** @brief Helper to set a combo box and verify whether it succeeded
 * @param module KStars module that holds the combo box
 * @param combo object name of the combo box
 * @param value value the combo box should be set
 */
#define KTRY_SET_COMBO(module, combo, value) \
    KTRY_GADGET(module, QComboBox, combo); combo->setCurrentText(value); QTRY_VERIFY_WITH_TIMEOUT(combo->currentText() == value, 1000)

/** @brief Helper to set a combo box by index and verify whether it succeeded
 * @param module KStars module that holds the combo box
 * @param combo object name of the combo box
 * @param value index the combo box should be selected
 */
#define KTRY_SET_COMBO_INDEX(module, combo, value) \
    KTRY_GADGET(module, QComboBox, combo); combo->setCurrentIndex(value); QVERIFY(combo->currentIndex() == value)

/** @brief Subroutine version of @see KTRY_SET_COMBO
 * @param module KStars module that holds the combo box
 * @param combo object name of the combo box
 * @param value value the combo box should be set
 */
#define KTRY_SET_COMBO_SUB(module, combo, value) \
    KWRAP_SUB(KTRY_GADGET(module, QComboBox, combo); combo->setCurrentText(value); QVERIFY(combo->currentText() == value))

/** @brief Helper to set a combo box by index and verify whether it succeeded
 * @param module KStars module that holds the combo box
 * @param combo object name of the combo box
 * @param value index the combo box should be selected
 */
#define KTRY_SET_COMBO_INDEX_SUB(module, combo, value) \
    KWRAP_SUB(KTRY_SET_COMBO_INDEX(module, combo, value))

/** @brief Helper to set a line edit box and verify whether it succeeded
 * @param module KStars module that holds the line edit box
 * @param spinbox object name of the line edit box
 * @param value value the line edit box should be set
 */
#define KTRY_SET_LINEEDIT(module, lineedit, value) \
    KTRY_GADGET(module, QLineEdit, lineedit); lineedit->setText(value); QVERIFY((lineedit->text() == value))

/** @brief Subroutine version of @see KTRY_SET_LINEEDIT
 * @param module KStars module that holds the line edit box
 * @param spinbox object name of the line edit box
 * @param value value the line edit box should be set
 */
#define KTRY_SET_LINEEDIT_SUB(module, lineedit, value) \
    KWRAP_SUB(KTRY_GADGET(module, QLineEdit, lineedit); lineedit->setText(value); QVERIFY((lineedit->text() == value)))

/**
  * @brief Helper to check whether a state queue is empty after the given delay
  * @param queue event queue
  * @param delay in milliseconds
  */
#define KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT(queue, delay) \
    if (! QTest::qWaitFor([&](){return queue.isEmpty();}, delay)) { \
    QString result("States not reached: "); \
    QTextStream stream(&result); \
    while (!(queue).isEmpty()) stream << (queue).dequeue(); \
    QFAIL(qPrintable(result));}

/**
  * @brief Helper to check whether a state queue is empty after the given delay
  * @param queue event queue
  * @param delay in milliseconds
  */
#define KVERIFY_EMPTY_QUEUE_WITH_TIMEOUT_SUB(queue, delay) \
    if (! QTest::qWaitFor([&](){return queue.isEmpty();}, delay)) { \
    QString result("States not reached: "); \
    QTextStream stream(&result); \
    while (!(queue).isEmpty()) stream << (queue).dequeue(); \
    QWARN(qPrintable(result)); return false;}

/**
  * @brief Helper to verify if the text in the text field starts with the given text
  * @param field UI Text field
  * @param text Text the text field should start with
  * @param timeout in ms
  */
#define KTRY_VERIFY_TEXTFIELD_STARTS_WITH_TIMEOUT_SUB(field, title, timeout) \
            KTRY_VERIFY_WITH_TIMEOUT_SUB(field->text().length() >= QString(title).length() && \
                                         field->text().left(QString(title).length()).compare(title) == 0, timeout)

/**
  * @brief Helper function for switching to a certain module
  * @param module target module
  * @param timeout in ms
  */
#define KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(module, timeout) do {\
    KTRY_EKOS_GADGET(QTabWidget, toolsWidget); \
    toolsWidget->setCurrentWidget(module); \
    QTRY_COMPARE_WITH_TIMEOUT(toolsWidget->currentWidget(), module, timeout);} while (false)

#define SET_INDI_VALUE_DOUBLE(device, group, property, value) do {\
    int result = QProcess::execute(QString("indi_setprop"), {QString("-n"), QString("%1.%2.%3=%4").arg(device).arg(group).arg(property).arg(value)});\
    qCInfo(KSTARS_EKOS_TEST) << "Process result code: " << result;\
    } while (false)

#define SET_INDI_VALUE_SWITCH(device, group, property, value) do {\
    int result = QProcess::execute(QString("indi_setprop"), {QString("-s"), QString("%1.%2.%3=%4").arg(device).arg(group).arg(property).arg(value==true?"On":"Off")});\
    qCInfo(KSTARS_EKOS_TEST) << "Process result code: " << result;\
    } while (false)

#define CLOSE_MODAL_DIALOG(button_nr) do { \
    QTimer::singleShot(1000, capture, [&]() { \
        QDialog * const dialog = qobject_cast <QDialog*> (QApplication::activeModalWidget()); \
        if (dialog) \
        { \
            QList<QPushButton*> pb = dialog->findChildren<QPushButton*>(); \
            QTest::mouseClick(pb[button_nr], Qt::MouseButton::LeftButton); \
        } \
    }); \
    } while (false)



class TestEkosHelper : public QObject
{
        Q_OBJECT
    public:
        explicit TestEkosHelper(QString guider = nullptr);

        // main optical train
        QString m_primaryTrain = i18n("Primary");
        QString m_guidingTrain = i18n("Secondary");

        // predefined set of scopes
        OAL::Scope *fsq85, *newton_10F4, *takfinder10x50;
        enum ScopeType {SCOPE_FSQ85, SCOPE_NEWTON_10F4, SCOPE_TAKFINDER10x50};
        // map scope type to predefined scope
        OAL::Scope *getScope(ScopeType type);

        // Mount device
        QString m_MountDevice = "";
        // CCD device
        QString m_CCDDevice = "";
        // Rotator device
        QString m_RotatorDevice = "";
        // Guiding device
        QString m_GuiderDevice = "";
        // Focusing device
        QString m_FocuserDevice = "";
        // Flat light panel device
        QString m_LightPanelDevice = "";
        // Dome device
        QString m_DomeDevice = "";

        // Guider (PHD2 or Internal)
        QString m_Guider = "Internal";

        // PHD2 setup (host and port)
        QProcess *phd2 { nullptr };
        QString const phd2_guider_host = "localhost";
        QString const phd2_guider_port = "4400";

        // guiding used?
        bool use_guiding = false;
        // did a dithering start?
        bool dithered = false;

        // sequence of alignment states that are expected
        QQueue<Ekos::AlignState> expectedAlignStates;
        // sequence of capture states that are expected
        QQueue<Ekos::CaptureState> expectedCaptureStates;
        // sequence of focus states that are expected
        QQueue<Ekos::FocusState> expectedFocusStates;
        // sequence of guiding states that are expected
        QQueue<Ekos::GuideState> expectedGuidingStates;
        // sequence of mount states that are expected
        QQueue<ISD::Mount::Status> expectedMountStates;
        // sequence of dome states that are expected
        QQueue<ISD::Dome::Status> expectedDomeStates;
        // sequence of meridian flip states that are expected
        QQueue<Ekos::MeridianFlipState::MeridianFlipMountState> expectedMeridianFlipStates;
        // sequence of scheduler states that are expected
        QQueue<Ekos::SchedulerState> expectedSchedulerStates;

        /**
         * @brief Initialization ahead of executing the test cases.
         */
        void virtual init();

        /**
         * @brief Cleanup after test cases have been executed.
         */
        void virtual cleanup();

        /**
         * @brief Fill mount, guider, CCD and focuser of an EKOS profile
         * @param isDone will be true if everything succeeds
         */
        void fillProfile(bool *isDone);

        /**
         * @brief create a new EKOS profile
         * @param name name of the profile
         * @param isPHD2 use internal guider or PHD2
         * @param isDone will be true if everything succeeds
         */
        void createEkosProfile(QString name, bool isPHD2, bool *isDone);

        /**
         * @brief Configure the EKOS profile
         * @param name of the profile
         * @param isPHD2 use internal guider or PHD2
         */
        bool setupEkosProfile(QString name, bool isPHD2);

        /**
         * @brief Start a test EKOS profile.
         */
        bool startEkosProfile();

        /**
         * @brief Shutdown the current test EKOS profile.
         */
        bool shutdownEkosProfile();


        /**
         * @brief Helper function starting PHD2
         */
        void startPHD2();

        /**
         * @brief Helper function stopping PHD2
         */
        void stopPHD2();

        /**
         * @brief Helper function for preparing the PHD2 test configuration
         */
        void preparePHD2();

        /**
         * @brief Helper function for cleaning up PHD2 test configuration
         */
        void cleanupPHD2();

        /**
         * @brief Prepare two optical trains for test purposes, one for capturing and one for guiding
         */
        void prepareOpticalTrains();

        /**
         * @brief Helper function that ensures that alignment works in a test environment
         */
        void prepareAlignmentModule();

        /**
         * @brief Helper function that ensures that capturing works in a test environment
         */
        void prepareCaptureModule();

        /**
         * @brief Helper function that ensures that focusing works in a test environment
         */
        void prepareFocusModule();

        /**
         * @brief Helper function that ensures that guiding works in a test environment
         */
        void prepareGuidingModule();

        /**
         * @brief Prepare the mount module with the given mount parameters
         */
        void prepareMountModule(ScopeType primary = SCOPE_FSQ85, ScopeType guiding = SCOPE_TAKFINDER10x50);

        /**
         * @brief Slew to a given position
         * @param fast set to true if slewing should be fast using sync first close to the position
         */
        bool slewTo(double RA, double DEC, bool fast);

        /**
         * @brief Helper function for start of guiding
         * @param guiding exposure time
         */
        bool startGuiding(double expTime);

        /**
         * @brief Helper function to stop guiding
         */
        bool stopGuiding();

        /**
         * @brief Start alignment process
         * @param expTime exposure time
         * @return true iff starting alignment succeeds
         */
        bool startAligning(double expTime);

        /**
         * @brief Check if astrometry files exist.
         * @return true iff astrometry files found
         */
        bool checkAstrometryFiles();

        /**
         * @brief Returns true iff astrometry is available and checks if necessary.
         */
        bool isAstrometryAvailable();

        /**
         * @brief Helper function for focusing
         * @param initialFocusPosition starting point for focusing
         */
        bool executeFocusing(int initialFocusPosition = 40000);

        /**
         * @brief Helper function to stop focusing
         */
        bool stopFocusing();

        /**
         * @brief Determine the number of seconds until the meridian flip should take place by reading
         *        the displayed meridian flip status.
         * @param message text containing the seconds to the meridian flip
         */
        int secondsToMF(QString message);

        /**
         * @brief update J2000 coordinates
         */
        void updateJ2000Coordinates(SkyPoint *target);

        /**
         * @brief Set a tree view combo to a given value
         * @param combo box with tree view
         * @param lookup target value
         */
        void setTreeviewCombo(QComboBox *combo, const QString lookup);

        /**
         * @brief Simple write-string-to-file utility.
         * @param filename name of the file to be created
         * @param lines file content
         */
        bool writeFile(const QString &filename, const QStringList &lines,
                       QFileDevice::Permissions permissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner);

        /**
         * @brief createCountingScript create a script that reads the number from its log file,
         * increases it by 1 and outputs it to the logfile. With this script we can count
         *  how often it has been executed.
         * @param scriptname full path to the script file
         * @param logfilename full path to the log file the script will produce
         * @return true iff file creation succeeds
         */
        bool createCountingScript(Ekos::ScriptTypes, const QString scriptname);

        /**
         * @brief createAllCaptureScripts Create all pre-/post job/capture scripts
         * @param directory where the scripts should be located
         */
        bool createAllCaptureScripts(QTemporaryDir *destination);

        /**
         * @brief countScriptRuns Utility that extracts the number of test script runs from its log file.
         * @param scripttype script type, its name will be extracted from {@see #scripts}
         * @return counter from the log file
         */
        int countScriptRuns(Ekos::ScriptTypes scripttype);

        /**
         * @brief checkScriptRuns Check if the configured pre- and post scripts have been running correctly
         * @param captures_per_sequence number of captures per sequence (all sequences must have the identical
         * number)
         * @param sequences number of capture sequences
         */
        bool checkScriptRuns(int captures_per_sequence, int sequences);

        /**
         * @brief Retrieve the current alignment status.
         */
        inline Ekos::AlignState getAlignStatus()
        {
            return m_AlignStatus;
        }
        /**
         * @brief Retrieve the current capture status.
         */
        inline Ekos::CaptureState getCaptureStatus()
        {
            return m_CaptureStatus;
        }
        /**
         * @brief Retrieve the current focus status.
         */
        inline Ekos::FocusState getFocusStatus()
        {
            return m_FocusStatus;
        }
        /**
         * @brief Retrieve the current guiding status.
         */
        Ekos::GuideState getGuidingStatus()
        {
            return m_GuideStatus;
        }
        /**
         * @brief Retrieve the current guide deviation
         */
        double getGuideDeviation()
        {
            return m_GuideDeviation;
        }
        /**
         * @brief Retrieve the current mount meridian flip status.
         */
        Ekos::MeridianFlipState::MeridianFlipMountState getMeridianFlipStatus()
        {
            return m_MFStatus;
        }
        /**
         * @brief Retrieve the current scheduler status.
         */
        Ekos::SchedulerState getSchedulerStatus()
        {
            return m_SchedulerStatus;
        }
        /**
         * @brief Retrieve the current dome status.
         */
        ISD::Dome::Status getDomeStatus()
        {
            return m_DomeStatus;
        }

        const QMap<Ekos::ScriptTypes, QString> &getScripts() const
        {
            return scripts;
        }
        /**
         * @brief Connect to read all modules state changes
         */
        void connectModules();

        /**
         * @brief Search for a certain scope in the database and create it if necessary
         * @param model scope model
         * @param vendor scope vendor
         * @param type scope type (refractor, newton, ...
         * @param aperture scope aperture in mm
         * @param focallenght focal length in mm
         * @return scope object
         */
        OAL::Scope *createScopeIfNecessary(QString model, QString vendor, QString type, double aperture, double focallenght);

private:
        // current mount status
        ISD::Mount::Status m_MountStatus { ISD::Mount::MOUNT_IDLE };

        // current mount meridian flip status
        Ekos::MeridianFlipState::MeridianFlipMountState m_MFStatus { Ekos::MeridianFlipState::MOUNT_FLIP_NONE };

        // current alignment status
        Ekos::AlignState m_AlignStatus { Ekos::ALIGN_IDLE };

        // current capture status
        Ekos::CaptureState m_CaptureStatus { Ekos::CAPTURE_IDLE };

        // current focus status
        Ekos::FocusState m_FocusStatus { Ekos::FOCUS_IDLE };

        // current guiding status
        Ekos::GuideState m_GuideStatus { Ekos::GUIDE_IDLE };

        // current dome status
        ISD::Dome::Status m_DomeStatus { ISD::Dome::DOME_IDLE };

        // current guiding deviation
        double m_GuideDeviation { -1 };

        // current scheduler status
        Ekos::SchedulerState m_SchedulerStatus { Ekos::SCHEDULER_IDLE };

        // pre- and post sequence / capture execution scripts
        QMap<Ekos::ScriptTypes, QString> scripts;

        /**
         * @brief Slot to track the align status of the mount
         * @param status new align state
         */
        void alignStatusChanged(Ekos::AlignState status);

        /**
         * @brief Slot to track the mount status
         * @param status new mount status
         */
        void mountStatusChanged(ISD::Mount::Status status);

        /**
         * @brief Slot to track the meridian flip stage of the mount
         * @param status new meridian flip state
         */
        void meridianFlipStatusChanged(Ekos::MeridianFlipState::MeridianFlipMountState status);

        /**
         * @brief Slot to track the focus status
         * @param status new focus status
         */
        void focusStatusChanged(Ekos::FocusState status);

        /**
         * @brief Slot to track the guiding status
         * @param status new guiding status
         */
        void guidingStatusChanged(Ekos::GuideState status);

        /**
         * @brief Slot to track guiding deviation events
         * @param delta_ra RA deviation
         * @param delta_dec DEC deviation
         */
        void guideDeviationChanged(double delta_ra, double delta_dec);

        /**
         * @brief Slot to track the capture status
         * @param status new capture status
         */
        void captureStatusChanged(Ekos::CaptureState status);

        /**
         * @brief Slot to track the scheduler status
         * @param status new scheduler status
         */
        void schedulerStatusChanged(Ekos::SchedulerState status);

        /**
         * @brief Slot to track the dome status
         * @param status new scheduler status
         */
        void domeStatusChanged(ISD::Dome::Status status);

    private:
        bool m_astrometry_available;

};
