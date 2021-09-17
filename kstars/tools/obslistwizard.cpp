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
    connect(olw->TypeList, &QListWidget::itemSelectionChanged, this, &ObsListWizard::slotObjectCountDirty);
    connect(olw->ConstellationList, &QListWidget::itemSelectionChanged, this, &ObsListWizard::slotObjectCountDirty);
    connect(olw->RAMin, &QLineEdit::editingFinished, this, &ObsListWizard::slotParseRegion);
    connect(olw->RAMax, &QLineEdit::editingFinished, this, &ObsListWizard::slotParseRegion);
    connect(olw->DecMin, &QLineEdit::editingFinished, this, &ObsListWizard::slotParseRegion);
    connect(olw->DecMax, &QLineEdit::editingFinished, this, &ObsListWizard::slotParseRegion);
    connect(olw->RA, &QLineEdit::editingFinished, this, &ObsListWizard::slotParseRegion);
    connect(olw->Dec, &QLineEdit::editingFinished, this, &ObsListWizard::slotParseRegion);
    connect(olw->Radius, &QLineEdit::editingFinished, this, &ObsListWizard::slotObjectCountDirty);
    connect(olw->Date, &QDateEdit::dateChanged, this, &ObsListWizard::slotObjectCountDirty);
    connect(olw->Mag, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
            &ObsListWizard::slotObjectCountDirty);
    connect(olw->IncludeNoMag, &QPushButton::clicked, this, &ObsListWizard::slotObjectCountDirty);
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

    connect(olw->SelectByDate, SIGNAL(clicked()), this, SLOT(slotToggleDateWidgets()));
    connect(olw->SelectByMagnitude, SIGNAL(clicked()), this, SLOT(slotToggleMagWidgets()));

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

    olw->RA->setDegType(false);
    olw->RAMin->setDegType(false);
    olw->RAMax->setDegType(false);

    //Initialize object counts
    ObjectCount   = 0; //number of objects in observing list
    StarCount     = data->skyComposite()->stars().size();     //total number of stars
    PlanetCount   = 10;                                       //Sun, Moon, 8 planets
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
            case SkyObject::STAR:
            case SkyObject::CATALOG_STAR:
                StarCount += cnt;
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

bool ObsListWizard::isItemSelected(const QString &name, QListWidget *listWidget, bool *ok)
{
    /*QList<QListWidgetItem *> items = listWidget->findItems(name, Qt::MatchContains);
    if (ok)
        *ok = items.size();
    return items.size() && listWidget->isItemSelected(items[0]);
    ;*/
    foreach(QListWidgetItem *item, listWidget->selectedItems())
    {
        if (item->text().compare(name, Qt::CaseInsensitive) == 0)
        {
            if (ok)
                *ok = true;
            return true;
        }
    }

    if (ok)
        *ok = false;
    return false;
}

void ObsListWizard::setItemSelected(const QString &name, QListWidget *listWidget, bool value, bool *ok)
{
    QList<QListWidgetItem *> items = listWidget->findItems(name, Qt::MatchContains);
    if (ok)
        *ok = items.size();
    if (items.size())
        items[0]->setSelected(value);
}

