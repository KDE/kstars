/***************************************************************************
                          observinglist.h  -  K Desktop Planetarium
                             -------------------
    begin                : 29 Nov 2004
    copyright            : (C) 2004 by Jeff Woods, Jason Harris
    email                : jcwoods@bellsouth.net, jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OBSERVINGLIST_H_
#define OBSERVINGLIST_H_

#include <QList>
#include <QAbstractTableModel>

#include <QDialog>
//#include <KIO/CopyJob>

#include "ui_observinglist.h"
#include "kstarsdatetime.h"

class KSAlmanac;
class QSortFilterProxyModel;
class QStandardItemModel;
class KStars;
class KStarsDateTime;
class GeoLocation;
class ObsListPopupMenu;
class SkyPoint;
class SkyObject;
class KSDssDownloader;

class ObservingListUI : public QFrame, public Ui::ObservingList {
    Q_OBJECT

public:
    /** @short Cunstructor
        */
    ObservingListUI( QWidget *parent );
};

/** @class ObservingList
    *Tool window for managing a custom list of objects.  The window
    *displays the Name, RA, Dec, mag, and type of each object in the list.
    *
    *By selecting an object in the list, you can perform a number of functions
    *on that object:
    *+ Center it in the display
    *+ Examine its Details Window
    *+ Point the telescope at it
    *+ Attach a custom icon or name label (TBD)
    *+ Attach a trail (solar system only) (TBD)
    *+ Open the AltVsTime tool
    *
    *The user can also save/load their observing lists, and can export
    *list data (TBD: as HTML table?  CSV format?  plain text?)
    *
    *The observing notes associated with the selected object are displayed
    *below the list.
    *
    *TODO:
    *+ Implement a "shaded" state, in which the UI is compressed to
    *  make it easier to float on the KStars window.  Displays only
    *  object names, and single-letter action buttons, and no user log.
    *+ Implement an InfoBox version (the ultimate shaded state)
    *
    *@short Tool for managing a custom list of objects
    *@author Jeff Woods, Jason Harris
    *@version 1.0
    */

class ObservingList : public QDialog
{
    Q_OBJECT

public:
    /** @short Constructor
        */
    ObservingList();
    /** @short Destuctor (empty)
        */
    ~ObservingList();

    /** @return true if the object is in the observing list
        *@p o pointer to the object to test.
        */
    inline bool contains( const SkyObject *o ) { return m_WishList.contains( const_cast<SkyObject*>(o) ); }

    /** @return true if the window is in its default "large" state.
        */
    bool isLarge() const { return bIsLarge; }

    /** @return reference to the current observing list
        */
    QList<SkyObject*>& obsList() { return m_WishList; }

    /** @return reference to the current observing list
        */
    QList<SkyObject*>& sessionList() { return m_SessionList; }

    /** @return pointer to the currently-selected object in the observing list
        *@note if more than one object is selected, this function returns 0.
        */
    inline SkyObject *currentObject() const { return m_CurrentObject; }

    /** @short If the current list has unsaved changes, ask the user about saving it.
        *@note also clears the list in preparation of opening a new one
        */
    void saveCurrentList();

    /** @short Plot the SkyObject's Altitude vs Time in the AVTPlotWidget.
       *@p o pointer to the object to be plotted
       */
    void plot( SkyObject *o );

    /** @short Return the altitude of the given SkyObject for the given hour.
        *@p p pointer to the SkyObject
        *@p hour time at which altitude has to be found
        */
    double findAltitude( SkyPoint *p, double hour=0);

    /** @short Sets the image parameters for the current object
        *@p o The passed object for setting the parameters
        */
    void setCurrentImage( const SkyObject *o );

    /**
     * @short Returns a path to the current image, or a writable image.
     */
    QString getCurrentImagePath();

    /** @short Save the user log text to a file.
        *@note the log is attached to the current object in obsList.
        */
    void saveCurrentUserLog();

    /** @short decides on whether to enable the SaveImages button or not
        */
    void setSaveImagesButton();

    /** @short This is the declaration of the event filter function
     * which is installed on the KImageFilePreview and the TabeView
     */
    bool eventFilter( QObject *obj, QEvent *event );

    /** @short saves a thumbnail image for the details dialog
     * from the downloaded image
     */
    void saveThumbImage();

    QString getTime( const SkyObject *o ) const;

    QTime scheduledTime( SkyObject *o ) const;

    void setTime( const SkyObject *o, QTime t );

    inline GeoLocation* geoLocation() { return geo; }

    inline KStarsDateTime dateTime() const { return dt; }

    /** @short return the object with the name as the passed
     * QString from the Session List, return null otherwise
     */
    SkyObject* findObjectByName( QString name );

    /** @short make a selection in the session view
     */
    void selectObject( const SkyObject *o );

    /** @short set the default image in the image preview.
     */
    void setDefaultImage();

    /** @short get object name. If star has no name, generate a name based on catalog number.
     * @param translated set to true if the translated name is required.
     */
    QString getObjectName(const SkyObject *o, bool translated=true);

public slots:
    /** @short add a new object to list
        *@p o pointer to the object to add to the list
        *@p session flag toggle adding the object to the session list
        *@p update flag to toggle the call of slotSaveList
        */
    void slotAddObject( SkyObject *o=NULL, bool session=false, bool update=false );

    /** @short Remove skyobjects which are highlighted in the
        *observing list tool from the observing list.
        */
    void slotRemoveSelectedObjects();

    /** @short Remove skyobject from the observing list.
        *@p o pointer to the SkyObject to be removed.
        *@p session flag to tell it whether to remove the object
        *from the sessionlist or from the wishlist
        *@p update flag to toggle the call of slotSaveList
        *Use SkyMap::clickedObject() if o is NULL (default)
        */
    void slotRemoveObject( SkyObject *o=NULL, bool session=false, bool update=false );

