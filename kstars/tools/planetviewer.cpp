/*
    SPDX-FileCopyrightText: 2003 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "planetviewer.h"

#include "ksfilereader.h"
#include "ksnumbers.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "skyobjects/ksplanet.h"
#include "skyobjects/ksplanetbase.h"
#include "widgets/timespinbox.h"

#include <KLocalizedString>
#include <KPlotting/KPlotAxis>
#include <KPlotting/KPlotObject>
#include <KPlotting/KPlotPoint>
#include <KPlotting/KPlotWidget>

#include <QFile>
#include <QKeyEvent>
#include <QVBoxLayout>

#include <cmath>

PlanetViewerUI::PlanetViewerUI(QWidget *p) : QFrame(p)
{
    setupUi(this);
}

PlanetViewer::PlanetViewer(QWidget *parent) : QDialog(parent), scale(1.0), isClockRunning(false), tmr(this)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    KStarsData *data = KStarsData::Instance();
    pw               = new PlanetViewerUI(this);

    QVBoxLayout *mainLayout = new QVBoxLayout;

    mainLayout->addWidget(pw);
    setLayout(mainLayout);

    setWindowTitle(i18nc("@title:window", "Solar System Viewer"));
    //setMainWidget( pw );
    //setButtons( QDialog::Close );
    setModal(false);

    pw->map->setLimits(-48.0, 48.0, -48.0, 48.0);
    pw->map->axis(KPlotWidget::BottomAxis)
    ->setLabel(i18nc("axis label for x-coordinate of solar system viewer.  AU means astronomical unit.",
                     "X-position (AU)"));
    pw->map->axis(KPlotWidget::LeftAxis)
    ->setLabel(i18nc("axis label for y-coordinate of solar system viewer.  AU means astronomical unit.",
                     "Y-position (AU)"));

    pw->TimeStep->setDaysOnly(true);
    pw->TimeStep->tsbox()->setValue(1); //start with 1-day timestep

    pw->RunButton->setIcon(QIcon::fromTheme("arrow-right"));
    pw->ZoomInButton->setIcon(QIcon::fromTheme("zoom-in"));
    pw->ZoomOutButton->setIcon(QIcon::fromTheme("zoom-out"));
    pw->DateBox->setDate(data->lt().date());

    resize(500, 500);
    pw->map->QWidget::setFocus(); //give keyboard focus to the plot widget for key and mouse events

    setCenterPlanet(QString());

    PlanetList.append(KSPlanetBase::createPlanet(KSPlanetBase::MERCURY));
    PlanetList.append(KSPlanetBase::createPlanet(KSPlanetBase::VENUS));
    PlanetList.append(new KSPlanet(i18n("Earth")));
    PlanetList.append(KSPlanetBase::createPlanet(KSPlanetBase::MARS));
    PlanetList.append(KSPlanetBase::createPlanet(KSPlanetBase::JUPITER));
    PlanetList.append(KSPlanetBase::createPlanet(KSPlanetBase::SATURN));
    PlanetList.append(KSPlanetBase::createPlanet(KSPlanetBase::URANUS));
    PlanetList.append(KSPlanetBase::createPlanet(KSPlanetBase::NEPTUNE));
    //PlanetList.append( KSPlanetBase::createPlanet( KSPlanetBase::PLUTO ) );

    ut = data->ut();
    KSNumbers num(ut.djd());

    for (int i = 0; i < PlanetList.count(); ++i)
    {
        PlanetList[i]->findPosition(&num, nullptr, nullptr); // nullptr args: don't need geocent. coords.
        LastUpdate[i] = int(ut.date().toJulianDay());
    }

    //The planets' update intervals are 0.25% of one period:
    UpdateInterval[0] = 0;
    UpdateInterval[1] = 0;
    UpdateInterval[2] = 0;
    UpdateInterval[3] = 1;
    UpdateInterval[4] = 5;
    UpdateInterval[5] = 13;
    UpdateInterval[6] = 38;
    UpdateInterval[7] = 75;
    //UpdateInterval[8] = 113;

    QTimer::singleShot(0, this, SLOT(initPlotObjects()));

    connect(&tmr, SIGNAL(timeout()), SLOT(tick()));
    connect(pw->TimeStep, SIGNAL(scaleChanged(float)), SLOT(setTimeScale(float)));
    connect(pw->RunButton, SIGNAL(clicked()), SLOT(slotRunClock()));
    connect(pw->ZoomInButton, SIGNAL(clicked()), pw->map, SLOT(slotZoomIn()));
    connect(pw->ZoomOutButton, SIGNAL(clicked()), pw->map, SLOT(slotZoomOut()));
    connect(pw->DateBox, SIGNAL(dateChanged(QDate)), SLOT(slotChangeDate()));
    connect(pw->TodayButton, SIGNAL(clicked()), SLOT(slotToday()));
    connect(this, SIGNAL(closeClicked()), SLOT(slotCloseWindow()));
}

QString PlanetViewer::planetName(uint i) const
{
    return PlanetList[i]->name();
}

void PlanetViewer::tick()
{
    //Update the time/date
    ut.setDJD(ut.djd() + scale * 0.1);
    pw->DateBox->setDate(ut.date());

    updatePlanets();
}

void PlanetViewer::setTimeScale(float f)
{
    scale = f / 86400.; //convert seconds to days
}

void PlanetViewer::slotRunClock()
{
    isClockRunning = !isClockRunning;

    if (isClockRunning)
    {
        pw->RunButton->setIcon(
            QIcon::fromTheme("media-playback-pause"));
        tmr.start(100);
        //		pw->DateBox->setEnabled( false );
    }
    else
    {
        pw->RunButton->setIcon(QIcon::fromTheme("arrow-right"));
        tmr.stop();
        //		pw->DateBox->setEnabled( true );
    }
}

void PlanetViewer::slotChangeDate()
{
    ut.setDate(pw->DateBox->date());
    updatePlanets();
}

void PlanetViewer::slotCloseWindow()
{
    //Stop the clock if it's running
    if (isClockRunning)
    {
        tmr.stop();
        isClockRunning = false;
        pw->RunButton->setIcon(QIcon::fromTheme("arrow-right"));
    }
}

void PlanetViewer::updatePlanets()
{
    KSNumbers num(ut.djd());
    bool changed(false);

    //Check each planet to see if it needs to be updated
    for (int i = 0; i < PlanetList.count(); ++i)
    {
        if (abs(int(ut.date().toJulianDay()) - LastUpdate[i]) > UpdateInterval[i])
        {
            KSPlanetBase *p = PlanetList[i];
            p->findPosition(&num);

            double s, c, s2, c2;
            p->helEcLong().SinCos(s, c);
            p->helEcLat().SinCos(s2, c2);
            QList<KPlotPoint *> points = planet[i]->points();
            points.at(0)->setX(p->rsun() * c * c2);
            points.at(0)->setY(p->rsun() * s * c2);

            if (centerPlanet() == p->name())
            {
                QRectF dataRect = pw->map->dataRect();
                double xc       = (dataRect.right() + dataRect.left()) * 0.5;
                double yc       = (dataRect.bottom() + dataRect.top()) * 0.5;
                double dx       = points.at(0)->x() - xc;
                double dy       = points.at(0)->y() - yc;
                pw->map->setLimits(dataRect.x() + dx, dataRect.right() + dx, dataRect.y() + dy, dataRect.bottom() + dy);
            }

            LastUpdate[i] = int(ut.date().toJulianDay());
            changed       = true;
        }
    }

    if (changed)
        pw->map->update();
}

void PlanetViewer::slotToday()
{
    pw->DateBox->setDate(KStarsData::Instance()->lt().date());
}

void PlanetViewer::paintEvent(QPaintEvent *)
{
    pw->map->update();
}

void PlanetViewer::initPlotObjects()
{
    // Planets
    ksun = new KPlotObject(Qt::yellow, KPlotObject::Points, 12, KPlotObject::Circle);
    ksun->addPoint(0.0, 0.0);
    pw->map->addPlotObject(ksun);

    //Read in the orbit curves
    for (int i = 0; i < PlanetList.count(); ++i)
    {
        KSPlanetBase *p    = PlanetList[i];
        KPlotObject *orbit = new KPlotObject(Qt::white, KPlotObject::Lines, 1.0);

        QFile orbitFile;
        QString orbitFileName =
            (p->isMajorPlanet() ? (dynamic_cast<KSPlanet *>(p))->untranslatedName().toLower() : p->name().toLower()) + ".orbit";
        if (KSUtils::openDataFile(orbitFile, orbitFileName))
        {
            KSFileReader fileReader(orbitFile); // close file is included
            double x, y;
            while (fileReader.hasMoreLines())
            {
                QString line       = fileReader.readLine();
                QStringList fields = line.split(' ', QString::SkipEmptyParts);
                if (fields.size() == 3)
                {
                    x = fields[0].toDouble();
                    y = fields[1].toDouble();
                    orbit->addPoint(x, y);
                }
            }
        }

        pw->map->addPlotObject(orbit);
    }

    for (int i = 0; i < PlanetList.count(); ++i)
    {
        KSPlanetBase *p = PlanetList[i];
        planet[i]       = new KPlotObject(p->color(), KPlotObject::Points, 6, KPlotObject::Circle);

        double s, c;
        p->helEcLong().SinCos(s, c);

        planet[i]->addPoint(p->rsun() * c, p->rsun() * s, p->translatedName());
        pw->map->addPlotObject(planet[i]);
    }

    update();
}

void PlanetViewer::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape)
        close();
    else
        e->ignore();
}
