/*
    SPDX-FileCopyrightText: 2005 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "obslistwizard.h"
#include "Options.h"

#include "geolocation.h"
#include "kstarsdata.h"
#include "dialogs/locationdialog.h"
#include "skycomponents/constellationboundarylines.h"
#include "skycomponents/catalogscomponent.h"
#include "skycomponents/skymapcomposite.h"
#include "catalogobject.h"
#include "catalogsdb.h"

ObsListWizardUI::ObsListWizardUI(QWidget *p) : QFrame(p)
{
    setupUi(this);
}

ObsListWizard::ObsListWizard(QWidget *ksparent) : QDialog(ksparent)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    olw                     = new ObsListWizardUI(this);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(olw);
    setLayout(mainLayout);

    setWindowTitle(i18nc("@title:window", "Observing List Wizard"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal);
    nextB                       = new QPushButton(i18n("&Next >"));
    nextB->setDefault(true);
    backB = new QPushButton(i18n("< &Back"));
    backB->setEnabled(false);

    buttonBox->addButton(backB, QDialogButtonBox::ActionRole);
    buttonBox->addButton(nextB, QDialogButtonBox::ActionRole);
    mainLayout->addWidget(buttonBox);

    connect(nextB, SIGNAL(clicked()), this, SLOT(slotNextPage()));
    connect(backB, SIGNAL(clicked()), this, SLOT(slotPrevPage()));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(slotApplyFilters()));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    connect(olw->AllButton, SIGNAL(clicked()), this, SLOT(slotAllButton()));
    connect(olw->NoneButton, SIGNAL(clicked()), this, SLOT(slotNoneButton()));
    connect(olw->DeepSkyButton, SIGNAL(clicked()), this, SLOT(slotDeepSkyButton()));
    connect(olw->SolarSystemButton, SIGNAL(clicked()), this, SLOT(slotSolarSystemButton()));
    connect(olw->LocationButton, SIGNAL(clicked()), this, SLOT(slotChangeLocation()));

    //Update the count of objects when the user asks for it
    connect(olw->updateButton, SIGNAL(clicked()), this, SLOT(slotUpdateObjectCount()));

    // Enable the update count button when certain elements are changed
    // -1- ObjectType
    connect(olw->TypeList, &QListWidget::itemSelectionChanged, this, &ObsListWizard::slotObjectCountDirty);
    // -A- By constellation
    connect(olw->ConstellationList, &QListWidget::itemSelectionChanged, this, &ObsListWizard::slotObjectCountDirty);
    // -B- Rectangular region
    connect(olw->RAMin,  &QLineEdit::editingFinished, this, &ObsListWizard::slotParseRegion);
    connect(olw->RAMax,  &QLineEdit::editingFinished, this, &ObsListWizard::slotParseRegion);
    connect(olw->DecMin, &QLineEdit::editingFinished, this, &ObsListWizard::slotParseRegion);
    connect(olw->DecMax, &QLineEdit::editingFinished, this, &ObsListWizard::slotParseRegion);
    // -C- Circular region
    connect(olw->RA,     &QLineEdit::editingFinished, this, &ObsListWizard::slotParseRegion);
    connect(olw->Dec,    &QLineEdit::editingFinished, this, &ObsListWizard::slotParseRegion);
    connect(olw->Radius, &QLineEdit::editingFinished, this, &ObsListWizard::slotParseRegion);
    // -2- Date
    connect(olw->SelectByDate, SIGNAL(clicked()), this, SLOT(slotToggleDateWidgets()));
    connect(olw->Date, &QDateEdit::dateChanged, this, &ObsListWizard::slotObjectCountDirty);
    connect(olw->timeTo, &QTimeEdit::timeChanged, this, &ObsListWizard::slotObjectCountDirty);
    connect(olw->timeFrom, &QTimeEdit::timeChanged, this, &ObsListWizard::slotObjectCountDirty);
    connect(olw->minAlt, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
            &ObsListWizard::slotObjectCountDirty);
    connect(olw->maxAlt, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
            &ObsListWizard::slotObjectCountDirty);
    olw->coverage->setValue(Options::obsListCoverage());
    connect(olw->coverage, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), [&](double value)
    {
        Options::setObsListCoverage(value);
        slotObjectCountDirty();
    });
    // -3- Magnitude
    connect(olw->SelectByMagnitude, SIGNAL(clicked()), this, SLOT(slotToggleMagWidgets()));
    connect(olw->Mag, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
            &ObsListWizard::slotObjectCountDirty);
    connect(olw->IncludeNoMag, &QPushButton::clicked, this, &ObsListWizard::slotObjectCountDirty);



    geo = KStarsData::Instance()->geo();
    olw->LocationButton->setText(geo->fullName());
    olw->Date->setDate(KStarsDateTime::currentDateTime().date());
    olw->timeFrom->setTime(QTime(18, 0));
    olw->timeTo->setTime(QTime(23, 59));

    initialize();
}

void ObsListWizard::initialize()
{
    KStarsData *data = KStarsData::Instance();
    olw->olwStack->setCurrentIndex(0);

    //Populate the list of constellations
    foreach (SkyObject *p, data->skyComposite()->constellationNames())
        olw->ConstellationList->addItem(p->name());

    //unSelect all object types
    olw->TypeList->clearSelection();

    olw->Mag->setMinimum(-5.0);
    olw->Mag->setMaximum(20.0);
    olw->Mag->setValue(6.0);

    olw->RA->setUnits(dmsBox::HOURS);
    olw->RAMin->setUnits(dmsBox::HOURS);
    olw->RAMax->setUnits(dmsBox::HOURS);

    //Initialize object counts
    ObjectCount   = 0; //number of objects in observing list    

    StarCount     = data->skyComposite()->stars().size();     //total number of stars
    PlanetCount   = std::size(sun_moon_planets_list);         //Sun, Moon, 7 planets (excluding Earth and Pluto)
    AsteroidCount = data->skyComposite()->asteroids().size(); //total number of asteroids
    CometCount    = data->skyComposite()->comets().size();    //total number of comets
    //DeepSkyObjects
    OpenClusterCount = 0;
    GlobClusterCount = 0;
    GasNebCount      = 0;
    PlanNebCount     = 0;
    GalaxyCount      = 0;

    CatalogsDB::DBManager manager{ CatalogsDB::dso_db_path() };

    const auto &stats{ manager.get_master_statistics() };
    if (!stats.first)
        return;

    for (const auto &element : stats.second.object_counts)
    {
        auto cnt = element.second;
        switch (element.first)
        {
            case SkyObject::GALAXY:
                GalaxyCount += cnt;
                break;
            case SkyObject::STAR:// Excluded case SkyObject::CATALOG_STAR later on in filter as well.
                break;
            case SkyObject::OPEN_CLUSTER:
                OpenClusterCount += cnt;
                break;
            case SkyObject::GLOBULAR_CLUSTER:
                GlobClusterCount += cnt;
                break;
            case SkyObject::GASEOUS_NEBULA:
            case SkyObject::SUPERNOVA_REMNANT:
                GasNebCount += cnt;
                break;
            case SkyObject::PLANETARY_NEBULA:
                PlanNebCount += cnt;
                break;
            default:
                break;
        }
    }
}

bool ObsListWizard::isItemSelected(const QString &name, QListWidget *listWidget)
{
    foreach(QListWidgetItem *item, listWidget->selectedItems())
    {
        if (item->text().compare(name, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

void ObsListWizard::setItemSelected(const QString &name, QListWidget *listWidget, bool value)
{
    QList<QListWidgetItem *> items = listWidget->findItems(name, Qt::MatchContains);
    if (items.size())
        items[0]->setSelected(value);
}

//Advance to the next page in the stack.  However, on page 2 the user
//selects what regional filter they want to use, and this determines
//what the page following page 2 should be:
// + Constellation(s):   3
// + Rectangular region: 4
// + Circular region:    5
// + No region selected (a.k.a. ALL_OVER_THE_SKY): 6
//
//Also, if the current page index is 3, 4 or 5, then the next page should be 6.
//
//NOTE: the page indexes are hard-coded here, which isn't ideal.  However,
//there's no easy way to access the pointers of widgets in the stack
//if you didn't save them at the start.
// The order is: MAIN, OBJECT_TYPE, REGION_TYPE, CONSTELLATION, RECTANGULAR, CIRCULAR, DATE, MAGNITUDE
//                  0,           1,           2,             3,           4,        5,    6,         7

void ObsListWizard::slotNextPage()
{
    int NextPage = olw->olwStack->currentIndex() + 1;

    if (olw->olwStack->currentIndex() == PAGE_ID_REGION_TYPE)
    {
        //On the Region select page.  Determine what the next page index should be.
        //No need to handle BY_CONSTELLATION, it's already currentIndex + 1.
        if (isItemSelected(i18n(IN_A_RECTANGULAR_REGION), olw->RegionList))
            NextPage = PAGE_ID_RECTANGULAR;
        else if (isItemSelected(i18n(IN_A_CIRCULAR_REGION), olw->RegionList))
            NextPage = PAGE_ID_CIRCULAR;
        else if (isItemSelected(i18n(ALL_OVER_THE_SKY), olw->RegionList))
            NextPage = PAGE_ID_DATE;
    }

    if ( olw->olwStack->currentIndex() == PAGE_ID_CONSTELLATION ||
         olw->olwStack->currentIndex() == PAGE_ID_RECTANGULAR)
        NextPage = PAGE_ID_DATE;

    olw->olwStack->setCurrentIndex(NextPage);

    if (olw->olwStack->currentIndex() == olw->olwStack->count() - 1)
        nextB->setEnabled(false);

    backB->setEnabled(true);
}

//Advance to the previous page in the stack.  However, because the
//path through the wizard branches depending on the user's choice of
//Region filter, the previous page is not always currentPage-1.
//Specifically, if the current page index is 4, 5, or 6, then the Previous
//page index should be 2 rather than currentIndex-1.
void ObsListWizard::slotPrevPage()
{
    int PrevPage = olw->olwStack->currentIndex() - 1;

    if ( olw->olwStack->currentIndex() == PAGE_ID_RECTANGULAR ||
         olw->olwStack->currentIndex() == PAGE_ID_CIRCULAR ||
         olw->olwStack->currentIndex() == PAGE_ID_DATE)
        PrevPage = PAGE_ID_REGION_TYPE;

    olw->olwStack->setCurrentIndex(PrevPage);

    if (olw->olwStack->currentIndex() == 0)
        backB->setEnabled(false);

    nextB->setEnabled(true);
}

void ObsListWizard::slotAllButton()
{
    for (int i = 0; i < olw->TypeList->count(); ++i)
        olw->TypeList->item(i)->setSelected(true);
}

void ObsListWizard::slotNoneButton()
{
    olw->TypeList->clearSelection();
}

void ObsListWizard::slotDeepSkyButton()
{
    olw->TypeList->clearSelection();
    setItemSelected(i18n("Open clusters"), olw->TypeList, true);
    setItemSelected(i18n("Globular clusters"), olw->TypeList, true);
    setItemSelected(i18n("Gaseous nebulae"), olw->TypeList, true);
    setItemSelected(i18n("Planetary nebulae"), olw->TypeList, true);
    setItemSelected(i18n("Galaxies"), olw->TypeList, true);
}

void ObsListWizard::slotSolarSystemButton()
{
    olw->TypeList->clearSelection();
    setItemSelected(i18n("Sun, moon, planets"), olw->TypeList, true);
    setItemSelected(i18n("Comets"), olw->TypeList, true);
    setItemSelected(i18n("Asteroids"), olw->TypeList, true);
}

void ObsListWizard::slotChangeLocation() {
    QPointer<LocationDialog> ld = new LocationDialog(this);

    if (ld->exec() == QDialog::Accepted)
    {
        //set geographic location
        if (ld->selectedCity())
        {
            geo = ld->selectedCity();
            olw->LocationButton->setText(geo->fullName());
        }
    }
    delete ld;
}

void ObsListWizard::slotToggleDateWidgets()
{
    bool needDate = olw->SelectByDate->isChecked();
    olw->Date->setEnabled(needDate);
    olw->LocationButton->setEnabled(needDate);
    olw->timeTo->setEnabled(needDate);
    olw->timeFrom->setEnabled(needDate);
    olw->minAlt->setEnabled(needDate);
    olw->maxAlt->setEnabled(needDate);

    slotObjectCountDirty();
}

void ObsListWizard::slotToggleMagWidgets()
{
    bool needMagnitude = olw->SelectByMagnitude->isChecked();
    olw->Mag->setEnabled(needMagnitude);
    olw->IncludeNoMag->setEnabled(needMagnitude);

    slotObjectCountDirty();
}

/** Parse the user-entered info to a complete set of parameters if possible. */
void ObsListWizard::slotParseRegion()
{
    if ( isItemSelected(i18n(IN_A_RECTANGULAR_REGION), olw->RegionList) &&
            (sender()->objectName() == "RAMin" || sender()->objectName() == "RAMax" ||
            sender()->objectName() == "DecMin" || sender()->objectName() == "DecMax"))
    {
        if (!olw->RAMin->isEmpty() && !olw->RAMax->isEmpty() &&
            !olw->DecMin->isEmpty() && !olw->DecMax->isEmpty())
        {
            bool rectOk = false;
            xRect1      = 0.0;
            xRect2      = 0.0;
            yRect1      = 0.0;
            yRect2      = 0.0;

            xRect1 = olw->RAMin->createDms(&rectOk).Hours();
            if (rectOk)
                xRect2 = olw->RAMax->createDms(&rectOk).Hours();
            if (rectOk)
                yRect1 = olw->DecMin->createDms(&rectOk).Degrees();
            if (rectOk)
                yRect2 = olw->DecMax->createDms(&rectOk).Degrees();
            if (xRect2 == 0.0)
                xRect2 = 24.0;

            if (!rectOk)
            {
                qWarning() << i18n( "Illegal rectangle specified, no region selection possible." ) ;
                return;
            }

            if (yRect1 > yRect2)
                std::swap(yRect1, yRect2);

            //If xRect1 > xRect2, we may need to swap the two values, or subtract 24h from xRect1.
            if (xRect1 > xRect2)
            {
                if (xRect1 - xRect2 > 12.0) //the user probably wants a region that straddles 0h
                {
                    xRect1 -= 24.0;
                }
                else //the user probably wants xRect2 to be the lower limit
                {
                    double temp = xRect2;
                    xRect2      = xRect1;
                    xRect1      = temp;
                }
            }
            slotObjectCountDirty();
        }
        return; // only one selection possible.
    }    

    if ( isItemSelected(i18n(IN_A_CIRCULAR_REGION), olw->RegionList) &&
              (!olw->RA->isEmpty() && !olw->Dec->isEmpty() && !olw->Radius->isEmpty()))
    {
        bool circOk1;
        bool circOk2;
        bool circOk3;
        dms ra = olw->RA->createDms(&circOk1);
        dms dc = olw->Dec->createDms(&circOk2);
        pCirc.set(ra, dc);
        rCirc = olw->Radius->createDms(&circOk3).Degrees();
        if (circOk1  && circOk2 && circOk3)
            slotObjectCountDirty();
        else
            qWarning() << i18n("Illegal circle specified, no region selection possible.");
    }
}

