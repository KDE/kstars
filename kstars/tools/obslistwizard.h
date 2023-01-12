/*
    SPDX-FileCopyrightText: 2005 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_obslistwizard.h"
#include "skyobjects/skypoint.h"

#include <QDialog>

class QListWidget;
class QPushButton;

class SkyObject;
class GeoLocation;

struct FilterParameters
{
    double maglimit;
    bool needMagnitude;
    bool needNoMagnitude;
    bool needRegion;
    bool needDate;
    bool doBuildList;
};

class ObsListWizardUI : public QFrame, public Ui::ObsListWizard
{
    Q_OBJECT

  public:
    explicit ObsListWizardUI(QWidget *p);
};

/**
 * @class ObsListWizard
 * @short Wizard for constructing observing lists
 *
 * @author Jason Harris
 */
class ObsListWizard : public QDialog
{
    Q_OBJECT
  public:
    explicit ObsListWizard(QWidget *parent);
    virtual ~ObsListWizard() override = default;

    /** @return reference to QPtrList of objects selected by the wizard */
    QList<SkyObject *> &obsList() { return ObsList; }

  private slots:
    void slotNextPage();
    void slotPrevPage();
    void slotAllButton();
    void slotNoneButton();
    void slotDeepSkyButton();
    void slotSolarSystemButton();
    void slotChangeLocation();
    void slotToggleDateWidgets();
    void slotToggleMagWidgets();

    void slotParseRegion();

    /** @short Construct the observing list by applying the selected filters */
    void slotObjectCountDirty();
    void slotUpdateObjectCount();
    void slotApplyFilters() { applyFilters(true); }

  private:
    void initialize();
    void applyFilters(bool doBuildList);

    /** @return true if the object passes the filter region constraints, false otherwise.*/
    bool applyMagnitudeAndRegionAndObservableFilter(SkyObject *o, FilterParameters filterParameters);
    bool applyMagnitudeFilter(SkyObject *o, FilterParameters filterParameters);
    bool applyRegionFilter(SkyObject *o, bool doBuildList);
    bool applyObservableFilter(SkyObject *o, bool doBuildList);

    /**
     * Convenience function for safely getting the selected state of a QListWidget item by name.
     * QListWidget has no method for easily selecting a single item based on its text.
     * @return true if the named QListWidget item is selected.
     * @param name the QListWidget item to be queried is the one whose text matches this string
     * @param listWidget pointer to the QListWidget whose item is to be queried
     * @param ok pointer to a bool, which if present will return true if a matching list item was found
     */
    bool isItemSelected(const QString &name, QListWidget *listWidget);
    /**
     * Convenience function for safely setting the selected state of a QListWidget item by name.
     * QListWidget has no method for easily selecting a single item based on its text.
     * @param name the QListWidget item to be (de)selected is the one whose text matches this string
     * @param listWidget pointer to the QListWidget whose item is to be (de)selected
     * @param value set the item's selected state to this bool value
     */
    void setItemSelected(const QString &name, QListWidget *listWidget, bool value);

    QList<SkyObject *> ObsList;
    ObsListWizardUI *olw { nullptr };
    uint ObjectCount { 0 };
    uint StarCount { 0 };
    uint PlanetCount { 0 };
    uint CometCount { 0 };
    uint AsteroidCount { 0 };
    uint GalaxyCount { 0 };
    uint OpenClusterCount { 0 };
    uint GlobClusterCount { 0 };
    uint GasNebCount { 0 };
    uint PlanNebCount { 0 };
    const char* sun_moon_planets_list[9] = { // Move this def? And pNames in modCalcPlanets.cpp etc...
        "Sun" ,"Moon" ,"Mercury" ,"Venus" ,"Mars" ,"Jupiter" ,"Saturn" ,"Uranus" ,"Neptune" };
    const char* ALL_OVER_THE_SKY = "all over the sky";
    const char* BY_CONSTELLATION = "by constellation";
    const char* IN_A_RECTANGULAR_REGION = "in a rectangular region";
    const char* IN_A_CIRCULAR_REGION = "in a circular region";
    const int PAGE_ID_MAIN          = 0;
    const int PAGE_ID_OBJECT_TYPE   = 1;
    const int PAGE_ID_REGION_TYPE   = 2;
    const int PAGE_ID_CONSTELLATION = 3;
    const int PAGE_ID_RECTANGULAR   = 4;
    const int PAGE_ID_CIRCULAR      = 5;
    const int PAGE_ID_DATE          = 6;
    const int PAGE_ID_MAGNITUDE     = 7;

    double xRect1 { 0 };
    double xRect2 { 0 };
    double yRect1 { 0 };
    double yRect2 { 0 };
    double rCirc { 0 };
    SkyPoint pCirc;
    GeoLocation *geo { nullptr };
    QPushButton *nextB { nullptr };
    QPushButton *backB { nullptr };

};

