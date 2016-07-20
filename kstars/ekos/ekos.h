#ifndef EKOS_H
#define EKOS_H

#include <KLocalizedString>

namespace Ekos
{

    // Guide States
    static const QStringList guideStates = { I18N_NOOP("Idle"), I18N_NOOP("Calibrating"), I18N_NOOP("Calibration successful"), I18N_NOOP("Calibration error"), I18N_NOOP("Guiding"),
                                             I18N_NOOP("Aborted"), I18N_NOOP("Suspended"), I18N_NOOP("Dithering"), I18N_NOOP("Dithering Successful"), I18N_NOOP("Dithering error")};

    typedef enum { GUIDE_IDLE, GUIDE_CALIBRATING, GUIDE_CALIBRATION_SUCESS, GUIDE_CALIBRATION_ERROR, GUIDE_GUIDING,
                   GUIDE_ABORTED, GUIDE_SUSPENDED, GUIDE_DITHERING, GUIDE_DITHERING_SUCCESS, GUIDE_DITHERING_ERROR } GuideState;

    const QString & getGuideStatusString(GuideState state);

    // Capture States
    static const QStringList captureStates = { I18N_NOOP("Idle"), I18N_NOOP("In Progress"), I18N_NOOP("Capturing"), I18N_NOOP("Aborted"), I18N_NOOP("Waiting"), I18N_NOOP("Image Received"),
                                               I18N_NOOP("Dithering"), I18N_NOOP("Focusing"), I18N_NOOP("Changing Filter"), I18N_NOOP("Setting Temperature"), I18N_NOOP("Aligning"),
                                               I18N_NOOP("Calibrating"), I18N_NOOP("Meridian Flip"), I18N_NOOP("Complete") };

    typedef enum { CAPTURE_IDLE, CAPTURE_PROGRESS, CAPTURE_CAPTURING, CAPTURE_ABORTED, CAPTURE_WAITING, CAPTURE_IMAGE_RECEIVED, CAPTURE_DITHERING, CAPTURE_FOCUSING,
                   CAPTURE_CHANGING_FILTER, CAPTURE_SETTING_TEMPERATURE, CAPTURE_ALIGNING, CAPTURE_CALIBRATING, CAPTURE_MERIDIAN_FLIP, CAPTURE_COMPLETE } CaptureState;


    const QString &getCaptureStatusString(CaptureState state);

    // Focus States
    static const QStringList focusStates = { I18N_NOOP("Idle"), I18N_NOOP("Complete"), I18N_NOOP("Failed"), I18N_NOOP("Aborted"), I18N_NOOP("In Progress"),
                                             I18N_NOOP("Capturing"), I18N_NOOP("Framing"), I18N_NOOP("User Input"), I18N_NOOP("Changing Filter")};

    typedef enum { FOCUS_IDLE, FOCUS_COMPLETE, FOCUS_FAILED, FOCUS_ABORTED, FOCUS_PROGRESS, FOCUS_FRAMING, FOCUS_WAITING, FOCUS_CHANGING_FILTER} FocusState;

    const QString &getFocusStatusString(FocusState state);

}

#endif // EKOS_H