void ObsListWizard::slotObjectCountDirty()
{
    olw->updateButton->setDisabled(false);
}

void ObsListWizard::slotUpdateObjectCount()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    ObjectCount = 0;
    if (isItemSelected(i18n("Stars"), olw->TypeList))
        ObjectCount += StarCount;
    if (isItemSelected(i18n("Sun, moon, planets"), olw->TypeList))
        ObjectCount += PlanetCount;
    if (isItemSelected(i18n("Comets"), olw->TypeList))
        ObjectCount += CometCount;
    if (isItemSelected(i18n("Asteroids"), olw->TypeList))
        ObjectCount += AsteroidCount;
    if (isItemSelected(i18n("Galaxies"), olw->TypeList))
        ObjectCount += GalaxyCount;
    if (isItemSelected(i18n("Open clusters"), olw->TypeList))
        ObjectCount += OpenClusterCount;
    if (isItemSelected(i18n("Globular clusters"), olw->TypeList))
        ObjectCount += GlobClusterCount;
    if (isItemSelected(i18n("Gaseous nebulae"), olw->TypeList))
        ObjectCount += GasNebCount;
    if (isItemSelected(i18n("Planetary nebulae"), olw->TypeList))
        ObjectCount += PlanNebCount;

    applyFilters(false); //false = only adjust counts, do not build list
    QApplication::restoreOverrideCursor();
    olw->updateButton->setDisabled(true);
}

