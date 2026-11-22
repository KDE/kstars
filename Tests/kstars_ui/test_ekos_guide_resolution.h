/*
    KStars UI tests for a guide camera that changes its size at runtime

    SPDX-FileCopyrightText: 2026 Andreas R. <temp@ruthner.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "config-kstars.h"
#include "test_ekos.h"
#include "test_ekos_helper.h"

#if defined(HAVE_INDI)

#include <QObject>
#include <QSize>

namespace ISD
{
class Camera;
class CameraChip;
}

/**
 * @brief Guide captures after the guide camera changed its resolution, with the INDI simulators and the internal guider.
 *
 * Some camera drivers change the chip size at runtime, e.g. the ToupTek drivers with their CCD_RESOLUTION switch. The
 * CCD Simulator has the same switch (the Guide Simulator has none), so it is the guide camera here. After a switch the next guide frame must have the new
 * size: before the fix, Guide kept requesting the chip size it saw when the camera was set, so a larger resolution gave
 * only the top-left part of the chip, and a smaller one a frame the driver could not take.
 */
class TestEkosGuideResolution : public QObject
{
        Q_OBJECT
    public:
        explicit TestEkosGuideResolution(QObject *parent = nullptr);

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();
        void init();
        void cleanup();

        void testResolutionChange_data();
        void testResolutionChange();

    private:
        TestEkosHelper *m_Helper { nullptr };
        ISD::Camera *m_Camera { nullptr };
        QList<QSize> m_Images;

        ISD::CameraChip *guideChip() const;
        /** Switch CCD_RESOLUTION of the guide camera and wait until the driver reports the new chip size. */
        bool setResolution(const QString &resolution, const QSize &size);
        /** Take one guide frame and check its size, and that the driver took the requested frame. */
        bool captureAndCheck(const QSize &expected);
};

#endif // HAVE_INDI
