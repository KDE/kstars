
/***************************************************************************
                          XPlanetImageviewer.cpp  -  Based on: KStars Image Viwer by Thomas Kabelmann
                             -------------------
    begin                : Sun Aug 12, 2018
    copyright            : (C) 2018 by Robert Lancaster
    email                : rlancaste@gmail.com
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

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
#include <QUrl>
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

  protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *) override;

  private:
    //Image related
    QPixmap pix;
    QImage m_Image;

    //Mouse related
    bool mouseButtonDown = false;
    QPoint lastMousePoint;
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
     * @brief loadImage Load image from local file and display it
     * @param filename path to local image
     * @return True if opened and displayed, false otherwise
     */
    bool loadImage();

    void startXplanet();

  private:
    /**
     * Display the downloaded image.  Resize the window to fit the image,  If the image is
     * larger than the screen, make the image as large as possible while preserving the
     * original aspect ratio
     */
    bool showImage();

    /** prepares the output file**/
    bool setupOutputFile();

    QImage image;

    /** Save the downloaded image to a local file. */
    void saveFile(QUrl &url);

    QFile file;

    QString filename;

    FileDownloader downloadJob;

    XPlanetImageLabel *m_View { nullptr };
    QLabel *m_Caption { nullptr };

    QUrl lastURL;

    typedef enum { YEARS, MONTHS, DAYS, HOURS, MINS, SECS } timeUnits;

    void setXPlanetDate(KStarsDateTime time);

    //XPlanet strings
    QString object;
    QString origin;
    QString date;
    QString dateText;

    //XPlanet numbers
    int rotation;
    int timeUnit;
    int radius;
    double FOV;
    double lat;
    double lon;

    // Time
    KStarsDateTime xplanetTime;
    bool xplanetRunning = false;

    QComboBox *originSelector {nullptr};

    // Field of view controls
    QPushButton *kstarsFOV  {nullptr};
    QPushButton *noFOV  {nullptr};
    NonLinearDoubleSpinBox *FOVEdit {nullptr};

    // Rotation controls
    QSpinBox *rotateEdit {nullptr};

    // Free rotation controls
    QLabel *latDisplay {nullptr};
    QLabel *lonDisplay {nullptr};
    QLabel *radDisplay {nullptr};
    QPushButton *freeRotate {nullptr};

    // Time controls
    QLabel *XPlanetTimeDisplay {nullptr};
    QSlider *timeSlider {nullptr};
    QSpinBox *timeEdit {nullptr};
    QComboBox *timeUnitsSelect {nullptr};

    //Animation controls
    QPushButton *runTime {nullptr};
    QTimer *XPlanetTimer {nullptr};


  private slots:

    // Saves file to disk.
    void saveFileToDisk();

    // Inverts colors
    void invertColors();

    // Rotation slots
    void updateXPlanetRotationEdit();
    void resetXPlanetRotation();
    void invertXPlanetRotation();

    // Free Rotation slots
    void changeXPlanetPosition(QPoint delta);
    void slotFreeRotate();
    void updateStates();

    // Field of View slots
    void zoomInXPlanetFOV();
    void zoomOutXPlanetFOV();
    void updateXPlanetFOVEdit();
    void clearXPlanetFOV();
    void setKStarsXPlanetFOV();

    // Time slots
    void updateXPlanetTime(int timeShift);
    void updateXPlanetObject(const QString & obj);
    void updateXPlanetOrigin(const QString & obj);
    void updateXPlanetTimeUnits(int units);
    void updateXPlanetTimeEdit();
    void setXPlanetTime();
    void setXPlanetTimetoKStarsTime();
    void resetXPlanetTime();

    // Animation slots
    void incrementXPlanetTime();
    void toggleXPlanetRun();

};