void ObsListWizard::applyFilters(bool doBuildList)
{
    KStarsData *data = KStarsData::Instance();
    if (doBuildList)
        obsList().clear();

    //We don't need to call applyRegionFilter() if no region filter is selected.
    bool needRegion = !isItemSelected(i18n(ALL_OVER_THE_SKY), olw->RegionList);

    double maglimit = 100.;
    bool needMagnitude = olw->SelectByMagnitude->isChecked();
    bool needNoMagnitude = true;
    if (needMagnitude)
    {
        maglimit        = olw->Mag->value();
        needNoMagnitude = olw->IncludeNoMag->isChecked();
    }
    bool needDate = olw->SelectByDate->isChecked();
    FilterParameters filterParameters = { maglimit, needMagnitude, needNoMagnitude, needRegion, needDate, doBuildList};

    if (isItemSelected(i18n("Stars"), olw->TypeList))
    {
        const QList<SkyObject *> &starList = data->skyComposite()->stars();
        int starIndex(starList.size());
        qDebug() << Q_FUNC_INFO << QString("starIndex: [%1] and maglimit: [%2]").arg(starIndex).arg(maglimit);
        for (int i = 0; i < starIndex; ++i)
        {
            SkyObject *obj = (SkyObject *)(starList[i]);
            applyMagnitudeAndRegionAndObservableFilter(obj, filterParameters);
        }
    }

    if (isItemSelected(i18n("Sun, moon, planets"), olw->TypeList))
    {
        for (auto name : sun_moon_planets_list)
        {
            QString qStringName = i18n(name);
            SkyObject *obj      = data->skyComposite()->findByName(qStringName);
            if (obj == nullptr)
            {
                qWarning() << Q_FUNC_INFO
                           << QString("Failed to find element by name: [%1]").arg(name);
                ObjectCount--;
                continue;
            }
            applyMagnitudeAndRegionAndObservableFilter(obj, filterParameters);
        }
    }

    bool dso = (isItemSelected(i18n("Open clusters"), olw->TypeList) ||
                isItemSelected(i18n("Globular clusters"), olw->TypeList) ||
                isItemSelected(i18n("Gaseous nebulae"), olw->TypeList) ||
                isItemSelected(i18n("Planetary nebulae"), olw->TypeList) ||
                isItemSelected(i18n("Galaxies"), olw->TypeList));

    if (dso)
    {
        CatalogsDB::DBManager manager{ CatalogsDB::dso_db_path() };
        CatalogsDB::CatalogObjectList cObjectList = manager.get_objects_all(); // JFD: Can't skip faint objects because counting down

        for (auto &o : cObjectList)
        {
            //Skip unselected object types
            bool typeSelected = false;
            switch (o.type())
            {
                case SkyObject::OPEN_CLUSTER:
                    if (isItemSelected(i18n("Open clusters"), olw->TypeList))
                        typeSelected = true;
                    break;

                case SkyObject::GLOBULAR_CLUSTER:
                    if (isItemSelected(i18n("Globular clusters"), olw->TypeList))
                        typeSelected = true;
                    break;

                case SkyObject::GASEOUS_NEBULA:
                case SkyObject::SUPERNOVA_REMNANT:
                    if (isItemSelected(i18n("Gaseous nebulae"), olw->TypeList))
                        typeSelected = true;
                    break;

                case SkyObject::PLANETARY_NEBULA:
                    if (isItemSelected(i18n("Planetary nebulae"), olw->TypeList))
                        typeSelected = true;
                    break;
                case SkyObject::GALAXY:
                    if (isItemSelected(i18n("Galaxies"), olw->TypeList))
                        typeSelected = true;
                    break;
            }
            if (!typeSelected)
                continue;

            auto *obj = &o;
            obj       = &data->skyComposite()->catalogsComponent()->insertStaticObject(o);
            applyMagnitudeAndRegionAndObservableFilter(obj, filterParameters);
        } // end for objects
    }

    if (isItemSelected(i18n("Comets"), olw->TypeList))
    {
        foreach (SkyObject *o, data->skyComposite()->comets())
            applyMagnitudeAndRegionAndObservableFilter(o, filterParameters);
    }

    if (isItemSelected(i18n("Asteroids"), olw->TypeList))
    {
        foreach (SkyObject *o, data->skyComposite()->asteroids())
            applyMagnitudeAndRegionAndObservableFilter(o, filterParameters);
    }

    //Update the object count label
    if (doBuildList)
        ObjectCount = obsList().size();

    olw->CountLabel->setText(i18np("Your observing list currently has 1 object",
                                   "Your observing list currently has %1 objects", ObjectCount));
}

