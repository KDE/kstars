/*  KStars UI tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TESTEKOSCAPTURE_H
#define TESTEKOSCAPTURE_H

#include "config-kstars.h"

#if defined(HAVE_INDI)

#include "test_ekos_capture_helper.h"

#include <QObject>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTest>


class TestEkosCapture : public QObject
{
    Q_OBJECT

public:
    explicit TestEkosCapture(QObject *parent = nullptr);

private:
    TestEkosCaptureHelper *m_CaptureHelper = new TestEkosCaptureHelper();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void init();
    void cleanup();

    /** @brief Test addition, UI sync and removal of capture jobs. */
    void testAddCaptureJob();

    /** @brief Test that storing to a system temporary folder makes the capture a preview. */
    void testCaptureToTemporary();

    /** @brief Test capturing a single frame in multiple attempts. */
    void testCaptureSingle();

    /** @brief Test capturing multiple frames in multiple attempts. */
    void testCaptureMultiple();

    /**
         * @brief Test dark flat frames capture after flat frame
         */
    void testCaptureDarkFlats();
};

#endif // HAVE_INDI
#endif // TESTEKOSCAPTURE_H
