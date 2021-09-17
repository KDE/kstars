/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyobjects/skypoint.h"
#include "skyobjects/skyline.h"

#include <QTimer>
#include <QPainter>

#include <config-kstars.h>
#include <QQuickItem>
#include <QPolygonF>
#include <QLinkedList>
#include "kstarsdata.h"
#include "kstarslite/skyitems/rootnode.h"

#include <basedevice.h>

class DeviceOrientation;

class dms;
class KStarsData;
class SkyObject;
class Projector;
class SolarSystemSingleComponent;
class PlanetsItem;
class AsteroidsItem;
class CometsItem;
class PlanetMoonsComponent;
class HorizonItem;
class LinesItem;
class SkyNode;
class RootNode;
class TelescopeLite;

class SkyObjectLite;
class SkyPointLite;

class QSGTexture;

/** @class SkyMapLite
*
*This is the main item that displays all SkyItems. After its instantiation it is reparanted
*to an object with objectName SkyMapLiteWrapper in main.qml. To display SkyItems they are reparanted
*to instance of SkyMapLite.
*
*SkyMapLite handles most user interaction events (both mouse and keyboard).
*
*@short Item for displaying sky objects; also handles user interaction events.
*@author Artem Fedoskin
*@version 1.0
*/

class SkyMapLite : public QQuickItem
{
        Q_OBJECT
        /** magnitude limit. Used in QML **/
        Q_PROPERTY(double magLim READ getMagLim WRITE setMagLim NOTIFY magLimChanged)

        /** wrappers for clickedPoint and clickedObject. Used to set clicked object and point from QML **/
        Q_PROPERTY(SkyPointLite *clickedPointLite READ getClickedPointLite NOTIFY pointLiteChanged)
        Q_PROPERTY(SkyObjectLite *clickedObjectLite READ getClickedObjectLite NOTIFY objectLiteChanged)
        /** list of FOVSymbols that are currently available **/
        Q_PROPERTY(QStringList FOVSymbols READ getFOVSymbols NOTIFY symbolsFOVChanged)
        /** true if SkyMapLite is being panned **/
        Q_PROPERTY(bool slewing READ getSlewing WRITE setSlewing NOTIFY slewingChanged)
        /**
             * @short true if SkyMapLite is centered on an object and only pinch-to-zoom needs to be available
            **/
        Q_PROPERTY(bool centerLocked READ getCenterLocked WRITE setCenterLocked NOTIFY centerLockedChanged)
        Q_PROPERTY(bool automaticMode READ getAutomaticMode WRITE setAutomaticMode NOTIFY automaticModeChanged)
        Q_PROPERTY(double skyRotation READ getSkyRotation WRITE setSkyRotation NOTIFY skyRotationChanged)

        enum class SkyMapOrientation
        {
            Top0,
            Right90,
            Bottom180,
            Left270
        };

    protected:
        SkyMapLite();