    /** @short center the selected object in the display
        */
    void slotCenterObject();

    /** @short slew the telescope to the selected object
        */
    void slotSlewToObject();

    /**
     * @brief slotAddToEkosScheduler Add object to Ekos scheduler
     */
    void slotAddToEkosScheduler();

    /** @short Show the details window for the selected object
        */
    void slotDetails();

    /** @short Show the Altitude vs Time for selecteld objects
        */
    void slotAVT();

    /** @short Open the WUT dialog
    */
    void slotWUT();

    /** @short Add the object to the Session List
        */
    void slotAddToSession();

    /** @short Open the Find Dialog
        */
    void slotFind();

    /** @short Tasks needed when changing the selected object
        *Save the user log of the previous selected object,
        *find the new selected object in the obsList, and
        *show the notes associated with the new selected object
        */
    void slotNewSelection();

    /** @short load an observing list from disk.
        */
    void slotOpenList();

    /** @short save the current observing list to disk.
        */
    void slotSaveList();

    /** @short Load the Wish list from disk.
        */
    void slotLoadWishList();

    /** @short save the current observing session plan to disk, specify filename.
        */
    void slotSaveSessionAs(bool nativeSave = true);

    /** @short save the current session
        */
    void slotSaveSession(bool nativeSave = true);

    /** @short construct a new observing list using the wizard.
        */
    void slotWizard();

    /** @short toggle between the large and small window states
        */
    void slotToggleSize();

    /** @short toggle the setEnabled flags according to current view
        *set the m_currentItem to NULL and clear selections
        *@p index captures the integer value sent by the signal
        *which is the currentIndex of the table
        */
    void slotChangeTab(int index);

    /** @short Opens the Location dialog to set the GeoLocation
        *for the sessionlist.
        */
    void slotLocation();

    /** @short Updates the tableviews for the new geolocation and date
        */
    void slotUpdate();

    /** @short Takes the time from the QTimeEdit box and sets it as the
        *time parameter in the tableview of the SessionList.
        */
    void slotSetTime();

    /** @short Downloads the corresponding DSS or SDSS image from the web and
        *displays it
        */
    void slotGetImage( bool _dss = false, const SkyObject *o = 0 );

    void slotSearchImage();

    /** @short Downloads the images of all the objects in the session list
        *Note: This downloads the SDSS image, checks if the size is > default image
        *and gets the DSS image if thats the case
        */
    void slotSaveAllImages();

    /** @short saves the image syncronously from a given URL into a given file
        *@p url the url from which the image has to be downloaded
        *@p filename the file onto which the url has to be copied to
        *NOTE: This is not a generic image saver, it is specific to the current object
        */
    void saveImage( QUrl url , QString filename, const SkyObject *o = 0 );

    /** @short Shows the image in a ImageViewer window.
        */
    void slotImageViewer();

    /** @short Remove the current image
        */
    void slotDeleteCurrentImage();

    /** @short Removes all the save DSS/SDSS images from the disk.
        */
    void slotDeleteAllImages();

    /** @short download the DSS image and show it
        */
    void slotDSS() { slotGetImage( true ); }

    /**
     *@short Present the user with options to get the right DSS image for the job
     */
    void slotCustomDSS();

    /** @short Export a target list to the oal compliant format
        */
    void slotOALExport();

    void slotAddVisibleObj();

    /**
     * @short Show the eyepiece field view
     */
    void slotEyepieceView();


protected slots:
    void slotClose();
    void downloadReady( bool success );

private:

    /**
     * @short Return the active list
     * @return The session list or the wish list depending on which tab is currently being viewed.
     */
    inline QList<SkyObject *>& getActiveList() { return ( ( sessionView ) ? m_SessionList : m_WishList ); }

    /**
     * @short Return the active itemmodel
     * @return the session model or the wishlist model depending on which tab is currently being viewed.
     */
    inline QStandardItemModel *getActiveModel() const { return ( ( sessionView ) ? m_SessionModel : m_WishListModel ); }

    /**
     * @short Return the active sort model
     * @return the session sort model or the wishlist sort model depending on which tab is currently being viewed.
     */
    inline QSortFilterProxyModel *getActiveSortModel() const { return ( ( sessionView ) ? m_SessionSortModel : m_WishListSortModel ); }

    /**
     * @short Return the active view
     * @return the active view in the UI -- session view or wishlist view depending on which one is active.
     */
    inline QTableView *getActiveView() const { return ( ( sessionView ) ? ( ui->SessionView ) : ( ui->WishListView ) ); }

    /**
     * @short Get the currently selected item indexes
     * @return a QModelIndexList containing the selected rows in the active QTableView
     */
    inline QModelIndexList getSelectedItems() const { return getActiveView()->selectionModel()->selectedRows(); }

    KSAlmanac *ksal;
    ObservingListUI *ui;
    QList<SkyObject*> m_WishList, m_SessionList;
    SkyObject *LogObject, *m_CurrentObject;
    bool isModified, bIsLarge, sessionView, dss, singleSelection, showScope, noSelection;
    QString m_listFileName, m_currentImageFileName, ThumbImage;
    KStarsDateTime dt;
    GeoLocation *geo;
    QStandardItemModel *m_WishListModel, *m_SessionModel;
    QSortFilterProxyModel *m_WishListSortModel, *m_SessionSortModel;
    QHash<QString, QTime> TimeHash;
    ObsListPopupMenu *pmenu;
    KSDssDownloader *m_dl;
    QHash<SkyObject *, QPixmap> ImagePreviewHash;
    QPixmap m_NoImagePixmap;
};

#endif // OBSERVINGLIST_H_
