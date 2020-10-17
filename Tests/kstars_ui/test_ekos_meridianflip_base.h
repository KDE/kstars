/*
    KStars UI tests for meridian flip

    Copyright (C) 2020
    Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "config-kstars.h"
#include "test_ekos_debug.h"

#if defined(HAVE_INDI)

#include <QObject>
#include <QPushButton>
#include <QQuickItem>
#include <QtTest>
#include <QQueue>

#include "ekos/align/align.h"
#include "ekos/guide/guide.h"
#include "ekos/mount/mount.h"
#include "ekos/profileeditor.h"

#include "test_ekos_simulator.h"
#include "test_ekos_capture.h"

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
    QVERIFY2(name != nullptr, QString(#klass " '%1' does not exist and cannot be used").arg(#name).toStdString().c_str())

/** @brief Helper to retrieve a gadget from a certain module view (subroutine version).
 * @param module KStars module that holds the checkox
 * @param klass is the class of the gadget to look for.
 * @param name is the gadget name to look for in the UI configuration.
 * @warning Fails the test if the gadget "name" of class "klass" does not exist in the Mount module
 */
#define KTRY_GADGET_SUB(module, klass, name) klass * const name = module->findChild<klass*>(#name); \
    KVERIFY2_SUB(name != nullptr, QString(#klass " '%1' does not exist and cannot be used").arg(#name).toStdString().c_str())

/** @brief Helper to click a button from a certain module view.
 * @param module KStars module that holds the checkox
 * @param button is the gadget name of the button to look for in the UI configuration.
 * @warning Fails the test if the button is not currently enabled.
 */
#define KTRY_CLICK(module, button) do { \
    QTimer::singleShot(100, Ekos::Manager::Instance(), []() { \
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

/** @brief Helper to set a checkbox and verify whether it succeeded (subroutine version)
 * @param module KStars module that holds the checkox
 * @param checkbox object name of the checkbox
 * @param value value the checkbox should be set
 */
#define KTRY_SET_CHECKBOX_SUB(module, checkbox, value) \
    KWRAP_SUB(KTRY_GADGET(module, QCheckBox, checkbox); checkbox->setChecked(value); QVERIFY(checkbox->isChecked() == value))

/** @brief Helper to set a radiobutton and verify whether it succeeded
 * @param module KStars module that holds the radiobutton
 * @param checkbox object name of the radiobutton
 * @param value value the radiobutton should be set
 */
#define KTRY_SET_RADIOBUTTON_SUB(module, radiobutton, value) \
    KWRAP_SUB(KTRY_GADGET(module, QRadioButton, radiobutton); radiobutton->setChecked(value); QVERIFY(radiobutton->isChecked() == value))

/** @brief Helper to set a spinbox and verify whether it succeeded
 * @param module KStars module that holds the spinbox
 * @param spinbox object name of the spinbox
 * @param value value the spinbox should be set
 */
#define KTRY_SET_SPINBOX_SUB(module, spinbox, x) \
    KWRAP_SUB(KTRY_GADGET(module, QSpinBox, spinbox); spinbox->setValue(x); QVERIFY(spinbox->value() == x))

/** @brief Helper to set a doublespinbox and verify whether it succeeded
 * @param module KStars module that holds the spinbox
 * @param spinbox object name of the spinbox
 * @param value value the spinbox should be set
 */
#define KTRY_SET_DOUBLESPINBOX_SUB(module, spinbox, x) \
    KWRAP_SUB(KTRY_GADGET(module, QDoubleSpinBox, spinbox); spinbox->setValue(x); QVERIFY((spinbox->value() - x < 0.001)))

/** @brief Helper to set a combo box and verify whether it succeeded
 * @param module KStars module that holds the combo box
 * @param combo object name of the combo box
 * @param value value the combo box should be set
 */
#define KTRY_SET_COMBO(module, combo, value) \
    KTRY_GADGET(module, QComboBox, combo); combo->setCurrentText(value); QVERIFY(combo->currentText() == value)

/** @brief Subroutine version of @see KTRY_SET_COMBO
 * @param module KStars module that holds the combo box
 * @param combo object name of the combo box
 * @param value value the combo box should be set
 */
#define KTRY_SET_COMBO_SUB(module, combo, value) \
    KWRAP_SUB(KTRY_GADGET(module, QComboBox, combo); combo->setCurrentText(value); QVERIFY(combo->currentText() == value))

/** @brief Helper to set a combo box by indexand verify whether it succeeded
 * @param module KStars module that holds the combo box
 * @param combo object name of the combo box
 * @param value index the combo box should be selected
 */
#define KTRY_SET_COMBO_INDEX_SUB(module, combo, value) \
    KWRAP_SUB(KTRY_GADGET(module, QComboBox, combo); combo->setCurrentIndex(value); QVERIFY(combo->currentIndex() == value))

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
  * @brief Helper to verify if the text in the text field starts with the given text
  * @param field UI Text field
  * @param text Text the text field should start with
  * @param timeout in ms
  */
#define KTRY_VERIFY_TEXTFIELD_STARTS_WITH_TIMEOUT_SUB(field, title, timeout) \
            KTRY_VERIFY_WITH_TIMEOUT_SUB(field->text().length() >= QString(title).length() && \
                                         field->text().left(QString(title).length()).compare(title) == 0, timeout)

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
  * @brief Helper function for switching to a certain module
  * @param module target module
  * @param timeout in ms
  */
#define KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(module, timeout) do {\
    KTRY_EKOS_GADGET(QTabWidget, toolsWidget); \
    toolsWidget->setCurrentWidget(module); \
    QTRY_COMPARE_WITH_TIMEOUT(toolsWidget->currentWidget(), module, timeout);} while (false)


#define QTEST_KSTARS_MAIN_GUIDERSELECT(klass) \
int main(int argc, char *argv[]) { \
    QString guider = "Internal"; \
    std::vector<char *> testargv; \
    bool showfunctions = false; \
    for (int i = 0; i < argc; i++) { \
        if (!strcmp("-functions", argv[i])) \
        {testargv.push_back(argv[i]); showfunctions = true;} \
        else if (!strcmp("-guider", argv[i]) && i+1 < argc) \
        { guider = argv[i+1]; i++; } \
        else testargv.push_back(argv[i]); } \
    klass tc(guider); \
    if (showfunctions) return QTest::qExec(&tc, testargv.size(), testargv.data()); \
    QApplication app(argc, argv); \
    prepare_tests(); \
    int failure = 0; \
    QTimer::singleShot(1000, &app, [&] { \
        qDebug("Starting tests..."); \
        failure |= run_wizards(testargv.size(), testargv.data()); \
        if (!failure) { failure |= QTest::qExec(&tc, testargv.size(), testargv.data()); } \
        qDebug("Tests are done."); \
        app.quit(); }); \
    execute_tests(); \
    return failure; }


class TestEkosMeridianFlipBase : public QObject
{
    Q_OBJECT

public:
    explicit TestEkosMeridianFlipBase(QObject *parent = nullptr);
    explicit TestEkosMeridianFlipBase(QString guider, QObject *parent = nullptr);

protected:
    /**
     * @brief Configure the EKOS profile
     * @param name of the profile
     * @param isPHD2 use internal guider or PHD2
     */
    bool setupEkosProfile(QString name, bool isPHD2);

    /**
     * @brief create a new EKOS profile
     * @param name name of the profile
     * @param isPHD2 use internal guider or PHD2
     * @param isDone will be true if everything succeeds
     */
    void createEkosProfile(QString name, bool isPHD2, bool *isDone);

    /**
     * @brief Start a test EKOS profile.
     */
    bool startEkosProfile();

    /**
     * @brief Shutdown the current test EKOS profile.
     */
    bool shutdownEkosProfile(QString guider);

    /**
     * @brief Fill mount, guider, CCD and focuser of an EKOS profile
     * @param isDone will be true if everything succeeds
     */
    void fillProfile(bool *isDone);

    /**
     * @brief Set a tree view combo to a given value
     * @param combo box with tree view
     * @param lookup target value
     */
    void setTreeviewCombo(QComboBox *combo, const QString lookup);

    /**
     * @brief Enable the meridian flip on the mount tab
     * @param delay seconds delay of the meridian flip
     */
    bool enableMeridianFlip(double delay);

    /**
     * @brief Position the mount so that the meridian will be reached in certain timeframe
     * @param secsToMF seconds until the meridian will be crossed
     * @param fast use sync for fast positioning
     */
    bool positionMountForMF(int secsToMF, bool fast = true);

    /**
     * @brief Check if meridian flip runs and completes
     * @param startDelay upper limit for expected time in seconds that are expected the flip to start
     */
    bool checkMFExecuted(int startDelay);

    /**
     * @brief Determine the number of seconds until the meridian flip should take place by reading
     *        the displayed meridian flip status.
     * @return
     */
    int secondsToMF();

    /**
     * @brief Helper function that reads capture sequence test data, creates entries in the capture module,
     *        executes upfront focusing if necessary and positions the mount close to the meridian.
     * @param secsToMF seconds until the meridian will be crossed
     * @param calibrate execute initial guiding to calibrate the guider
     * @param initialFocus execute upfront focusing
     */
    bool prepareCaptureTestcase(int secsToMF, bool calibrate, bool initialFocus);

    /**
     * @brief Prepare test data iterating over all combination of parameters.
     * @param filterList variants of filter parameter tests
     * @param focusList variants with/without focus tests
     * @param autofocusList variants with/without HFR autofocus tests
     * @param ditherList variants with/without dithering tests
     */
    void prepareTestData(QList<QString> filterList, QList<bool> focusList, QList<bool> autofocusList, QList<bool> ditherList);

    /**
     * @brief Check if astrometry files exist.
     * @return true iff astrometry files found
     */
    bool checkAstrometryFiles();

    /**
     * @brief Check if guiding and dithering is restarted if required.
     */
    bool checkDithering();

    /**
     * @brief Check if re-focusing is issued if required.
     * @param is guiding on?
     */
    bool checkRefocusing(bool guiding);

    /**
     * @brief Helper function for start of alignment
     * @param exposure time
     */
    bool startAligning(double expTime);

    /**
     * @brief Helper function to stop guiding
     */
    bool stopAligning();

    /**
     * @brief Helper function for start of capturing
     */
    bool startCapturing();

    /**
     * @brief Helper function to stop capturing
     */
    bool stopCapturing();

    /**
     * @brief Helper function for start focusing
     */
    bool startFocusing();

    /**
     * @brief Helper function to stop focusing
     */
    bool stopFocusing();

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


    // Mount device
    QString m_MountDevice = "Telescope Simulator";
    // CCD device
    QString m_CCDDevice = "CCD Simulator";
    // Guiding device
    QString m_GuiderDevice = "Guide Simulator";
    // Focusing device
    QString m_FocuserDevice = "Focuser Simulator";
    // Guider (PHD2 or Internal)
    QString m_Guider = "Internal";

    // PHD2 setup (host and port)
    QProcess *phd2;
    QString const phd2_guider_host = "localhost";
    QString const phd2_guider_port = "4400";

    // current mount meridian flip status
    Ekos::Mount::MeridianFlipStatus m_MFStatus;
    // sequence of alignment states that are expected
    QQueue<Ekos::AlignState> expectedAlignStates;
    // sequence of capture states that are expected
    QQueue<Ekos::CaptureState> expectedCaptureStates;
    // sequence of focus states that are expected
    QQueue<Ekos::FocusState> expectedFocusStates;
    // sequence of guiding states that are expected
    QQueue<Ekos::GuideState> expectedGuidingStates;
    // sequence of meridian flip states that are expected
    QQueue<Ekos::Mount::MeridianFlipStatus> expectedMeridianFlipStates;

    // regular focusing on?
    bool refocus_checked = false;
    // HFR autofocus on?
    bool autofocus_checked = false;
    // regular dithering on?
    bool dithering_checked = false;

    /**
     * @brief Retrieve the current alignment status.
     */
    inline Ekos::AlignState getAlignStatus() {return m_AlignStatus;}
    /**
     * @brief Retrieve the current capture status.
     */
    inline Ekos::CaptureState getCaptureStatus() {return m_CaptureStatus;}
    /**
     * @brief Retrieve the current focus status.
     */
    inline Ekos::FocusState getFocusStatus() {return m_FocusStatus;}
    /**
     * @brief Retrieve the current guiding status.
     */
    Ekos::GuideState getGuidingStatus() {return m_GuideStatus;};
    /**
     * @brief Retrieve the current mount meridian flip status.
     */
    Ekos::Mount::MeridianFlipStatus getMeridianFlipStatus() {return m_MFStatus;};

        
protected slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();


private:
    // current alignment status
    Ekos::AlignState m_AlignStatus;

    // current capture status
    Ekos::CaptureState m_CaptureStatus;

    // current focus status
    Ekos::FocusState m_FocusStatus;

    // current guiding status
    Ekos::GuideState m_GuideStatus;

    /**
     * @brief Slot to track the align status of the mount
     * @param status new align state
     */
    void alignStatusChanged(Ekos::AlignState status);

    /**
     * @brief Slot to track the meridian flip stage of the mount
     * @param status new meridian flip state
     */
    void meridianFlipStatusChanged(Ekos::Mount::MeridianFlipStatus status);

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
     * @brief Slot to track the capture status
     * @param status new capture status
     */
    void captureStatusChanged(Ekos::CaptureState status);

};

#endif // HAVE_INDI

