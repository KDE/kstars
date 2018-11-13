/***************************************************************************
                          wutdialog.h  -  K Desktop Planetarium
                             -------------------
    begin                : Die Feb 25 2003
    copyright            : (C) 2003 by Thomas Kabelmann
    email                : tk78@gmx.de
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

#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "ui_wutdialog.h"

#include <QFrame>
#include <QDialog>

class GeoLocation;
class SkyObject;

class WUTDialogUI : public QFrame, public Ui::WUTDialog
{
    Q_OBJECT

  public:
    explicit WUTDialogUI(QWidget *p = nullptr);
};

/**
 * @class WUTDialog
 *
 * What's up tonight dialog is a window which lists all sky objects
 * that will be visible during the next night.
 *
 * @author Thomas Kabelmann
 * @version 1.0
 */
class WUTDialog : public QDialog
{
    Q_OBJECT

  public:
    /** Constructor */
    explicit WUTDialog(QWidget *ks, bool session = false, GeoLocation *geo = KStarsData::Instance()->geo(),
                       KStarsDateTime lt = KStarsData::Instance()->lt());
    virtual ~WUTDialog() override = default;

    /**
     * @short Check visibility of object
     * @p o the object to check
     * @return true if visible
     */
    bool checkVisibility(const SkyObject *o);

  public slots:
    /**
     * @short Determine which objects are visible, and store them in
     * an array of lists, classified by object type
     */
    void init();

  private slots:
    /**
     * @short Load the list of visible objects for selected object type.
     * @p category the string describing the type of object
     */
    void slotLoadList(const QString &category);

    /** Display the rise/transit/set times for selected object */
    void slotDisplayObject(const QString &name);

    /**
     * @short Apply user's choice of what part of the night should be examined:
     * @li 0: Evening only (sunset to midnight)
     * @li 1: Morning only (midnight to sunrise)
     * @li 2: All night (sunset to sunrise)
     */
    void slotEveningMorning(int flag);

    /**
     * @short Adjust the date for the WUT tool
     * @note this does NOT affect the date of the sky map
     */
    void slotChangeDate();

    /**
     * @short Adjust the geographic location for the WUT tool
     * @note this does NOT affect the geographic location for the sky map
     */
    void slotChangeLocation();

    /** Open the detail dialog for the current object */
    void slotDetails();

    /** Center the display on the current object */
    void slotCenter();

    /** Add the object to the observing list */
    void slotObslist();

    /** Filters the objects displayed by Magnitude */
    void slotChangeMagnitude();

    void updateMag();

  private:
    QSet<const SkyObject *> &visibleObjects(const QString &category);
    bool isCategoryInitialized(const QString &category);
    /** @short Initialize all SIGNAL/SLOT connections, used in constructor */
    void makeConnections();
    /** @short Initialize category list, used in constructor */
    void initCategories();

    WUTDialogUI *WUT { nullptr };
    bool session { false };
    QTime sunRiseTomorrow, sunSetToday, sunRiseToday, moonRise, moonSet;
    KStarsDateTime T0, UT0, Tomorrow, TomorrowUT, Evening, EveningUT;
    GeoLocation *geo { nullptr };
    int EveningFlag { 0 };
    double m_Mag { 0 };
    QTimer *timer { nullptr };
    QStringList m_Categories;
    QHash<QString, QSet<const SkyObject *>> m_VisibleList;
    QHash<QString, bool> m_CategoryInitialized;
};
