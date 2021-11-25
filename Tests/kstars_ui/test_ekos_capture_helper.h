/*
    Helper class of KStars UI capture tests

    SPDX-FileCopyrightText: 2020 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "config-kstars.h"
#include "test_ekos_debug.h"
#include "test_ekos_helper.h"
#include "test_ekos_simulator.h"
#include "test_ekos_capture.h"

#include "ekos/profileeditor.h"

#include <QObject>

#pragma once



class TestEkosCaptureHelper : public TestEkosHelper
{

public:

    explicit TestEkosCaptureHelper(QString guider = nullptr);

    /**
     * @brief Initialization ahead of executing the test cases.
     */
    void init() override;

    /**
     * @brief Cleanup after test cases have been executed.
     */
    void cleanup() override;

    /**
     * @brief Helper function for start of capturing
     * @param checkCapturing set to true if check of capturing should be included
     */
    bool startCapturing(bool checkCapturing = true);

    /**
     * @brief Helper function to stop capturing
     */
    bool stopCapturing();

    /**
     * @brief Fill the capture sequences in the Capture GUI
     * @param target capturing target name
     * @param sequence comma separated list of <filter>:<count>
     * @param exptime exposure time
     * @param fitsDirectory directory where the captures will be placed
     * @return true if everything was successful
     */
    bool fillCaptureSequences(QString target, QString sequence, double exptime, QString fitsDirectory);

    /**
     * @brief Fill the fields of the script manager in the capture module
     * @param scripts Pre-... and post-... scripts
     */
    bool fillScriptManagerDialog(const QMap<Ekos::ScriptTypes, QString> &scripts);

    /**
     * @brief Stop and clean up scheduler
     */
    void cleanupScheduler();

    /**
     * @brief calculateSignature Calculate the signature of a given filter
     * @param filter filter name
     * @return signature
     */
    QString calculateSignature(QString target, QString filter);

    QDir *getImageLocation();

    // destination where images will be located
    QTemporaryDir *destination;

private:
    QDir *imageLocation = nullptr;


};
