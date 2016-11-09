/** *************************************************************************
                          kstarslite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 30/04/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSTARSLITE_H_
#define KSTARSLITE_H_

#include <config-kstars.h>
#include <QString>
#include <QtQml/QQmlApplicationEngine>
#include <QPalette>

//Needed for Projection enum
#include "projections/projector.h"

// forward declaration is enough. We only need pointers
class KStarsData;
class SkyMapLite;
class SkyPoint;
class GeoLocation;
class ImageProvider;

class FindDialogLite;
class DetailDialogLite;
class LocationDialogLite;

class ClientManagerLite;

class QQuickItem;

/**
 *@class KStarsLite
 *@short This class loads QML files and connects SkyMapLite and KStarsData
 * Unlike KStars class it is not a main window (see KStarsLite::m_Engine) but a root object that contains the program clock and
 * holds pointers to SkyMapLite and KStarsData objects.
 * KStarsLite is a singleton, use KStarsLite::createInstance() to create an instance and KStarsLite::Instance() to get a pointer to the instance
 *@author Artem Fedoskin
 *@version 1.0
 */

class KStarsLite : public QObject
{
    Q_OBJECT
    //runTutorial is a wrapper for Options::RunStartupWizard()
    Q_PROPERTY(bool runTutorial WRITE setRunTutorial READ getRunTutorial NOTIFY runTutorialChanged)
private:
    /**
     * @short Constructor.
     * @param doSplash should the splash panel be displayed during
     * initialization.
     * @param startClockRunning should the clock be running on startup?
     * @param startDateString date (in string representation) to start running from.
     */
    explicit KStarsLite( bool doSplash, bool startClockRunning = true, const QString &startDateString = QString() );
    
    static KStarsLite *pinstance; // Pointer to an instance of KStarsLite
    
public:
    /**
     * @short Create an instance of this class. Destroy any previous instance
     * @param doSplash
     * @param clockrunning
     * @param startDateString
     * @note See KStarsLite::KStarsLite for details on parameters
     * @return a pointer to the instance
     */
    static KStarsLite *createInstance( bool doSplash, bool clockrunning = true, const QString &startDateString = QString() );
    
    /** @return a pointer to the instance of this class */
    inline static KStarsLite *Instance() { return pinstance; }
    
    /** Destructor. Does nothing yet*/
    virtual ~KStarsLite();
    
    /** @return pointer to SkyMapLite object which draws SkyMap. */
    inline SkyMapLite* map() const { return m_SkyMapLite; }
    
    /** @return pointer to KStarsData object which contains application data. */
    inline KStarsData* data() const { return m_KStarsData; }
    
    /** @return pointer to ImageProvider that is used in QML to display image fetched from CCD **/
    inline ImageProvider *imageProvider() const { return m_imgProvider; }
    
    /** @return pointer to QQmlApplicationEngine that runs QML **/
    inline QQmlApplicationEngine *qmlEngine() { return &m_Engine; }
    
    /** @short used from QML to update positions of sky objects and update SkyMapLite */
    Q_INVOKABLE void fullUpdate();

    /** @short currently sets color scheme from config **/
    Q_INVOKABLE void applyConfig( bool doApplyFocus = true );

    /** @short set whether tutorial should be shown on next startup **/
    void setRunTutorial(bool runTutorial);

    /** @return true if tutorial should be shown **/
    bool getRunTutorial();
    
    /** @return pointer to KStarsData object which handles connection to INDI server. */
    inline ClientManagerLite *clientManagerLite() const { return m_clientManager; }

    /** @defgroup kconfigwrappers QML wrappers around KConfig
     *  @{
     */

    enum ObjectsToToggle { Stars,
                           DeepSky,
                           Planets,
                           CLines,
                           CBounds,
                           ConstellationArt,
                           MilkyWay,
                           CNames,
                           EquatorialGrid,
                           HorizontalGrid,
                           Ground,
                           Flags,
                           Satellites,
                           Supernovae
                         };

    Q_ENUMS(ObjectsToToggle)
    /**
     * @short setProjection calls Options::setProjection(proj) and updates SkyMapLite
     */
    /*Having projection as uint is not good but it will go away once KConfig is fixed
    The reason for this is that you can't use Enums of another in class in Q_INVOKABLE function*/
    Q_INVOKABLE void setProjection(uint proj);

    /** These functions are just convenient getters to access internals of KStars from QML **/

    /**
     * @short returns color with key name from current color scheme
     * @param schemeColor name the key name of the color to be retrieved from current color scheme
     * @return color from name
     */
    Q_INVOKABLE QColor getColor(QString name);

    Q_INVOKABLE QString getConfigCScheme();

    /**
     * @short toggles on/off objects of group toToggle
     * @see ObjectsToToggle
     */
    Q_INVOKABLE void toggleObjects(ObjectsToToggle toToggle, bool toggle);

    /**
     * @return true if objects from group toToggle are currently toggled on
     **/
    Q_INVOKABLE bool isToggled(ObjectsToToggle toToggle);

    /** @} */ // end of kconfigwrappers group

signals:
    /** Sent when KStarsData finishes loading data */
    void dataLoadFinished();
    
    /** Makes splash (Splash.qml) visible on startup */
    void showSplash();
    
    /** Emitted whenever TimeSpinBox in QML changes the scale **/
    void scaleChanged(float);

    void runTutorialChanged();

    /** Once this signal is emitted, notification with text msg will appear on the screen.
        Use this signal to output messages to user (warnings, info etc.) **/
    void notificationMessage(QString msg);
    
public Q_SLOTS:
    /**
     * Update time-dependent data and (possibly) repaint the sky map.
     * @param automaticDSTchange change DST status automatically?
     */
    void updateTime( const bool automaticDSTchange = true );
    
    /** Write current settings to config file. Used to save config file upon exit
     */
    bool writeConfig();
    
    /** Load a color scheme.
     * @param name the name of the color scheme to load (e.g., "Moonless Night")
     */
    void loadColorScheme( const QString &name );
    
    /** sets time and date according to parameter time*/
    void slotSetTime(QDateTime time);
    
    /** action slot: toggle whether kstars clock is running or not */
    void slotToggleTimer();
    
    /** action slot: advance one step forward in time */
    void slotStepForward();
    
    /** action slot: advance one step backward in time */
    void slotStepBackward();
    
    /** @short start tracking clickedPoint or stop tracking if we are already tracking some object **/
    void slotTrack();
    
private slots:
    /** finish setting up after the KStarsData has finished
     */
    void datainitFinished();
    
    /** Save data to config file before exiting.*/
    void handleStateChange(Qt::ApplicationState state);
    
private:
    /** Initialize focus position */
    void initFocus();
    
    QQmlApplicationEngine m_Engine;
    SkyMapLite *m_SkyMapLite;
    QPalette OriginalPalette, DarkPalette;
    
    QObject *m_RootObject;
    bool StartClockRunning;
    
    KStarsData *m_KStarsData;
    ImageProvider *m_imgProvider;
    
    //Dialogs
    FindDialogLite *m_findDialogLite;
    DetailDialogLite *m_detailDialogLite;
    LocationDialogLite *m_locationDialogLite;

    ClientManagerLite *m_clientManager;
};

#endif