        /** Updates SkyMapLite by calling RootNode::update(), which in turn initiates update of all child nodes. **/
        virtual QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) override;

    public:
        /** Creates instance of SkyMapLite (delete the old one if any) **/
        static SkyMapLite *createInstance();

        /** Bind size to parent's size and initialize star images **/
        void initialize(QQuickItem *parent);

        static SkyMapLite *Instance()
        {
            return pinstance;
        }

        static bool IsSlewing()
        {
            return pinstance->isSlewing();
        }

        /** Destructor. Clear star images.*/
        ~SkyMapLite();

        /**
             * @short skyNode will be deleted on the next call to updatePaintNode (currently used only in
             * StarNode(struct in StarBlock))
             */
        void deleteSkyNode(SkyNode *skyNode);

        /** @short Update the focus position according to current options. */
        void updateFocus();

        /** @short Retrieve the Focus point; the position on the sky at the
             *center of the skymap.
             *@return a pointer to the central focus point of the sky map
             */
        SkyPoint *focus()
        {
            return &Focus;
        }

        /** @short retrieve the Destination position.
             *
             *The Destination is the point on the sky to which the focus will be moved.
             *@return a pointer to the destination point of the sky map
             */
        SkyPoint *destination()
        {
            return &Destination;
        }

        /** @short retrieve the FocusPoint position.
             *
             *The FocusPoint stores the position on the sky that is to be
             *focused next.  This is not exactly the same as the Destination
             *point, because when the Destination is set, it will begin slewing
             *immediately.
             *
             *@return a pointer to the sky point which is to be focused next.
             */
        SkyPoint *focusPoint()
        {
            return &FocusPoint;
        }

        /** @short sets the central focus point of the sky map.
             *@param f a pointer to the SkyPoint the map should be centered on
             */
        void setFocus(SkyPoint *f);

        /** @short sets the focus point of the skymap, using ra/dec coordinates
             *
             *@note This function behaves essentially like the above function.
             *It differs only in the data types of its arguments.
             *
             *@param ra the new right ascension
             *@param dec the new declination
             */
        void setFocus(const dms &ra, const dms &dec);

        /** @short sets the focus point of the sky map, using its alt/az coordinates
             *@param alt the new altitude (actual, i.e. without refraction correction)
             *@param az the new azimuth
             */
        void setFocusAltAz(const dms &alt, const dms &az);

        /** @short sets the destination point of the sky map.
             *@note setDestination() emits the destinationChanged() SIGNAL,
             *which triggers the SLOT function SkyMap::slewFocus().  This
             *function iteratively steps the Focus point toward Destination,
             *repainting the sky at each step (if Options::useAnimatedSlewing()==true).
             *@param f a pointer to the SkyPoint the map should slew to
             */
        void setDestination(const SkyPoint &f);

        /** @short sets the destination point of the skymap, using ra/dec coordinates.
             *
             *@note This function behaves essentially like the above function.
             *It differs only in the data types of its arguments.
             *
             *@param ra the new right ascension
             *@param dec the new declination
             */
        void setDestination(const dms &ra, const dms &dec);

        /** @short sets the destination point of the sky map, using its alt/az coordinates.
             *@param alt the new altitude
             *@param az the new azimuth
             *@param altIsRefracted set to true if the altitude supplied is apparent
             */
        void setDestinationAltAz(const dms &alt, const dms &az, bool altIsRefracted);

        /** @short set the FocusPoint; the position that is to be the next Destination.
             *@param f a pointer to the FocusPoint SkyPoint.
             */
        void setFocusPoint(SkyPoint *f)
        {
            if (f)
                FocusPoint = *f;
        }

        /** @short Retrieve the ClickedPoint position.
             *
             *When the user clicks on a point in the sky map, the sky coordinates of the mouse
             *cursor are stored in the private member ClickedPoint.  This function retrieves
             *a pointer to ClickedPoint.
             *@return a pointer to ClickedPoint, the sky coordinates where the user clicked.
             */
        SkyPoint *clickedPoint()
        {
            return &ClickedPoint;
        }

        /** @short Set the ClickedPoint to the skypoint given as an argument.
             *@param f pointer to the new ClickedPoint.
             */
        void setClickedPoint(SkyPoint *f);

        /** @short Retrieve the object nearest to a mouse click event.
             *
             *If the user clicks on the sky map, a pointer to the nearest SkyObject is stored in
             *the private member ClickedObject.  This function returns the ClickedObject pointer,
             *or nullptr if there is no CLickedObject.
             *@return a pointer to the object nearest to a user mouse click.
             */
        SkyObject *clickedObject() const
        {
            return ClickedObject;
        }

        /** @short Set the ClickedObject pointer to the argument.
             *@param o pointer to the SkyObject to be assigned as the ClickedObject
             */
        void setClickedObject(SkyObject *o);

        /** @short Retrieve the object which is centered in the sky map.
             *
             *If the user centers the sky map on an object (by double-clicking or using the
             *Find Object dialog), a pointer to the "focused" object is stored in
             *the private member FocusObject.  This function returns a pointer to the
             *FocusObject, or nullptr if there is not FocusObject.
             *@return a pointer to the object at the center of the sky map.
             */
        SkyObject *focusObject() const
        {
            return FocusObject;
        }

        /** @short Set the FocusObject pointer to the argument.
             *@param o pointer to the SkyObject to be assigned as the FocusObject
             */
        void setFocusObject(SkyObject *o);

        /** @ Set zoom factor.
             *@param factor zoom factor
             */
        void setZoomFactor(double factor);

        /** @short Call to set up the projector before update of SkyItems positions begins. */
        void setupProjector();

        /** @short Returns index for a Harvard spectral classification */
        int harvardToIndex(char c);

        /** @short returns cache of star images
             *  @return star images cache
             */
        QVector<QVector<QPixmap *>> getImageCache();

        /**
             * @short creates QImage from text and converts it to QSGTexture
             * @param text the text string
             * @param color text color
             * @param zoomFont if true zoom-dependent font from SkyLabeler will be used else standart
             * font is used
             * @return QSGTexture with text
             * @note font size is set in SkyLabeler::SkyLabeler() by initializing m_stdFont with default font
             */
        QSGTexture *textToTexture(QString text, QColor color = QColor(255, 255, 255), bool zoomFont = false);

        /**
             * @short returns cached texture from textureCache.
             *
             * Use outside of scene graph rendering thread (e.g. not during call to updatePaintNode)
             * is prohibited!
             * @param size size of the star
             * @param spType spectral class
             * @return cached QSGTexture from textureCache
             */
        QSGTexture *getCachedTexture(int size, char spType);

        /** @short called when SkyMapComposite finished loading all SkyComponents */
        inline void loadingFinished()
        {
            m_loadingFinished = true;
        }

        /** @return true if the map is in slewing mode or clock is active **/
        bool isSlewing() const;

        /** @return current magnitude limit **/
        inline double getMagLim()
        {
            return m_magLim;
        }

        /** @short set magnitude limit **/
        void setMagLim(double magLim);

        /** @short Convenience function for shutting off tracking mode.  Just calls KStars::slotTrack() **/
        void stopTracking();

        /** Get the current projector.
                @return a pointer to the current projector. */
        inline const Projector *projector() const
        {
            return m_proj;
        }

        /**
             * @short used in QML
             * @return type of current projection system
             */
        Q_INVOKABLE uint projType() const;

        /** Set magnitude limit for size of stars. Used in StarItem **/
        inline void setSizeMagLim(float sizeMagLim)
        {
            m_sizeMagLim = sizeMagLim;
        }

        /** Used in PointSourceNode **/
        inline float sizeMagLim() const
        {
            return m_sizeMagLim;
        }

        static inline RootNode *rootNode()
        {
            return m_rootNode;
        }

        static inline void setRootNode(RootNode *root)
        {
            m_rootNode = root;
        }

        /** return limit of hides for the node to delete it **/
        static double deleteLimit();

        /**
             * @short adds FOV symbol to m_FOVSymbols
             * @param FOVName name of a FOV symbol
             * @param initialState defines whether the state is initial
             */
        Q_INVOKABLE void addFOVSymbol(const QString &FOVName, bool initialState = false);

        /**
             * @param index of FOVSymbol in m_FOVSymbols
             * @return true if FOV symbol with name FOVName should be drawn.
             */
        bool isFOVVisible(int index);

        /**
              * @param index of FOVSymbol in m_FOVSymbols
              * @param visible defines whether the FOV symbol should be visible
              * @short updates visibility of FOV symbol according to visible
              */
        Q_INVOKABLE void setFOVVisible(int index, bool visible);

        /**
              * @short this QList should be used as a model in QML to switch on/off FOV symbols
              **/
        Q_INVOKABLE inline QStringList getFOVSymbols()
        {
            return m_FOVSymbols;
        }

        /** @short Initializes images of Stars and puts them in cache (copied from SkyQPainter)*/
        void initStarImages();

        /**
             * @short getter for clickedPointLite
             */
        SkyPointLite *getClickedPointLite()
        {
            return m_ClickedPointLite;
        }

        /**
             * @short getter for clickedObjectLite
             */
        SkyObjectLite *getClickedObjectLite()
        {
            return m_ClickedObjectLite;
        }

        /**
             * @short getter for centerLocked
             */
        bool getCenterLocked()
        {
            return m_centerLocked;
        }

        /**
             * @short Proxy method for SkyMapDrawAbstract::drawObjectLabels()
             */
        //inline void drawObjectLabels( QList< SkyObject* >& labelObjects ) { dynamic_cast<SkyMapDrawAbstract *>(m_SkyMapDraw)->drawObjectLabels( labelObjects ); }

        /**
             * @return true if SkyMapLite is being slewed
             */
        bool getSlewing() const
        {
            return m_slewing;
        }

        /**
             * @short sets whether SkyMapLite is being slewed
             */
        void setSlewing(bool newSlewing);

        /**
             * @short sets whether SkyMapLite is centered on an object and locked(olny pinch-to-zoom is available)
             */
        void setCenterLocked(bool centerLocked);

        /** True if automatic mode is on (SkyMapLite is controlled by smartphones accelerometer magnetometer) **/
        bool getAutomaticMode() const
        {
            return m_automaticMode;
        }

        /**
             * @short switch automatic mode on/off according to isOn parameter
             */
        Q_INVOKABLE void setAutomaticMode(bool automaticMode);

        double getSkyRotation() const
        {
            return m_skyRotation;
        }

    public slots:
        /** Called whenever wrappers' width or height are changed. Probably will be used to
            * update positions of items.
            */
        void resizeItem();

        /** Recalculates the positions of objects in the sky, and then repaints the sky map.
             */
        void forceUpdate();

        /** @short Left for compatibility reasons
             * @see forceUpdate()
             */
        void forceUpdateNow()
        {
            forceUpdate();
        }

        /**
             * @short Update the focus point and call forceUpdate()
             * @param now is saved for compatibility reasons
             */
        void slotUpdateSky(bool now);

        /** Step the Focus point toward the Destination point.  Do this iteratively, redrawing the Sky
             * Map after each step, until the Focus point is within 1 step of the Destination point.
             * For the final step, snap directly to Destination, and redraw the map.
             */
        void slewFocus();

        /** @short Center the display at the point ClickedPoint.
             *
             * The essential part of the function is to simply set the Destination point, which will emit
             * the destinationChanged() SIGNAL, which triggers the slewFocus() SLOT.  Additionally, this
             * function performs some bookkeeping tasks, such updating whether we are tracking the new
             * object/position, adding a Planet Trail if required, etc.
             *
             * @see destinationChanged()
             * @see slewFocus()
             */
        void slotCenter();

        /** Checks whether the timestep exceeds a threshold value.  If so, sets
             * ClockSlewing=true and sets the SimClock to ManualMode.
             */
        void slotClockSlewing();

        /** Zoom in one step. */
        void slotZoomIn();

        /** Zoom out one step. */
        void slotZoomOut();

        /** Set default zoom. */
        void slotZoomDefault();
        /**
             * @short centres skyObj in SkyMap and opens context drawer with skyObj
             * Used in FindDialogLite
             */
        void slotSelectObject(SkyObject *skyObj);

        /** @short updates focus of SkyMapLite according to data from DeviceOrientation
                (Smartphone's sensors)*/
        void updateAutomaticMode();

        void setSkyRotation(double skyRotation);

    signals:
        /** Emitted by setDestination(), and connected to slewFocus().  Whenever the Destination
             * point is changed, slewFocus() will iteratively step the Focus toward Destination
             * until it is reached.
             * @see SkyMap::setDestination()
             * @see SkyMap::slewFocus()
             */
        void destinationChanged();

        /** Emitted when zoom level is changed. */
        void zoomChanged();

        /** Emitted when current object changed. */
        void objectChanged();

        /** Wrapper of ClickedObject for QML **/
        void objectLiteChanged();

        /** Wrapper of ClickedPoint for QML **/
        void pointLiteChanged();

        /** Emitted when pointing changed. (At least should) */
        void positionChanged();

        /** Emitted when position under mouse changed. */
        void mousePointChanged(SkyPoint *);

        /** Emitted when a position is clicked */
        void positionClicked(SkyPoint *);

        /** Emitted when user clicks on SkyMapLite (analogous to positionClicked but sends QPoint) */
        void posClicked(QPointF pos);

        /** Emitted when magnitude limit is changed */
        void magLimChanged(double magLim);

        /** Emitted when FOVSymbols list was changed (new value appended) **/
        void symbolsFOVChanged(QStringList);

        /** Emitted when SkyMapLite is being slewed or slewing is finished **/
        void slewingChanged(bool);

        void centerLockedChanged(bool);

        void automaticModeChanged(bool);

        /** Emitted when skyRotation used to rotate coordinates of SkyPoints is changed **/
        void skyRotationChanged(double skyRotation);

    protected:
        /** Process keystrokes:
             * @li arrow keys  Slew the map
             * @li +/- keys  Zoom in and out
             * @li <i>Space</i>  Toggle between Horizontal and Equatorial coordinate systems
             * @li 0-9  Go to a major Solar System body (0=Sun; 1-9 are the major planets, except 3=Moon)
             * @li [  Place starting point for measuring an angular distance
             * @li ]  End point for Angular Distance; display measurement.
             * @li <i>Escape</i>  Cancel Angular measurement
             * @li ,/<  Step backward one time step
             * @li ./>  Step forward one time step
             */
        //virtual void keyPressEvent( QKeyEvent *e );

        /** When keyRelease is triggered, just set the "slewing" flag to false,
             * and update the display (to draw objects that are hidden when slewing==true). */
        //virtual void keyReleaseEvent( QKeyEvent *e );

        /** Determine RA, Dec coordinates of clicked location.  Find the SkyObject
             * which is nearest to the clicked location.
             *
             * If left-clicked: Set set mouseButtonDown==true, slewing==true; display
             * nearest object name in status bar.
             * If right-clicked: display popup menu appropriate for nearest object.
             */
        virtual void mousePressEvent(QMouseEvent *e) override;

        /** set mouseButtonDown==false, slewing==false */
        virtual void mouseReleaseEvent(QMouseEvent *e) override;

        /** Center SkyMap at double-clicked location  */
        virtual void mouseDoubleClickEvent(QMouseEvent *e) override;

        /** This function does several different things depending on the state of the program:
             * @li If Angle-measurement mode is active, update the end-ruler point to the mouse cursor,
             *     and continue this function.
             * @li If we are defining a ZoomBox, update the ZoomBox rectangle, redraw the screen,
             *     and return.
             * @li If dragging the mouse in the map, update focus such that RA, Dec under the mouse
             *     cursor remains constant.
             * @li If just moving the mouse, simply update the curso coordinates in the status bar.
             */
        virtual void mouseMoveEvent(QMouseEvent *e) override;

        /** Zoom in and out with the mouse wheel. */
        virtual void wheelEvent(QWheelEvent *e) override;

        /**
             * @short this function handles zooming in and out using "pinch to zoom" gesture
             */
        virtual void touchEvent(QTouchEvent *e) override;

    private slots:
        /** @short display tooltip for object under cursor. It's called by m_HoverTimer.
             *  if mouse didn't moved for last HOVER_INTERVAL milliseconds.
             */
        //void slotTransientLabel();

        /** Set the shape of mouse cursor to a cross with 4 arrows. */
        void setMouseMoveCursor();

    private:
        /** @short Sets the shape of the default mouse cursor to a cross. */
        void setDefaultMouseCursor();

        /** @short Sets the shape of the mouse cursor to a magnifying glass. */
        void setZoomMouseCursor();

        /** Calculate the zoom factor for the given keyboard modifier
             */
        double zoomFactor(const int modifier);

        /** calculate the magnitude factor (1, .5, .2, or .1) for the given
             * keyboard modifier.
             */
        double magFactor(const int modifier);

        /** Decrease the magnitude limit by a step size determined by the
             * keyboard modifier.
             * @param modifier
             */
        void decMagLimit(const int modifier);

        /** Increase the magnitude limit by a step size determined by the
            * keyboard modifier.
            * @param modifier
            */
        void incMagLimit(const int modifier);

        /** Convenience routine to either zoom in or increase mag limit
             * depending on the Alt modifier.  The Shift and Control modifiers
             * will adjust the size of the zoom or the mag step.
             * @param modifier
             */
        void zoomInOrMagStep(const int modifier);

        /** Convenience routine to either zoom out or decrease mag limit
             * depending on the Alt modifier.  The Shift and Control modifiers
             * will adjust the size of the zoom or the mag step.
             * @param modifier
             */
        void zoomOutOrMagStep(const int modifier);

        /**
         * pointer to RootNode. Use it to universally access RootNode
         * @warning RootNode should be used solely during updatePaintNode! See Qt Quick Scene Graph documentation.
         **/
        static RootNode *m_rootNode;
        static SkyMapLite *pinstance;
        static int starColorMode;

        /// True if SkyMapLite was initialized (star images were initialized etc.)
        bool isInitialized { false };
        bool mouseButtonDown { false };
        bool midMouseButtonDown { false };
        /// True if mouseMoveEvent; needed by setMouseMoveCursor
        bool mouseMoveCursor { false };
        bool m_slewing { false };
        bool clockSlewing { false };
        /// True if pinch to zoom or pinch to rotate is performed
        bool pinch { false };
        /// If false only old pixmap will repainted with bitBlt() to save a lot of CPU usage
        bool computeSkymap { false };
        double y0 { 0 };
        int count { 0 };
        KStarsData *data { nullptr };
        /// True if SkyMapComposite has finished loading of SkyComponents
        bool m_loadingFinished { false };
        /// Coordinates of point under cursor. It's update in function mouseMoveEvent
        SkyPoint m_MousePoint;
        SkyPoint Focus, ClickedPoint, FocusPoint, Destination;
        SkyObject *ClickedObject { nullptr };
        SkyObject *FocusObject { nullptr };
        SkyPointLite *m_ClickedPointLite { nullptr };
        SkyObjectLite *m_ClickedObjectLite { nullptr };
        bool m_centerLocked { false };
        //SkyLine AngularRuler; //The line for measuring angles in the map
        QRect ZoomRect; //The manual-focus circle.
        /// Mouse should not move for that interval to display tooltip
        static const int HOVER_INTERVAL = 500;
        /// Timer for tooltips
        QTimer m_HoverTimer;
        bool m_objPointingMode { false };
        bool m_fovCaptureMode { false };
        Projector *m_proj { nullptr };
        QQuickItem *m_SkyMapLiteWrapper { nullptr };
        /// Holds SkyNodes that need to be deleted
        QLinkedList<SkyNode *> m_deleteNodes;
        /// Used in PointSourceNode
        float m_sizeMagLim { 10 };
        /// Mag limit for all objects
        double m_magLim { 0 };
        /// Used to notify zoom-dependent labels about font size change
        bool m_fontSizeChanged { false };
        /// Used for drawing labels
        QPainter m_painter;
        /**
         * This timer is triggered every time user touches the screen with one finger.
         * If touch was released within 500 milliseconds than it is a tap, otherwise we pan.
         **/
        QTimer m_tapBeganTimer;
        /// Good to keep the original ruler start-point for purposes of dynamic_cast
        const SkyPoint *m_rulerStartPoint;
        QStringList m_FOVSymbols;
        QList<bool> m_FOVSymVisible;
        /// Total number of sizes of stars.
        const int nStarSizes { 15 };
        /// Total number of spectral classes (N.B. Must be in sync with harvardToIndex)
        const int nSPclasses { 7 };
        /// Cache for star images.
        QVector<QVector<QPixmap *>> imageCache;
        /// Textures created from cached star images
        QVector<QVector<QSGTexture *>> textureCache;
        bool clearTextures { false };
        bool tapBegan { false };
        QList<INDI::BaseDevice *> m_newTelescopes;
        QList<INDI::BaseDevice *> m_delTelescopes;
        bool m_automaticMode { false };
        double m_skyRotation { 0 };
        SkyMapOrientation m_skyMapOrientation { SkyMapOrientation::Top0 };
#if defined(Q_OS_ANDROID)
        QTimer automaticModeTimer;
        DeviceOrientation *m_deviceOrientation { nullptr };
#endif
};
