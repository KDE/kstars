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

#include <kdialog.h>
#include <kio/copyjob.h>

#include "ui_observinglist.h"
#include "skyobjects/skyobject.h"
#include "kstarsdatetime.h"
#include "geolocation.h"
#include "ksalmanac.h"

class KSAlmanac;
class QSortFilterProxyModel;
class QStandardItemModel;
class KStars;
class KStarsDateTime;
class GeoLocation;
class ObservingListUI : public QFrame, public Ui::ObservingList {
    Q_OBJECT

public:
    /**@short Cunstructor
        */
    ObservingListUI( QWidget *parent );
};

/**@class ObservingList
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

class ObservingList : public KDialog
{
    Q_OBJECT

public:
    /**@short Constructor
        */
    ObservingList( KStars *_ks );
    /**@short Destuctor (empty)
        */
    ~ObservingList() {}

    /**@return true if the object is in the observing list
        *@p o pointer to the object to test.
        */
    inline bool contains( SkyObject *o ) { return m_ObservingList.contains( o ); }

    /**@return true if the window is in its default "large" state.
        */
    bool isLarge() const { return bIsLarge; }

    /**@return reference to the current observing list
        */
    QList<SkyObject*>& obsList() { return m_ObservingList; }
    
    /**@return reference to the current observing list
        */
    QList<SkyObject*>& SessionList() { return m_SessionList; }

    /**@return pointer to the currently-selected object in the observing list
        *@note if more than one object is selected, this function returns 0.
        */
    SkyObject *currentObject() const { return m_CurrentObject; }

    /**@short If the current list has unsaved changes, ask the user about saving it.
        *@note also clears the list in preparation of opening a new one
        */
    void saveCurrentList();

    /**@short Plot the SkyObject's Altitude vs Time in the AVTPlotWidget.
       *@p o pointer to the object to be plotted
       */
    void plot( SkyObject *o );
    
    /**@short Return the altitude of the given SkyObject for the given hour.
        *@p p pointer to the SkyObject 
        *@p hour time at which altitude has to be found
        */
    double findAltitude( SkyPoint *p, double hour=0);

    /**@short Return the list of downloaded images
        */
    QList<QString> imageList() { return ImageList; }

public slots:
    /**@short add a new object to list
        *@p o pointer to the object to add to the list
        *@p session flag toggle adding the object to the session list
        *@p update flag to toggle the call of slotSaveList
        */
    void slotAddObject( SkyObject *o=NULL, bool session=false, bool update=false );

    /**@short Remove skyobjects which are highlighted in the
        *observing list tool from the observing list.
        */
    void slotRemoveSelectedObjects();

    /**@short Remove skyobject from the observing list.
        *@p o pointer to the SkyObject to be removed.
        *@p session flag to tell it whether to remove the object
        *from the sessionlist or from the wishlist
        *@p update flag to toggle the call of slotSaveList
        *Use SkyMap::clickedObject() if o is NULL (default)
        */
    void slotRemoveObject( SkyObject *o=NULL, bool session=false, bool update=false );

    /**@short center the selected object in the display
        */
    void slotCenterObject();

    /**@short slew the telescope to the selected object
        */
    void slotSlewToObject();

    /**@short Show the details window for the selected object
        */
    void slotDetails();

    /**@short Show the Altitude vs Time for selecteld objects
        */
    void slotAVT();

    /**@short Open the WUT dialog
    */
    void slotWUT();
    
    /**@short Add the object to the Session List
        */
    void slotAddToSession();

    /**@short Open the Find Dialog
        */
    void slotFind();

    /**@short Tasks needed when changing the selected object
        *Save the user log of the previous selected object, 
        *find the new selected object in the obsList, and 
        *show the notes associated with the new selected object
        */
    void slotNewSelection();

    /**@short load an observing list from disk.
        */
    void slotOpenList();

    /**@short save the current observing list to disk.
        */
    void slotSaveList();

    /**@short Load the Wish list from disk.
        */
    void slotLoadWishList();

    /**@short save the current observing session plan to disk, specify filename.
        */
    void slotSaveSessionAs();

    /**@short save the current session
        */
    void slotSaveSession();

    /**@short construct a new observing list using the wizard.
        */
    void slotWizard();

    /**@short toggle between the large and small window states
        */
    void slotToggleSize();

    /**@short Save the user log text to a file.
        *@note the log is attached to the current object in obsList.
        */
    void saveCurrentUserLog();

    /**@short toggle the setEnabled flags according to current view
        *set the m_currentItem to NULL and clear selections
        *@p index captures the integer value sent by the signal
        *which is the currentIndex of the table
        */
    void slotChangeTab(int index);

    /**@short Opens the Location dialog to set the GeoLocation
        *for the sessionlist.
        */
    void slotLocation();

    /**@short Updates the tableviews for the new geolocation and date 
        */
    void slotUpdate();

    /**@short Takes the time from the QTimeEdit box and sets it as the
        *time parameter in the tableview of the SessionList.
        */
    void slotSetTime();

    /**@short Downloads the corresponding DSS or SDSS image from the web and
        *displays it
        */
    void slotGetImage( bool _dss = false );

    /**@short Sets the image parameters for the current object
        *@p o The passed object for setting the parameters
        */
    void setCurrentImage( SkyObject *o );

protected slots:
    void slotClose();
    void downloadReady();

private:
    KStars *ks;
    KSAlmanac *ksal;
    ObservingListUI *ui;
    QList<SkyObject*> m_ObservingList, m_SessionList;
    SkyObject *LogObject, *m_CurrentObject;
    uint noNameStars;
    bool isModified, bIsLarge, sessionView, saveOnly, dss;
    QString FileName, SessionName, CurrentImage, DSSUrl, SDSSUrl;
    char decsgn;
    KStarsDateTime dt;
    GeoLocation *geo;
    QStandardItemModel *m_Model, *m_Session;
    QSortFilterProxyModel *m_SortModel, *m_SortModelSession;
    KIO::Job *downloadJob;  // download job of image -> 0 == no job is running
    QHash<QString, QTime> TimeHash; 
    QList<QString> ImageList;
};

#endif // OBSERVINGLIST_H_