bool ObsListWizard::applyMagnitudeAndRegionAndObservableFilter(SkyObject *o, FilterParameters filterParameters )
{
    bool needMagnitude = filterParameters.needMagnitude;
    bool needRegion = filterParameters.needRegion;
    bool needDate = filterParameters.needDate;
    bool doBuildList = filterParameters.doBuildList;

    bool filterPass = true;

    if (filterPass && needMagnitude)
        filterPass = applyMagnitudeFilter(o, filterParameters);
    if (filterPass && (needRegion || doBuildList))
        // Call for adding to obsList even if region filtering is not needed.
        filterPass = applyRegionFilter(o, doBuildList);
    if (filterPass && needDate )
        filterPass = applyObservableFilter(o, doBuildList);
    return filterPass;
}

bool ObsListWizard::applyMagnitudeFilter(SkyObject *o, FilterParameters filterParameters)
{
    bool needMagnitude   = filterParameters.needMagnitude;
    bool needNoMagnitude = filterParameters.needNoMagnitude;
    if (needMagnitude && ((std::isnan(o->mag()) && (!needNoMagnitude)) ||
                          (o->mag() > filterParameters.maglimit)))
    {
        ObjectCount--;
        return false;
    }
    return true;
}

/** This routine will add the object to the obsList if doBuildList.
 *  NB: Call this routine even if you don't need region filtering!*/
