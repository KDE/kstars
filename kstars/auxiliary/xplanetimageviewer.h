/*
    SPDX-FileCopyrightText: Thomas Kabelmann
    SPDX-FileCopyrightText: 2018 Robert Lancaster <rlancaste@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "auxiliary/filedownloader.h"

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include "nonlineardoublespinbox.h"
#include <QComboBox>
#include <QFile>
#include <QFrame>
#include <QImage>
#include <QPixmap>
#include "kstarsdata.h"
#include <QEvent>
#include <QGestureEvent>
#include <QPinchGesture>

class QLabel;
/**
 * @class XPlanetImageLabel
 * @short XPlanet Image viewer QFrame for the KPlanetImageViewer for KStars
 * @author Thomas Kabelmann
 * @author Jasem Mutlaq
 * @author Robert Lancaster
 * @version 1.1
 *
 * This image-viewer QFrame automatically resizes the picture.  It also receives input from the mouse to
 * zoom and pan the XPlanet Image.
 */
class XPlanetImageLabel : public QFrame
{
    Q_OBJECT
  public:
    explicit XPlanetImageLabel(QWidget *parent);
    ~XPlanetImageLabel() override = default;
    void setImage(const QImage &img);
    void invertPixels();
    void refreshImage();

  public slots:
    void wheelEvent(QWheelEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;

  signals:
    void zoomIn();
    void zoomOut();
    void changePosition(QPoint);
    void changeLocation(QPoint);

  protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *) override;

  private:
    //Image related
    QPixmap m_Pix;
    QImage m_Image;

    //Mouse related
    bool m_MouseButtonDown = false;
    QPoint m_LastMousePoint;
    bool event(QEvent *event) override;
    bool gestureEvent(QGestureEvent *event);
    void pinchTriggered(QPinchGesture *gesture);
};

/**
 * @class XPlanetImageViewer
 * @short XPlanet Image viewer window for KStars
 * @author Thomas Kabelmann
 * @author Jasem Mutlaq
 * @author Robert Lancaster
 * @version 1.1
 *
 * This class is meant to interact with XPlanet and display the results.  It has interfaces to control many of the XPlanet Options.
 * It can change a number of properties using controls.  It works with the XPlanetImageLabel to display the results.  It also can work
 * with the XPlanetImageLabel to respond to mouse inputs on the ImageLabel and act on them in the Image Viewer.  It has a hover mode where
 * it hovers over the same planetary body or a remote mode where it views one body from another.  It has interfaces for changing the field of
 * view, the time, the rotation, and other settings.  In hover mode, it can change latitude, longitude, and radius with the mouse inputs.
 * It also can animate events so that you can watch xplanet frames like a movie to see solar system events in action.
 */
class XPlanetImageViewer : public QDialog
{
    Q_OBJECT

  public:
    /** Create xplanet image viewer from Object */
    explicit XPlanetImageViewer(const QString &obj, QWidget *parent = nullptr);

    /** Destructor. If there is a partially downloaded image file, delete it.*/
    ~XPlanetImageViewer() override;

    /**
     * @brief loadImage Load image and display it
     * @return True if opened and displayed, false otherwise
     */
    bool loadImage();

    void startXplanet();

  private:


    /** prepares the output file**/
    bool setupOutputFile();

    QImage m_Image;

    /** Save the downloaded image to a local file. */
    void saveFile(const QString & fileName);

    QFile m_File;

    XPlanetImageLabel *m_View { nullptr };
    QLabel *m_Caption { nullptr };

    QString m_LastFile;

    QStringList m_ObjectNames;
    QList<double> m_objectDefaultFOVs;

    typedef enum { YEARS, MONTHS, DAYS, HOURS, MINS, SECS } timeUnits;

    void setXPlanetDate(KStarsDateTime time);

    //XPlanet strings
    QString m_ObjectName;
    QString m_OriginName;
    QString m_Date;
    QString m_DateText;

    //XPlanet numbers
    int m_CurrentObjectIndex { 0 };
    int m_CurrentOriginIndex { 0 };
    int m_Rotation { 0 };
    int m_CurrentTimeUnitIndex { 0 };
    uint32_t m_Radius { 0 };
    double m_FOV { 0 };
    double m_lat { 0 };
    double m_lon { 0 };
    QPoint center;

#ifndef Q_OS_WIN
    QFutureWatcher<bool> fifoImageLoadWatcher;
    QTimer watcherTimeout;
#endif

    // Time
    KStarsDateTime m_XPlanetTime {};
    bool m_XPlanetRunning = false;

    QComboBox *m_OriginSelector {nullptr};

    // Field of view controls
    QPushButton *m_KStarsFOV  {nullptr};
    QPushButton *m_setFOV  {nullptr};
    QPushButton *m_NoFOV  {nullptr};
    NonLinearDoubleSpinBox *m_FOVEdit {nullptr};

    // Rotation controls
    QSpinBox *m_RotateEdit {nullptr};

    // Free rotation controls
    QLabel *m_PositionDisplay {nullptr};
    QPushButton *m_FreeRotate {nullptr};
    bool m_ImageLoadSucceeded = false;

    // Time controls
    QLabel *m_XPlanetTimeDisplay {nullptr};
    QSlider *m_TimeSlider {nullptr};
    QSpinBox *m_TimeEdit {nullptr};
    QComboBox *m_TimeUnitsSelect {nullptr};

    //Animation controls
    QPushButton *m_RunTime {nullptr};
    QTimer *m_XPlanetTimer {nullptr};


  private slots:

    /**
     * Display the downloaded image.  Resize the window to fit the image,  If the image is
     * larger than the screen, make the image as large as possible while preserving the
     * original aspect ratio
     */
    bool showImage();

    // Saves file to disk.
    void saveFileToDisk();

    // Inverts colors
    void invertColors();

    // Rotation slots
    void updateXPlanetRotationEdit();
    void resetXPlanetRotation();
    void invertXPlanetRotation();

    void reCenterXPlanet();

    // Free Rotation slots
    void changeXPlanetPosition(QPoint delta);
    void changeXPlanetLocation(QPoint delta);
    void slotFreeRotate();
    void updateStates();
    void updatePositionDisplay();
    void resetLocation();

    // Field of View slots
    void zoomInXPlanetFOV();
    void zoomOutXPlanetFOV();
    void updateXPlanetFOVEdit();
    void resetXPlanetFOV();
    void setKStarsXPlanetFOV();
    void setFOVfromList();

    // Time slots
    void updateXPlanetTime(int timeShift);
    void updateXPlanetObject(int objectIndex);
    void updateXPlanetOrigin(int originIndex);
    void updateXPlanetTimeUnits(int units);
    void updateXPlanetTimeEdit();
    void setXPlanetTime();
    void setXPlanetTimetoKStarsTime();
    void resetXPlanetTime();

    // Animation slots
    void incrementXPlanetTime();
    void toggleXPlanetRun();
    void timeSliderDisplay(int timeShift);

};
