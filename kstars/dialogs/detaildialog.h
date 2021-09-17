/*
    SPDX-FileCopyrightText: 2002 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_details_data.h"
#include "ui_details_data_comet.h"
#include "ui_details_database.h"
#include "ui_details_links.h"
#include "ui_details_log.h"
#include "ui_details_position.h"

#include "skyobjectuserdata.h"
#include <kpagedialog.h>

#include <QPalette>
#include <QString>

#include <memory>

class QListWidgetItem;
class QPixmap;

class DataCometWidget;
class DataWidget;
class GeoLocation;
class KStars;
class KStarsDateTime;
class SkyObject;

class PositionWidget;
class LinksWidget;
class DatabaseWidget;
class LogWidget;

struct ADVTreeData
{
    QString Name;
    QString Link;
    int Type;
};

/**
  * @class DetailDialog
  * DetailDialog is a window showing detailed information for a selected object.
	* The window is split into four Tabs: General, Links, Advanced and Log.
	* The General Tab displays some type-specific data about the object, as well as its
	* present coordinates and Rise/Set/Transit times for the current date.  The Type-specific
	* data are:
	* @li Stars: common name, genetive name, Spectral type, magnitude, distance
	* @li Solar System: name, object type (planet/comet/asteroid), Distance, magnitude (TBD),
	* angular size (TBD)
	* @li Deep Sky: Common name, other names, object type, magnitude, angular size
	*
	* The Links Tab allows the user to manage the list of Image and Information links
	* listed in the object's popup menu.  The Advanced Tab allows the user to query
	* a number of professional-grade online astronomical databases for data on the object.
	* The Log tab allows the user to attach their own text notes about the object.
	*
	* The General Tab includes a clickable image of the object.  Clicking the image opens
	* a Thumbnail picker tool, which downloads a list of mages of the object from the
	* network, which the user may select as the new image for this objects Details window.
	*
	* @author Jason Harris, Jasem Mutlaq
	* @version 1.0
	*/
class DetailDialog : public KPageDialog
{
    Q_OBJECT
  public:
    /** Constructor */
    DetailDialog(SkyObject *o, const KStarsDateTime &ut, GeoLocation *geo, QWidget *parent = nullptr);

    /** Destructor */
    ~DetailDialog() override = default;

    /** @return pointer to the QPixmap of the object's thumbnail image */
    inline QPixmap *thumbnail() { return Thumbnail.get(); }

  public slots:
    /** @short Slot to add this object to the observing list. */
    void addToObservingList();

    /** @short Slot to center this object in the display. */
    void centerMap();

    /** @short Slot to center this object in the telescope. */
    void centerTelescope();

    //TODO: showThumbnail() is only called in the ctor; make it private and not a slot.
    /** @short Slot to display the thumbnail image for the object */
    void showThumbnail();

    /**
     * @short Slot to update thumbnail image for the object, using the Thumbnail
     * Picker tool.
   	 * @sa ThumbnailPicker
     */
    void updateThumbnail();

    /** @short Slot for viewing the selected image or info URL in the web browser. */
    void viewLink();

    /**
     * Popup menu function: Add a custom Image or Information URL.
     * Opens the AddLinkDialog window.
     */
    void addLink();

    /**
     * @short Set the currently-selected URL resource.
     *
     * This function is needed because there are two QListWidgets,
     * each with its own selection.  We need to know which the user selected most recently.
     */
    void setCurrentLink(QListWidgetItem *it);

    /**
     * @short Rebuild the Image and Info URL lists for this object.
   	 * @note used when an item is added to either list.
     */
    void updateLists();

    /**
     * @short Open a dialog to edit a URL in either the Images or Info lists,
     * and update the user's *url.dat file.
     */
    void editLinkDialog();

    /**
     * @short remove a URL entry from either the Images or Info lists, and
     * update the user's *url.dat file.
     */
    void removeLinkDialog();

    /**
     * Open the web browser to the selected online astronomy database,
     * with a query to the object of this Detail Dialog.
     */
    void viewADVData();

    /** Save the User's text in the Log Tab to the userlog.dat file. */
    void saveLogData();

    /** Update View/Edit/Remove buttons */
    void updateButtons();

  private:
    /** Build the General Data Tab for the current object. */
    void createGeneralTab();

    /** Build the Position Tab for the current object. */
    void createPositionTab(const KStarsDateTime &ut, GeoLocation *geo);

    /**
     * Build the Links Tab, populating the image and info lists with the
     * known URLs for the current Object.
     */
    void createLinksTab();

    /** Build the Advanced Tab */
    void createAdvancedTab();

    /** Build the Log Tab */
    void createLogTab();

    /** Populate the TreeView of known astronomical databases in the Advanced Tab */
    void populateADVTree();

    /**
     * Data for the Advanced Tab TreeView is stored in the file advinterface.dat.
     * This function parses advinterface.dat.
     */
    QString parseADVData(const QString &link);

    /**
     * Update the local info_url and image_url files
     * @param type The URL type. 0 for Info Links, 1 for Images.
     * @param search_line The line to be search for in the local URL files
     * @param replace_line The replacement line once search_line is found.
     * @note If replace_line is empty, the function will remove search_line from the file
     */
    void updateLocalDatabase(int type, const QString &search_line, const QString &replace_line = QString());

    SkyObject *selectedObject { nullptr };
    QPalette titlePalette;
    QListWidgetItem *m_CurrentLink { nullptr };
    std::unique_ptr<QPixmap> Thumbnail;
    DataWidget *Data { nullptr };
    DataCometWidget *DataComet { nullptr };
    PositionWidget *Pos { nullptr };
    LinksWidget *Links { nullptr };
    DatabaseWidget *Adv { nullptr };
    LogWidget *Log { nullptr };
    const SkyObjectUserdata::Data &m_user_data;
};

class DataWidget : public QFrame, public Ui::DetailsData
{
  Q_OBJECT

  public:
    explicit DataWidget(QWidget *parent = nullptr);
};

class DataCometWidget : public QFrame, public Ui::DetailsDataComet
{
  Q_OBJECT

  public:
    explicit DataCometWidget(QWidget *parent = nullptr);
};

class PositionWidget : public QFrame, public Ui::DetailsPosition
{
  Q_OBJECT

  public:
    explicit PositionWidget(QWidget *parent = nullptr);
};

class LinksWidget : public QFrame, public Ui::DetailsLinks
{
  Q_OBJECT

  public:
    explicit LinksWidget(QWidget *parent = nullptr);
};

class DatabaseWidget : public QFrame, public Ui::DetailsDatabase
{
  Q_OBJECT

  public:
    explicit DatabaseWidget(QWidget *parent = nullptr);
};

class LogWidget : public QFrame, public Ui::DetailsLog
{
  Q_OBJECT

  public:
    explicit LogWidget(QWidget *parent = nullptr);
};