bool ObsListWizard::applyRegionFilter(SkyObject *o, bool doBuildList)
{
    //select by constellation
    if (isItemSelected(i18n(BY_CONSTELLATION), olw->RegionList))
    {
        QString constellationName = KStarsData::Instance()
                                        ->skyComposite()
                                        ->constellationBoundary()
                                        ->constellationName(o);

        if (isItemSelected(constellationName, olw->ConstellationList))
        {
            if (doBuildList)
                obsList().append(o);
            return true;
        }
        ObjectCount--;
        return false;
    }

    //select by rectangular region
    else if (isItemSelected(i18n(IN_A_RECTANGULAR_REGION), olw->RegionList))
    {
        double ra      = o->ra().Hours();
        double dec     = o->dec().Degrees();
        bool addObject = false;
        if (dec >= yRect1 && dec <= yRect2)
        {
            if (xRect1 < 0.0)
            {
                addObject = ra >= xRect1 + 24.0 || ra <= xRect2;
            }
            else
            {
                addObject = ra >= xRect1 && ra <= xRect2;
            }
        }

        if (addObject)
        {
            if (doBuildList)
                obsList().append(o);
            return true;
        }
        ObjectCount--;
        return false;
    }

    //select by circular region
    //make sure circ region data are valid
    else if (isItemSelected(i18n(IN_A_CIRCULAR_REGION), olw->RegionList))
    {
        if (o->angularDistanceTo(&pCirc).Degrees() < rCirc)
        {
            if (doBuildList)
                obsList().append(o);
            return true;
        }
        ObjectCount--;
        return false;
    }

    //No region filter, just add the object
    else if (doBuildList)
    {
        obsList().append(o);
    }

    return true;
}