//Advance to the next page in the stack.  However, on page 2 the user
//selects what regional filter they want to use, and this determines
//what the page following page 2 should be:
// + Constellation(s): the next page index is 3
// + Rectangular region: the next page index is 4
// + Circular region: the next page index is 5
// + No region selected (a.k.a. "all over the sky"): the next page index is 6
//
//Also, if the current page index is 3 or 4, then the next page should be 6.
//
//NOTE: the page indexes are hard-coded here, which isn't ideal.  However,
//There's no easy way to access the pointers of widgets in the stack
//if you didn't save them at the start.
void ObsListWizard::slotNextPage()
{
    int NextPage = olw->olwStack->currentIndex() + 1;

    if (olw->olwStack->currentIndex() == 2)
    {
        //On the Region select page.  Determine what
        //the next page index should be.
        //No need to handle "by constellation, it's already currentIndex + 1.
        if (isItemSelected(i18n("in a rectangular region"), olw->RegionList))
            NextPage = 4;
        if (isItemSelected(i18n("in a circular region"), olw->RegionList))
            NextPage = 5;
        if (isItemSelected(i18n("all over the sky"), olw->RegionList))
            NextPage = 6;
    }

    if (olw->olwStack->currentIndex() == 3 || olw->olwStack->currentIndex() == 4)
        NextPage = 6;

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

    if (olw->olwStack->currentIndex() == 4 || olw->olwStack->currentIndex() == 5 || olw->olwStack->currentIndex() == 6)
        PrevPage = 2;

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

void ObsListWizard::slotChangeLocation()
{
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
    olw->Date->setEnabled(olw->SelectByDate->isChecked());
    olw->LocationButton->setEnabled(olw->SelectByDate->isChecked());
    olw->timeTo->setEnabled(olw->SelectByDate->isChecked());
    olw->timeFrom->setEnabled(olw->SelectByDate->isChecked());
    olw->minAlt->setEnabled(olw->SelectByDate->isChecked());
    olw->maxAlt->setEnabled(olw->SelectByDate->isChecked());

    //    slotUpdateObjectCount();
    slotObjectCountDirty();
}

void ObsListWizard::slotToggleMagWidgets()
{
    olw->Mag->setEnabled(olw->SelectByMagnitude->isChecked());
    olw->IncludeNoMag->setEnabled(olw->SelectByMagnitude->isChecked());
    slotObjectCountDirty();
    //    slotUpdateObjectCount();
}

void ObsListWizard::slotParseRegion()
{
    if (sender()->objectName() == "RAMin" || sender()->objectName() == "RAMax" || sender()->objectName() == "DecMin" ||
            sender()->objectName() == "DecMax")
    {
        if (!olw->RAMin->isEmpty() && !olw->RAMax->isEmpty() && !olw->DecMin->isEmpty() && !olw->DecMax->isEmpty())
        {
            bool rectOk = false;
            xRect1      = 0.0;
            xRect2      = 0.0;
            yRect1      = 0.0;
            yRect2      = 0.0;

            xRect1 = olw->RAMin->createDms(false, &rectOk).Hours();
            if (rectOk)
                xRect2 = olw->RAMax->createDms(false, &rectOk).Hours();
            if (rectOk)
                yRect1 = olw->DecMin->createDms(true, &rectOk).Degrees();
            if (rectOk)
                yRect2 = olw->DecMax->createDms(true, &rectOk).Degrees();
            if (xRect2 == 0.0)
                xRect2 = 24.0;

            if (!rectOk)
            {
                //			qWarning() << i18n( "Illegal rectangle specified, no region selection possible." ) ;
                return;
            }

            //Make sure yRect1 < yRect2.
            if (yRect1 > yRect2)
            {
                double temp = yRect2;
                yRect2      = yRect1;
                yRect1      = temp;
            }

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

            //            slotUpdateObjectCount();
            slotObjectCountDirty();
        }
    }
    else if (!olw->RA->isEmpty() && !olw->Dec->isEmpty() && !olw->Radius->isEmpty())
    {
        bool circOk;
        dms ra = olw->RA->createDms(false, &circOk);
        dms dc;
        if (circOk)
            dc = olw->Dec->createDms(true, &circOk);
        if (circOk)
        {
            pCirc.set(ra, dc);
            rCirc = olw->Radius->createDms(true, &circOk).Degrees();
        }
        else
        {
            qWarning() << i18n("Illegal circle specified, no region selection possible.");
            return;
        }
        //            slotUpdateObjectCount();
        slotObjectCountDirty();
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
    bool filterPass  = true;
    KStarsData *data = KStarsData::Instance();
    if (doBuildList)
        obsList().clear();

    //We don't need to call applyRegionFilter() if no region filter is selected, *and*
    //we are just counting items (i.e., doBuildList is false)
    bool needRegion = true;
    if (!doBuildList && isItemSelected(i18n("all over the sky"), olw->RegionList))
        needRegion = false;

    double maglimit = 100.;
    if (olw->SelectByMagnitude->isChecked())
        maglimit = olw->Mag->value();

    //Stars
    if (isItemSelected(i18n("Stars"), olw->TypeList))
    {
        const QList<SkyObject *> &starList = data->skyComposite()->stars();
        int starIndex(starList.size());
        if (maglimit < 100.)
        {
            //Stars are sorted by mag, so use binary search algo to find index of faintest mag
            int low(0), high(starList.size() - 1), middle(high);
            while (low < high)
            {
                middle = (low + high) / 2;
                if (maglimit == starList.at(middle)->mag())
                    break;
                if (maglimit < starList.at(middle)->mag())
                    high = middle - 1;
                if (maglimit > starList.at(middle)->mag())
                    low = middle + 1;
            }
            //now, the star at "middle" has the right mag, but we want the *last* star that has this mag.
            for (starIndex = middle + 1; starIndex < starList.size(); ++starIndex)
            {
                if (starList.at(starIndex)->mag() > maglimit)
                    break;
            }
        }

        //DEBUG
        qDebug() << QString("starIndex for mag %1: %2").arg(maglimit).arg(starIndex);

        if (!doBuildList)
        {
            //reduce StarCount by appropriate amount
            ObjectCount -= StarCount;
            ObjectCount += starIndex;
        }
        for (int i = 0; i < starIndex; ++i)
        {
            SkyObject *o = (SkyObject *)(starList[i]);

            // JM 2012-10-22: Skip unnamed stars
            if (o->name() == "star")
            {
                if (!doBuildList)
                    --ObjectCount;
                continue;
            }

            if (needRegion)
                filterPass = applyRegionFilter(o, doBuildList, !doBuildList);
            //Filter objects visible from geo at Date if region filter passes
            if (olw->SelectByDate->isChecked() && filterPass)
                applyObservableFilter(o, doBuildList, !doBuildList);
        }
    }

    //Sun, Moon, Planets
    if (isItemSelected(i18n("Sun, moon, planets"), olw->TypeList))
    {
        if (maglimit < data->skyComposite()->findByName(i18n("Sun"))->mag())
        {
            if (!doBuildList)
                --ObjectCount;
            filterPass = false;
        }
        else
            filterPass = true;

        if (needRegion && filterPass)
            filterPass = applyRegionFilter(data->skyComposite()->findByName(i18n("Sun")), doBuildList);
        if (olw->SelectByDate->isChecked() && filterPass)
            applyObservableFilter(data->skyComposite()->findByName(i18n("Sun")), doBuildList);

        if (maglimit < data->skyComposite()->findByName(i18n("Moon"))->mag())
        {
            if (!doBuildList)
                --ObjectCount;
            filterPass = false;
        }
        else
            filterPass = true;

        if (needRegion && filterPass)
            filterPass = applyRegionFilter(data->skyComposite()->findByName(i18n("Moon")), doBuildList);
        if (olw->SelectByDate->isChecked() && filterPass)
            applyObservableFilter(data->skyComposite()->findByName(i18n("Moon")), doBuildList);

        if (maglimit < data->skyComposite()->findByName(i18n("Mercury"))->mag())
        {
            if (!doBuildList)
                --ObjectCount;
            filterPass = false;
        }
        else
            filterPass = true;
        if (needRegion && filterPass)
            filterPass = applyRegionFilter(data->skyComposite()->findByName(i18n("Mercury")), doBuildList);
        if (olw->SelectByDate->isChecked() && filterPass)
            applyObservableFilter(data->skyComposite()->findByName(i18n("Mercury")), doBuildList);

        if (maglimit < data->skyComposite()->findByName(i18n("Venus"))->mag())
        {
            if (!doBuildList)
                --ObjectCount;
            filterPass = false;
        }
        else
            filterPass = true;

        if (needRegion && filterPass)
            filterPass = applyRegionFilter(data->skyComposite()->findByName(i18n("Venus")), doBuildList);
        if (olw->SelectByDate->isChecked() && filterPass)
            applyObservableFilter(data->skyComposite()->findByName(i18n("Venus")), doBuildList);

        if (maglimit < data->skyComposite()->findByName(i18n("Mars"))->mag())
        {
            if (!doBuildList)
                --ObjectCount;
            filterPass = false;
        }
        else
            filterPass = true;
        if (needRegion && filterPass)
            filterPass = applyRegionFilter(data->skyComposite()->findByName(i18n("Mars")), doBuildList);
        if (olw->SelectByDate->isChecked() && filterPass)
            applyObservableFilter(data->skyComposite()->findByName(i18n("Mars")), doBuildList);

        if (maglimit < data->skyComposite()->findByName(i18n("Jupiter"))->mag())
        {
            if (!doBuildList)
                --ObjectCount;
            filterPass = false;
        }
        else
            filterPass = true;
        if (needRegion && filterPass)
            filterPass = applyRegionFilter(data->skyComposite()->findByName(i18n("Jupiter")), doBuildList);
        if (olw->SelectByDate->isChecked() && filterPass)
            applyObservableFilter(data->skyComposite()->findByName(i18n("Jupiter")), doBuildList);

        if (maglimit < data->skyComposite()->findByName(i18n("Saturn"))->mag())
        {
            if (!doBuildList)
                --ObjectCount;
            filterPass = false;
        }
        else
            filterPass = true;

        if (needRegion && filterPass)
            filterPass = applyRegionFilter(data->skyComposite()->findByName(i18n("Saturn")), doBuildList);
        if (olw->SelectByDate->isChecked() && filterPass)
            applyObservableFilter(data->skyComposite()->findByName(i18n("Saturn")), doBuildList);

        if (maglimit < data->skyComposite()->findByName(i18n("Uranus"))->mag())
        {
            if (!doBuildList)
                --ObjectCount;
            filterPass = false;
        }
        else
            filterPass = true;

        if (needRegion && filterPass)
            filterPass = applyRegionFilter(data->skyComposite()->findByName(i18n("Uranus")), doBuildList);
        if (olw->SelectByDate->isChecked() && filterPass)
            applyObservableFilter(data->skyComposite()->findByName(i18n("Uranus")), doBuildList);

        if (maglimit < data->skyComposite()->findByName(i18n("Neptune"))->mag())
        {
            if (!doBuildList)
                --ObjectCount;
            filterPass = false;
        }
        else
            filterPass = true;

        if (needRegion && filterPass)
            filterPass = applyRegionFilter(data->skyComposite()->findByName(i18n("Neptune")), doBuildList);
        if (olw->SelectByDate->isChecked() && filterPass)
            applyObservableFilter(data->skyComposite()->findByName(i18n("Neptune")), doBuildList);

        //        if (maglimit < data->skyComposite()->findByName(i18nc("Asteroid name (optional)", "Pluto"))->mag())
        //        {
        //            if (!doBuildList)
        //                --ObjectCount;
        //            filterPass = false;
        //        }
        //        else
        //            filterPass = true;

        //        if (needRegion && filterPass)
        //            filterPass = applyRegionFilter(data->skyComposite()->findByName(i18nc("Asteroid name (optional)", "Pluto")), doBuildList);
        //        if (olw->SelectByDate->isChecked() && filterPass)
        //            applyObservableFilter(data->skyComposite()->findByName(i18nc("Asteroid name (optional)", "Pluto")), doBuildList);
    }

    //Deep sky objects
    bool dso =
        (isItemSelected(i18n("Open clusters"), olw->TypeList) ||
         isItemSelected(i18n("Globular clusters"), olw->TypeList) ||
         isItemSelected(i18n("Gaseous nebulae"), olw->TypeList) ||
         isItemSelected(i18n("Planetary nebulae"), olw->TypeList) || isItemSelected(i18n("Galaxies"), olw->TypeList));

    if (dso)
    {
        //Don't need to do anything if we are just counting objects and not
        //filtering by region or magnitude
        if (needRegion || olw->SelectByMagnitude->isChecked() || olw->SelectByDate->isChecked())
        {

            CatalogsDB::DBManager manager{ CatalogsDB::dso_db_path() };

            for(auto& o : manager.get_objects(olw->SelectByMagnitude->isChecked() ? maglimit : 99))
            {
                //Skip unselected object types
                bool typeSelected = false;
                // if ( (o->type() == SkyObject::STAR || o->type() == SkyObject::CATALOG_STAR) &&
                //       isItemSelected( i18n( "Stars" ), olw->TypeList ) )
    //                         typeSelected = true;
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

                if (olw->SelectByMagnitude->isChecked())
                {
                    if (o.mag() > 90.)
                    {
                        if (olw->IncludeNoMag->isChecked())
                        {
                            auto *obj = &o;
                            if(doBuildList)
                                obj = &data->skyComposite()->catalogsComponent()
                                           ->insertStaticObject(o);

                            if (needRegion)
                                filterPass = applyRegionFilter(obj, doBuildList);
                            if (olw->SelectByDate->isChecked() && filterPass)
                                applyObservableFilter(obj, doBuildList);
                        }
                        else if (!doBuildList)
                            --ObjectCount;
                    }
                    else
                    {
                        if (o.mag() <= maglimit)
                        {
                            auto *obj = &o;
                            if(doBuildList)
                                obj = &data->skyComposite()->catalogsComponent()
                                           ->insertStaticObject(o);

                            if (needRegion)
                                filterPass = applyRegionFilter(obj, doBuildList);
                            if (olw->SelectByDate->isChecked() && filterPass)
                                applyObservableFilter(obj, doBuildList);
                        }
                        else if (!doBuildList)
                            --ObjectCount;
                    }
                }
                else
                {
                    auto *obj = &o;
                    if(doBuildList)
                        obj = &data->skyComposite()->catalogsComponent()
                                   ->insertStaticObject(o);

                    if (needRegion)
                        filterPass = applyRegionFilter(obj, doBuildList);
                    if (olw->SelectByDate->isChecked() && filterPass)
                        applyObservableFilter(obj, doBuildList);
                }
            }
        }
    }

    //Comets
    if (isItemSelected(i18n("Comets"), olw->TypeList))
    {
        foreach (SkyObject *o, data->skyComposite()->comets())
        {
            if (olw->SelectByMagnitude->isChecked())
            {
                if (o->mag() > 90.)
                {
                    if (olw->IncludeNoMag->isChecked())
                    {
                        if (needRegion)
                            filterPass = applyRegionFilter(o, doBuildList);
                        if (olw->SelectByDate->isChecked() && filterPass)
                            applyObservableFilter(o, doBuildList);
                    }
                    else if (!doBuildList)
                        --ObjectCount;
                }
                else
                {
                    if (o->mag() <= maglimit)
                    {
                        if (needRegion)
                            filterPass = applyRegionFilter(o, doBuildList);
                        if (olw->SelectByDate->isChecked() && filterPass)
                            applyObservableFilter(o, doBuildList);
                    }
                    else if (!doBuildList)
                        --ObjectCount;
                }
            }
            else
            {
                if (needRegion)
                    filterPass = applyRegionFilter(o, doBuildList);
                if (olw->SelectByDate->isChecked() && filterPass)
                    applyObservableFilter(o, doBuildList);
            }
        }
    }

    //Asteroids
    if (isItemSelected(i18n("Asteroids"), olw->TypeList))
    {
        foreach (SkyObject *o, data->skyComposite()->asteroids())
        {
            if (olw->SelectByMagnitude->isChecked())
            {
                if (o->mag() > 90.)
                {
                    if (olw->IncludeNoMag->isChecked())
                    {
                        if (needRegion)
                            filterPass = applyRegionFilter(o, doBuildList);
                        if (olw->SelectByDate->isChecked() && filterPass)
                            applyObservableFilter(o, doBuildList);
                    }
                    else if (!doBuildList)
                        --ObjectCount;
                }
                else
                {
                    if (o->mag() <= maglimit)
                    {
                        if (needRegion)
                            filterPass = applyRegionFilter(o, doBuildList);
                        if (olw->SelectByDate->isChecked() && filterPass)
                            applyObservableFilter(o, doBuildList);
                    }
                    else if (!doBuildList)
                        --ObjectCount;
                }
            }
            else
            {
                if (needRegion)
                    filterPass = applyRegionFilter(o, doBuildList);
                if (olw->SelectByDate->isChecked() && filterPass)
                    applyObservableFilter(o, doBuildList);
            }
        }
    }

    //Update the object count label
    if (doBuildList)
        ObjectCount = obsList().size();

    olw->CountLabel->setText(i18np("Your observing list currently has 1 object",
                                   "Your observing list currently has %1 objects", ObjectCount));
}

bool ObsListWizard::applyRegionFilter(SkyObject *o, bool doBuildList, bool doAdjustCount)
{
    //select by constellation
    if (isItemSelected(i18n("by constellation"), olw->RegionList))
    {
        QString c = KStarsData::Instance()->skyComposite()->constellationBoundary()->constellationName(o);

        if (isItemSelected(c, olw->ConstellationList))
        {
            if (doBuildList)
                obsList().append(o);

            return true;
        }
        else if (doAdjustCount)
        {
            --ObjectCount;
            return false;
        }
        else
            return false;
    }

    //select by rectangular region
    else if (isItemSelected(i18n("in a rectangular region"), olw->RegionList))
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

        else
        {
            if (doAdjustCount)
                --ObjectCount;

            return false;
        }
    }

    //select by circular region
    //make sure circ region data are valid
    else if (isItemSelected(i18n("in a circular region"), olw->RegionList))
    {
        if (o->angularDistanceTo(&pCirc).Degrees() < rCirc)
        {
            if (doBuildList)
                obsList().append(o);

            return true;
        }
        else if (doAdjustCount)
        {
            --ObjectCount;
            return false;
        }
        else
            return false;
    }

    //No region filter, just add the object
    else if (doBuildList)
    {
        obsList().append(o);
    }

    return true;
}

bool ObsListWizard::applyObservableFilter(SkyObject *o, bool doBuildList, bool doAdjustCount)
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

    if (doAdjustCount)
        --ObjectCount;
    if (doBuildList)
        obsList().takeAt(obsList().indexOf(o));

    return false;

    // This is the strict mode where ANY object that does not meet the min & max
    // altitude at ANY time would be removed from the list.
#if 0
    for (KStarsDateTime t = Evening; t < Midnight; t = t.addSecs(3600.0))
    {
        dms LST = geo->GSTtoLST(t.gst());
        p.EquatorialToHorizontal(&LST, geo->lat());
        if (p.alt().Degrees() < minAlt || p.alt().Degrees() > maxAlt)
        {
            if (doAdjustCount)
                --ObjectCount;
            if (doBuildList)
                obsList().takeAt(obsList().indexOf(o));
            return false;
        }
    }
    return true;
#endif

}
