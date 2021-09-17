/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QMenu>
#include <QHash>

class QAction;

class DeepSkyObject;
class CatalogObject;
class KSMoon;
class Satellite;
class SkyObject;
class SkyPoint;
class StarObject;
class Supernova;

/**
 * @class KSPopupMenu
 * The KStars Popup Menu. The menu is sensitive to the
 * object type of the object which was clicked to invoke the menu.
 * Items in the menu include name and type data; rise/transit/set times;
 * actions such as Center, Details, Telescope actions, and Label;
 * and Image and Information URL links.
 *
 * @author Jason Harris
 * @version 1.0
 */
class KSPopupMenu : public QMenu
{
        Q_OBJECT
    public:
        /** Default constructor*/
        KSPopupMenu();

        /** Destructor (empty)*/
        ~KSPopupMenu() override;

        /**
         * Add an item to the popup menu for each of the URL links associated with
         * this object.  URL links appear in two categories: images and information pages.
         * For some objects, a link to Digitized Sky Survey images will automatically be added
         * in addition to the object's normal image links.  Also, for some objects, an
         * "Add link..." item will be included, which allows the user to add their own custom
         * URLs for this object.
         * @param obj pointer to the skyobject which the menu describes
         * @param showDSS if true, include DSS Image links
         */
        void addLinksToMenu(SkyObject *obj, bool showDSS = true);

        /**
         * @short Create a popup menu for a star.
         *
         * Stars get the following labels: a primary name and/or a genetive name,
         * a spectral type, an object type ("star"), and rise/transit/set times.
         * Stars get a "Center & Track" item, an Angular Distance item, and a
         * "Detailed Info" item.  Named stars get an "Attach Label" item and an
         * "Add Link..." item, and may have image/info links; all stars get DSS
         * image links.  Stars do not get an "Add Trail" item.
         * @param star pointer to the star which the menu describes
         */
        void createStarMenu(StarObject *star);

        /**
         * @short Create a popup menu for a deep-sky catalog object.
         *
         * DSOs get the following labels:
         * a common name and/or a catalog name, an object type, and rise/transit/set
         * times.  DSOs get a "Center & Track" item, an Angular Distance item, an
         * "Attach Label" item, and a "Detailed Info" item.
         * They may have image/info links, and also get the DSS Image links and the
         * "Add Link..." item.  They do not get an "Add Trail" item.
         * @param obj pointer to the object which the menu describes
         */
        void createCatalogObjectMenu(CatalogObject *obj);

        /**
         * @short Create a popup menu for a solar system body.
         *
         * Solar System bodies get a name label, a type label ("solar system object"),
         * and rise/set/transit time labels. They also get Center&Track,
         * Angular Distance, Detailed Info, Attach Label, and Add Trail items.
         * They can have image/info links, and also get the "Add Link..." item.
         * @note despite the name "createPlanetMenu", this function is used for
         * comets and asteroids as well.
         * @param p the solar system object which the menu describes.
         */
        void createPlanetMenu(SkyObject *p);

        void createMoonMenu(KSMoon *moon);

        /**
         * @short Create a popup menu for a satellite.
         * @param satellite the satellite which the menu describes.
         */
        void createSatelliteMenu(Satellite *satellite);

        /**
         * @short Create a popup menu for a supernova
         * @param supernova the supernova which the menu describes.
         */
        void createSupernovaMenu(Supernova *supernova);

        /**
         * @short Create a popup menu for empty sky.
         *
         * The popup menu when right-clicking on nothing is still useful.
         * Instead of a name label, it shows "Empty Sky".  The rise/set/transit
         * times of the clicked point on the sky are also shown.  You also get
         * the Center & Track and Angular Distance items, and the DSS image links.
         * @param nullObj pointer to point on the sky
         */
        void createEmptyMenu(SkyPoint *nullObj);

    private slots:
        void slotEditFlag();
        void slotDeleteFlag();
        void slotEditFlag(QAction *action);
        void slotDeleteFlag(QAction *action);
        void slotViewInWI();

    private:
        /**
         * Initialize the popup menus. Adds name and type labels, and possibly
         * Rise/Set/Transit labels, Center/Track item, and Show Details item.
         * @short initialize the right-click popup menu
         * @param obj pointer to the skyobject which the menu describes
         * @param name The object name
         * @param type a string identifying the object type
         * @param type short information about object
         * @param showDetails if true, the Show-Details item is added
         * @param showObsList if true, the Add to List/Remove from List item is added.
         */
        void initPopupMenu(SkyObject *obj, const QString &name, const QString &type, QString info, bool showDetails = true,
                           bool showObsList = true, bool showFlags = true);

        void initFlagActions(SkyObject *obj);

        /**
         * Add a submenu for INDI controls (Slew, Track, Sync, etc).
         * @return true if a valid INDI menu was added.
         */
        void addINDI();

        /**
         * Add fancy label to menu.
         * @param name is content of the label
         * @param deltaFontSize is change in font size from default
         */
        void addFancyLabel(const QString &name, int deltaFontSize = 0);

        int m_CurrentFlagIdx { 0 };
        QHash<QAction *, int> *m_EditActionMapping { nullptr };
        QHash<QAction *, int> *m_DeleteActionMapping { nullptr };
};