/** This routine will remove any item from the obsList if doBuildList is set which means
 *  it was added before in the previous filter.
 */
bool ObsListWizard::applyObservableFilter(SkyObject *o, bool doBuildList)
{
    SkyPoint p = *o;

    //Check altitude of object every hour from 18:00 to midnight
    //If it's ever above 15 degrees, flag it as visible
    KStarsDateTime Evening(olw->Date->date(), QTime(18, 0, 0), Qt::LocalTime);
    KStarsDateTime Midnight(olw->Date->date().addDays(1), QTime(0, 0, 0), Qt::LocalTime);
    double minAlt = 15, maxAlt = 90;

    // Or use user-selected values, if they're valid
    if (olw->timeFrom->time().isValid() && olw->timeTo->time().isValid())
    {
        Evening.setTime(olw->timeFrom->time());
        Midnight.setTime(olw->timeTo->time());

        // If time from < timeTo (e.g. 06:00 PM to 9:00 PM)
        // then we stay on the same day.
        if (olw->timeFrom->time() < olw->timeTo->time())
        {
            Midnight.setDate(olw->Date->date());
        }
        // Otherwise we advance by one day
        else
        {
            Midnight.setDate(olw->Date->date().addDays(1));
        }
    }

    minAlt = olw->minAlt->value();
    maxAlt = olw->maxAlt->value();

    // This is the "relaxed" search mode
    // where if the object obeys the restrictions in 50% of the time of the range
    // then it qualifies as "visible"
    double totalCount = 0, visibleCount = 0;
    for (KStarsDateTime t = Evening; t < Midnight; t = t.addSecs(3600.0))
    {
        dms LST = geo->GSTtoLST(t.gst());
        p.EquatorialToHorizontal(&LST, geo->lat());
        totalCount++;
        if (p.alt().Degrees() >= minAlt && p.alt().Degrees() <= maxAlt)
            visibleCount++;
    }

    // If the object is within the min/max alt at least coverage % of the time range
    // then consider it visible
    if (visibleCount / totalCount >= olw->coverage->value() / 100.0)
        return true;

    ObjectCount--;
    if (doBuildList)
        obsList().takeAt(obsList().indexOf(o));

    return false;
}
