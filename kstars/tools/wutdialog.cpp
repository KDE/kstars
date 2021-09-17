/*
    SPDX-FileCopyrightText: 2003 Thomas Kabelmann <tk78@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "wutdialog.h"

#include "kstars.h"
#include "skymap.h"
#include "dialogs/detaildialog.h"
#include "dialogs/locationdialog.h"
#include "dialogs/timedialog.h"
#include "skyobjects/kssun.h"
#include "skyobjects/ksmoon.h"
#include "skycomponents/skymapcomposite.h"
#include "tools/observinglist.h"
#include "catalogsdb.h"
#include "Options.h"

WUTDialogUI::WUTDialogUI(QWidget *p) : QFrame(p)
{
    setupUi(this);
}

WUTDialog::WUTDialog(QWidget *parent, bool _session, GeoLocation *_geo,
                     KStarsDateTime _lt)
    : QDialog(parent), session(_session), T0(_lt), geo(_geo)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    WUT = new WUTDialogUI(this);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(WUT);

    setWindowTitle(i18nc("@title:window", "What's up Tonight"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    setModal(false);

    //If the Time is earlier than 6:00 am, assume the user wants the night of the previous date
    if (T0.time().hour() < 6)
        T0 = T0.addDays(-1);

    //Now, set time T0 to midnight (of the following day)
    T0.setTime(QTime(0, 0, 0));
    T0  = T0.addDays(1);
    UT0 = geo->LTtoUT(T0);

    //Set the Tomorrow date/time to Noon the following day
    Tomorrow   = T0.addSecs(12 * 3600);
    TomorrowUT = geo->LTtoUT(Tomorrow);

    //Set the Evening date/time to 6:00 pm
    Evening   = T0.addSecs(-6 * 3600);
    EveningUT = geo->LTtoUT(Evening);

    QString sGeo = geo->translatedName();
    if (!geo->translatedProvince().isEmpty())
        sGeo += ", " + geo->translatedProvince();
    sGeo += ", " + geo->translatedCountry();
    WUT->LocationLabel->setText(i18n("at %1", sGeo));
    WUT->DateLabel->setText(
        i18n("The night of %1", QLocale().toString(Evening.date(), QLocale::LongFormat)));
    m_Mag = 10.0;
    WUT->MagnitudeEdit->setValue(m_Mag);
    //WUT->MagnitudeEdit->setSliderEnabled( true );
    WUT->MagnitudeEdit->setSingleStep(0.100);
    initCategories();

    makeConnections();
}

void WUTDialog::makeConnections()
{
    connect(WUT->DateButton, SIGNAL(clicked()), SLOT(slotChangeDate()));
    connect(WUT->LocationButton, SIGNAL(clicked()), SLOT(slotChangeLocation()));
    connect(WUT->CenterButton, SIGNAL(clicked()), SLOT(slotCenter()));
    connect(WUT->DetailButton, SIGNAL(clicked()), SLOT(slotDetails()));
    connect(WUT->ObslistButton, SIGNAL(clicked()), SLOT(slotObslist()));
    connect(WUT->CategoryListWidget, SIGNAL(currentTextChanged(QString)),
            SLOT(slotLoadList(QString)));
    connect(WUT->ObjectListWidget, SIGNAL(currentTextChanged(QString)),
            SLOT(slotDisplayObject(QString)));
    connect(WUT->EveningMorningBox, SIGNAL(activated(int)),
            SLOT(slotEveningMorning(int)));
    connect(WUT->MagnitudeEdit, SIGNAL(valueChanged(double)),
            SLOT(slotChangeMagnitude()));
}

void WUTDialog::initCategories()
{
    m_Categories << i18n("Planets") << i18n("Stars") << i18n("Nebulae")
                 << i18n("Galaxies") << i18n("Star Clusters") << i18n("Constellations")
                 << i18n("Asteroids") << i18n("Comets");

    foreach (const QString &c, m_Categories)
        WUT->CategoryListWidget->addItem(c);

    WUT->CategoryListWidget->setCurrentRow(0);
}

void WUTDialog::init()
{
    QString sRise, sSet, sDuration;
    float Dur;
    int hDur, mDur;
    KStarsData *data = KStarsData::Instance();
    // reset all lists
    foreach (const QString &c, m_Categories)
    {
        if (m_VisibleList.contains(c))
            visibleObjects(c).clear();
        else
            m_VisibleList[c] = QSet<const SkyObject *>();

        m_CategoryInitialized[c] = false;
    }

    // sun almanac information
    KSSun *oSun = dynamic_cast<KSSun *>(data->objectNamed(i18n("Sun")));

    Q_ASSERT(oSun);

    sunRiseTomorrow = oSun->riseSetTime(TomorrowUT, geo, true);
    sunSetToday     = oSun->riseSetTime(EveningUT, geo, false);
    sunRiseToday    = oSun->riseSetTime(EveningUT, geo, true);

    //check to see if Sun is circumpolar
    KSNumbers *num    = new KSNumbers(UT0.djd());
    KSNumbers *oldNum = new KSNumbers(data->ut().djd());
    CachingDms LST    = geo->GSTtoLST(T0.gst());

    oSun->updateCoords(num, true, geo->lat(), &LST, true);
    if (oSun->checkCircumpolar(geo->lat()))
    {
        if (oSun->alt().Degrees() > 0.0)
        {
            sRise     = i18n("circumpolar");
            sSet      = i18n("circumpolar");
            sDuration = "00:00";
            Dur = hDur = mDur = 0;
        }
        else
        {
            sRise     = i18n("does not rise");
            sSet      = i18n("does not rise");
            sDuration = "24:00";
            Dur = hDur = 24;
            mDur       = 0;
        }
    }
    else
    {
        //Round times to the nearest minute by adding 30 seconds to the time
        sRise = QLocale().toString(sunRiseTomorrow);
        sSet  = QLocale().toString(sunSetToday);

        Dur = 24.0 + (float)sunRiseTomorrow.hour() +
              (float)sunRiseTomorrow.minute() / 60.0 +
              (float)sunRiseTomorrow.second() / 3600.0 - (float)sunSetToday.hour() -
              (float)sunSetToday.minute() / 60.0 - (float)sunSetToday.second() / 3600.0;
        hDur = int(Dur);
        mDur = int(60.0 * (Dur - (float)hDur));
        QTime tDur(hDur, mDur);
        //sDuration = QLocale().toString(tDur);
        // Should always be in 24 hour format
        sDuration = tDur.toString("hh:mm");
    }

    WUT->SunSetLabel->setText(
        i18nc("Sunset at time %1 on date %2", "Sunset: %1 on %2", sSet,
              QLocale().toString(Evening.date(), QLocale::LongFormat)));
    WUT->SunRiseLabel->setText(
        i18nc("Sunrise at time %1 on date %2", "Sunrise: %1 on %2", sRise,
              QLocale().toString(Tomorrow.date(), QLocale::LongFormat)));
    if (Dur == 0)
        WUT->NightDurationLabel->setText(i18n("Night duration: %1", sDuration));
    else if (Dur > 1)
        WUT->NightDurationLabel->setText(i18n("Night duration: %1 hours", sDuration));
    else if (Dur == 1)
        WUT->NightDurationLabel->setText(i18n("Night duration: %1 hour", sDuration));
    else if (mDur > 1)
        WUT->NightDurationLabel->setText(i18n("Night duration: %1 minutes", sDuration));
    else if (mDur == 1)
        WUT->NightDurationLabel->setText(i18n("Night duration: %1 minute", sDuration));

    // moon almanac information
    KSMoon *oMoon = dynamic_cast<KSMoon *>(data->objectNamed(i18n("Moon")));
    moonRise      = oMoon->riseSetTime(UT0, geo, true);
    moonSet       = oMoon->riseSetTime(UT0, geo, false);

    //check to see if Moon is circumpolar
    oMoon->updateCoords(num, true, geo->lat(), &LST, true);
    if (oMoon->checkCircumpolar(geo->lat()))
    {
        if (oMoon->alt().Degrees() > 0.0)
        {
            sRise = i18n("circumpolar");
            sSet  = i18n("circumpolar");
        }
        else
        {
            sRise = i18n("does not rise");
            sSet  = i18n("does not rise");
        }
    }
    else
    {
        //Round times to the nearest minute by adding 30 seconds to the time
        sRise = QLocale().toString(moonRise.addSecs(30));
        sSet  = QLocale().toString(moonSet.addSecs(30));
    }

    WUT->MoonRiseLabel->setText(
        i18n("Moon rises at: %1 on %2", sRise,
             QLocale().toString(Evening.date(), QLocale::LongFormat)));

    // If the moon rises and sets on the same day, this will be valid [ Unless
    // the moon sets on the next day after staying on for over 24 hours ]

    if (moonSet > moonRise)
        WUT->MoonSetLabel->setText(
            i18n("Moon sets at: %1 on %2", sSet,
                 QLocale().toString(Evening.date(), QLocale::LongFormat)));
    else
        WUT->MoonSetLabel->setText(
            i18n("Moon sets at: %1 on %2", sSet,
                 QLocale().toString(Tomorrow.date(), QLocale::LongFormat)));
    oMoon->findPhase(nullptr);
    WUT->MoonIllumLabel->setText(oMoon->phaseName() +
                                 QString(" (%1%)").arg(int(100.0 * oMoon->illum())));

    //Restore Sun's and Moon's coordinates, and recompute Moon's original Phase
    oMoon->updateCoords(oldNum, true, geo->lat(), data->lst(), true);
    oSun->updateCoords(oldNum, true, geo->lat(), data->lst(), true);
    oMoon->findPhase(nullptr);

    if (WUT->CategoryListWidget->currentItem())
        slotLoadList(WUT->CategoryListWidget->currentItem()->text());

    delete num;
    delete oldNum;
}

QSet<const SkyObject *> &WUTDialog::visibleObjects(const QString &category)
{
    return m_VisibleList[category];
}

bool WUTDialog::isCategoryInitialized(const QString &category)
{
    return m_CategoryInitialized[category];
}

QVector<QPair<QString, const SkyObject *>>
WUTDialog::load_dso(const QString &category, const std::vector<SkyObject::TYPE> &types)
{
    CatalogsDB::DBManager db{ CatalogsDB::dso_db_path() };
    QVector<QPair<QString, const SkyObject *>> objects;

    auto &lst = m_CatalogObjects[category];
    lst.clear();

    for (const auto type : types)
    {
        lst.splice(lst.end(), db.get_objects(type, m_Mag));
    }

    objects.reserve(lst.size());
    for (const auto &obj : lst)
    {
        objects.push_back({ obj.name(), &obj });
    }

    return objects;
}

void WUTDialog::slotLoadList(const QString &c)
{
    KStarsData *data = KStarsData::Instance();
    if (!m_VisibleList.contains(c))
        return;

    WUT->ObjectListWidget->clear();
    setCursor(QCursor(Qt::WaitCursor));

    if (!isCategoryInitialized(c))
    {
        if (c == m_Categories[0]) //Planets
        {
            foreach (const QString &name,
                     data->skyComposite()->objectNames(SkyObject::PLANET))
            {
                SkyObject *o = data->skyComposite()->findByName(name);

                if (o->mag() <= m_Mag && checkVisibility(o))
                    visibleObjects(c).insert(o);
            }

            m_CategoryInitialized[c] = true;
        }

        else if (c == m_Categories[1]) //Stars
        {
            QVector<QPair<QString, const SkyObject *>> starObjects;
            starObjects.append(data->skyComposite()->objectLists(SkyObject::STAR));
            starObjects.append(
                data->skyComposite()->objectLists(SkyObject::CATALOG_STAR));
            starObjects.append(load_dso(c, { SkyObject::STAR, SkyObject::CATALOG_STAR }));

            for (const auto &object : starObjects)
            {
                const SkyObject *o = object.second;

                if (o->mag() <= m_Mag && checkVisibility(o))
                {
                    visibleObjects(c).insert(o);
                }
            }
            m_CategoryInitialized[c] = true;
        }

        else if (c == m_Categories[5]) //Constellations
        {
            foreach (SkyObject *o, data->skyComposite()->constellationNames())
                if (checkVisibility(o))
                    visibleObjects(c).insert(o);

            m_CategoryInitialized[c] = true;
        }

        else if (c == m_Categories[6]) //Asteroids
        {
            foreach (SkyObject *o, data->skyComposite()->asteroids())
                if (o->mag() <= m_Mag &&
                    o->name() != i18nc("Asteroid name (optional)", "Pluto") &&
                    checkVisibility(o))
                    visibleObjects(c).insert(o);

            m_CategoryInitialized[c] = true;
        }

        else if (c == m_Categories[7]) //Comets
        {
            foreach (SkyObject *o, data->skyComposite()->comets())
                if (o->mag() <= m_Mag && checkVisibility(o))
                    visibleObjects(c).insert(o);

            m_CategoryInitialized[c] = true;
        }

        else //all deep-sky objects, need to split clusters, nebulae and galaxies
        {
            auto dsos{ load_dso(c,
                                { SkyObject::OPEN_CLUSTER, SkyObject::GLOBULAR_CLUSTER,
                                  SkyObject::GASEOUS_NEBULA, SkyObject::PLANETARY_NEBULA,
                                  SkyObject::SUPERNOVA_REMNANT, SkyObject::SUPERNOVA,
                                  SkyObject::GALAXY }) };

            for (auto &dso : dsos)
            {
                const SkyObject *o = dso.second;
                if (checkVisibility(o) && o->mag() <= m_Mag)
                {
                    switch (o->type())
                    {
                        case SkyObject::OPEN_CLUSTER: //fall through
                        case SkyObject::GLOBULAR_CLUSTER:
                            visibleObjects(m_Categories[4]).insert(o); //star clusters
                            break;
                        case SkyObject::GASEOUS_NEBULA:   //fall through
                        case SkyObject::PLANETARY_NEBULA: //fall through
                        case SkyObject::SUPERNOVA:        //fall through
                        case SkyObject::SUPERNOVA_REMNANT:
                            visibleObjects(m_Categories[2]).insert(o); //nebulae
                            break;
                        case SkyObject::GALAXY:
                            visibleObjects(m_Categories[3]).insert(o); //galaxies
                            break;
                    }
                }
            }

            m_CategoryInitialized[m_Categories[2]] = true;
            m_CategoryInitialized[m_Categories[3]] = true;
            m_CategoryInitialized[m_Categories[4]] = true;
        }
    }

    //Now the category has been initialized, we can populate the list widget
    foreach (const SkyObject *o, visibleObjects(c))
        //WUT->ObjectListWidget->addItem(o->name());
        WUT->ObjectListWidget->addItem(o->longname());

    setCursor(QCursor(Qt::ArrowCursor));

    // highlight first item
    if (WUT->ObjectListWidget->count())
    {
        WUT->ObjectListWidget->setCurrentRow(0);
        WUT->ObjectListWidget->setFocus();
    }
}

bool WUTDialog::checkVisibility(const SkyObject *o)
{
    bool visible(false);
    double minAlt =
        6.0; //An object is considered 'visible' if it is above horizon during civil twilight.

    // reject objects that never rise
    if (o->checkCircumpolar(geo->lat()) == true && o->alt().Degrees() <= 0)
        return false;

    //Initial values for T1, T2 assume all night option of EveningMorningBox
    KStarsDateTime T1 = Evening;
    T1.setTime(sunSetToday);
    KStarsDateTime T2 = Tomorrow;
    T2.setTime(sunRiseTomorrow);

    //Check Evening/Morning only state:
    if (EveningFlag == 0) //Evening only
    {
        T2 = T0; //midnight
    }
    else if (EveningFlag == 1) //Morning only
    {
        T1 = T0; //midnight
    }

    for (KStarsDateTime test = T1; test < T2; test = test.addSecs(3600))
    {
        //Need LST of the test time, expressed as a dms object.
        KStarsDateTime ut = geo->LTtoUT(test);
        dms LST           = geo->GSTtoLST(ut.gst());
        SkyPoint sp       = o->recomputeCoords(ut, geo);

        //check altitude of object at this time.
        sp.EquatorialToHorizontal(&LST, geo->lat());

        if (sp.alt().Degrees() > minAlt)
        {
            visible = true;
            break;
        }
    }

    return visible;
}

void WUTDialog::slotDisplayObject(const QString &name)
{
    QTime tRise, tSet, tTransit;
    QString sRise, sTransit, sSet;

    sRise    = "--:--";
    sTransit = "--:--";
    sSet     = "--:--";
    WUT->DetailButton->setEnabled(false);

    SkyObject *o = nullptr;
    if (name.isEmpty())
    {
        //no object selected
        WUT->ObjectBox->setTitle(i18n("No Object Selected"));
        o = nullptr;
    }
    else
    {
        o = KStarsData::Instance()->objectNamed(name);

        if (!o) //should never get here
        {
            WUT->ObjectBox->setTitle(i18n("Object Not Found"));
        }
    }
    if (o)
    {
        WUT->ObjectBox->setTitle(o->name());

        if (o->checkCircumpolar(geo->lat()))
        {
            if (o->alt().Degrees() > 0.0)
            {
                sRise = i18n("circumpolar");
                sSet  = i18n("circumpolar");
            }
            else
            {
                sRise = i18n("does not rise");
                sSet  = i18n("does not rise");
            }
        }
        else
        {
            tRise = o->riseSetTime(T0, geo, true);
            tSet  = o->riseSetTime(T0, geo, false);
            //          if ( tSet < tRise )
            //              tSet = o->riseSetTime( JDTomorrow, geo, false );

            sRise.clear();
            sRise.asprintf("%02d:%02d", tRise.hour(), tRise.minute());
            sSet.clear();
            sSet.asprintf("%02d:%02d", tSet.hour(), tSet.minute());
        }

        tTransit = o->transitTime(T0, geo);
        //      if ( tTransit < tRise )
        //          tTransit = o->transitTime( JDTomorrow, geo );

        sTransit.clear();
        sTransit.sprintf("%02d:%02d", tTransit.hour(), tTransit.minute());

        WUT->DetailButton->setEnabled(true);
    }

    WUT->ObjectRiseLabel->setText(i18n("Rises at: %1", sRise));
    WUT->ObjectTransitLabel->setText(i18n("Transits at: %1", sTransit));
    WUT->ObjectSetLabel->setText(i18n("Sets at: %1", sSet));
}

void WUTDialog::slotCenter()
{
    KStars *kstars = KStars::Instance();
    SkyObject *o   = nullptr;
    // get selected item
    if (WUT->ObjectListWidget->currentItem() != nullptr)
    {
        o = kstars->data()->objectNamed(WUT->ObjectListWidget->currentItem()->text());
    }
    if (o != nullptr)
    {
        kstars->map()->setFocusPoint(o);
        kstars->map()->setFocusObject(o);
        kstars->map()->setDestination(*kstars->map()->focusPoint());
    }
}

void WUTDialog::slotDetails()
{
    KStars *kstars = KStars::Instance();
    SkyObject *o   = nullptr;
    // get selected item
    if (WUT->ObjectListWidget->currentItem() != nullptr)
    {
        o = kstars->data()->objectNamed(WUT->ObjectListWidget->currentItem()->text());
    }
    if (o != nullptr)
    {
        QPointer<DetailDialog> detail =
            new DetailDialog(o, kstars->data()->ut(), geo, kstars);
        detail->exec();
        delete detail;
    }
}
void WUTDialog::slotObslist()
{
    SkyObject *o = nullptr;
    // get selected item
    if (WUT->ObjectListWidget->currentItem() != nullptr)
    {
        o = KStarsData::Instance()->objectNamed(
            WUT->ObjectListWidget->currentItem()->text());
    }
    if (o != nullptr)
    {
        KStarsData::Instance()->observingList()->slotAddObject(o, session);
    }
}

void WUTDialog::slotChangeDate()
{
    // Set the time T0 to the evening of today. This will make life easier for the user, who most probably
    // wants to see what's up on the night of some date, rather than the night of the previous day
    T0.setTime(QTime(18, 0, 0)); // 6 PM

    QPointer<TimeDialog> td = new TimeDialog(T0, KStarsData::Instance()->geo(), this);
    if (td->exec() == QDialog::Accepted)
    {
        T0 = td->selectedDateTime();

        // If the time is set to 12 midnight, set it to 00:00:01, so that we don't have date interpretation problems
        if (T0.time() == QTime(0, 0, 0))
            T0.setTime(QTime(0, 0, 1));

        //If the Time is earlier than 6:00 am, assume the user wants the night of the previous date
        if (T0.time().hour() < 6)
            T0 = T0.addDays(-1);

        //Now, set time T0 to midnight (of the following day)
        T0.setTime(QTime(0, 0, 0));
        T0  = T0.addDays(1);
        UT0 = geo->LTtoUT(T0);

        //Set the Tomorrow date/time to Noon the following day
        Tomorrow   = T0.addSecs(12 * 3600);
        TomorrowUT = geo->LTtoUT(Tomorrow);

        //Set the Evening date/time to 6:00 pm
        Evening   = T0.addSecs(-6 * 3600);
        EveningUT = geo->LTtoUT(Evening);

        WUT->DateLabel->setText(i18n(
            "The night of %1", QLocale().toString(Evening.date(), QLocale::LongFormat)));

        init();
        slotLoadList(WUT->CategoryListWidget->currentItem()->text());
    }
    delete td;
}

void WUTDialog::slotChangeLocation()
{
    QPointer<LocationDialog> ld = new LocationDialog(this);
    if (ld->exec() == QDialog::Accepted)
    {
        GeoLocation *newGeo = ld->selectedCity();
        if (newGeo)
        {
            geo        = newGeo;
            UT0        = geo->LTtoUT(T0);
            TomorrowUT = geo->LTtoUT(Tomorrow);
            EveningUT  = geo->LTtoUT(Evening);

            WUT->LocationLabel->setText(i18n("at %1", geo->fullName()));

            init();
            slotLoadList(WUT->CategoryListWidget->currentItem()->text());
        }
    }
    delete ld;
}

void WUTDialog::slotEveningMorning(int index)
{
    if (EveningFlag != index)
    {
        EveningFlag = index;
        init();
        slotLoadList(WUT->CategoryListWidget->currentItem()->text());
    }
}

void WUTDialog::updateMag()
{
    m_Mag = WUT->MagnitudeEdit->value();
    init();
    slotLoadList(WUT->CategoryListWidget->currentItem()->text());
}

void WUTDialog::slotChangeMagnitude()
{
    if (timer)
    {
        timer->stop();
    }
    else
    {
        timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, SIGNAL(timeout()), this, SLOT(updateMag()));
    }
    timer->start(500);
}

void WUTDialog::showEvent(QShowEvent *event)
{
    Q_UNUSED(event);
    QTimer::singleShot(0, this, SLOT(init()));
};
