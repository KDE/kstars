/*
    SPDX-FileCopyrightText: 2008 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "skycalendar.h"

#include "geolocation.h"
#include "ksplanetbase.h"
#include "kstarsdata.h"
#include "dialogs/locationdialog.h"
#include "skycomponents/skymapcomposite.h"

#include <KPlotObject>

#include <QPainter>
#include <QPixmap>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QScreen>
#include <QtConcurrent>

SkyCalendarUI::SkyCalendarUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

SkyCalendar::SkyCalendar(QWidget *parent) : QDialog(parent)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    scUI = new SkyCalendarUI(this);

    QVBoxLayout *mainLayout = new QVBoxLayout;

    mainLayout->addWidget(scUI);
    setLayout(mainLayout);

    geo = KStarsData::Instance()->geo();

    setWindowTitle(i18nc("@title:window", "Sky Calendar"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QPushButton *printB = new QPushButton(QIcon::fromTheme("document-print"), i18n("&Print..."));
    printB->setToolTip(i18n("Print the Sky Calendar"));
    buttonBox->addButton(printB, QDialogButtonBox::ActionRole);
    connect(printB, SIGNAL(clicked()), this, SLOT(slotPrint()));

    setModal(false);

    //Adjust minimum size for small screens:
    if (QGuiApplication::primaryScreen()->geometry().height() <= scUI->CalendarView->height())
    {
        scUI->CalendarView->setMinimumSize(400, 600);
    }

    scUI->CalendarView->setShowGrid(false);
    scUI->Year->setValue(KStarsData::Instance()->lt().date().year());

    scUI->LocationButton->setText(geo->fullName());

    scUI->CalendarView->setHorizon();

    plotButtonText = scUI->CreateButton->text();
    connect(scUI->CreateButton, &QPushButton::clicked, [this]()
    {
        scUI->CreateButton->setText(i18n("Please Wait") + "...");
        QtConcurrent::run(this, &SkyCalendar::slotFillCalendar);
    });

    connect(scUI->LocationButton, SIGNAL(clicked()), this, SLOT(slotLocation()));
}

int SkyCalendar::year()
{
    return scUI->Year->value();
}

void SkyCalendar::slotFillCalendar()
{
    scUI->CreateButton->setEnabled(false);

    scUI->CalendarView->resetPlot();
    scUI->CalendarView->setHorizon();

    if (scUI->checkBox_Mercury->isChecked())
        addPlanetEvents(KSPlanetBase::MERCURY);
    if (scUI->checkBox_Venus->isChecked())
        addPlanetEvents(KSPlanetBase::VENUS);
    if (scUI->checkBox_Mars->isChecked())
        addPlanetEvents(KSPlanetBase::MARS);
    if (scUI->checkBox_Jupiter->isChecked())
        addPlanetEvents(KSPlanetBase::JUPITER);
    if (scUI->checkBox_Saturn->isChecked())
        addPlanetEvents(KSPlanetBase::SATURN);
    if (scUI->checkBox_Uranus->isChecked())
        addPlanetEvents(KSPlanetBase::URANUS);
    if (scUI->checkBox_Neptune->isChecked())
        addPlanetEvents(KSPlanetBase::NEPTUNE);

    scUI->CreateButton->setText(i18n("Plot Planetary Almanac"));
    scUI->CreateButton->setEnabled(true);

    //if ( scUI->checkBox_Pluto->isChecked() )
    //addPlanetEvents( KSPlanetBase::PLUTO );

    /*
      {
        QMutexLocker locker(&calculationMutex);

        calculating = false;
        scUI->CreateButton->setEnabled(true);
        scUI->CalendarView->update();
      }
      */
}

#if 0
void SkyCalendar::slotCalculating()
{
    while (calculating)
    {
        //QMutexLocker locker(&calculationMutex);

        if (!calculating)
            continue;

        scUI->CreateButton->setText(i18n("Please Wait") + ".  ");
        scUI->CreateButton->repaint();
        QThread::msleep(200);
        scUI->CreateButton->setText(i18n("Please Wait") + ".. ");
        scUI->CreateButton->repaint();
        QThread::msleep(200);

        scUI->CreateButton->repaint();
        QThread::msleep(200);
    }
    scUI->CreateButton->setText(plotButtonText);
}
#endif

// FIXME: For the time being, adjust with dirty, cluttering labels that don't align to the line
/*
void SkyCalendar::drawEventLabel( float x1, float y1, float x2, float y2, QString LabelText ) {
    QFont origFont = p->font();
    p->setFont( QFont( "Bitstream Vera", 10 ) );

    int textFlags = Qt::AlignCenter; // TODO: See if Qt::SingleLine flag works better
    QFontMetricsF fm( p->font(), p->device() );

    QRectF LabelRect = fm.boundingRect( QRectF(0,0,1,1), textFlags, LabelText );
    QPointF LabelPoint = scUI->CalendarView->mapToWidget( QPointF( x, y ) );

    float LabelAngle = atan2( y2 - y1, x2 - x1 )/dms::DegToRad;

    p->save();
    p->translate( LabelPoint );
    p->rotate( LabelAngle );
    p->drawText( LabelRect, textFlags, LabelText );
    p->restore();

    p->setFont( origFont );
}
*/

void SkyCalendar::addPlanetEvents(int nPlanet)
{
    KSPlanetBase *ksp = KStarsData::Instance()->skyComposite()->planet(nPlanet);
    QColor pColor     = ksp->color();
    //QVector<QPointF> vRise, vSet, vTransit;
    std::vector<QPointF> vRise, vSet, vTransit;

    for (KStarsDateTime kdt(QDate(year(), 1, 1), QTime(12, 0, 0)); kdt.date().year() == year();
            kdt = kdt.addDays(scUI->spinBox_Interval->value()))
    {
        float rTime, sTime, tTime;

        //Compute rise/set/transit times.  If they occur before noon,
        //recompute for the following day
        QTime tmp_rTime = ksp->riseSetTime(kdt, geo, true, true);  //rise time, exact
        QTime tmp_sTime = ksp->riseSetTime(kdt, geo, false, true); //set time, exact
        QTime tmp_tTime = ksp->transitTime(kdt, geo);
        QTime midday(12, 0, 0);

        // NOTE: riseSetTime should be fix now, this test is no longer necessary
        if (tmp_rTime == tmp_sTime)
        {
            tmp_rTime = QTime();
            tmp_sTime = QTime();
        }

        if (tmp_rTime.isValid() && tmp_sTime.isValid())
        {
            rTime = tmp_rTime.secsTo(midday) * 24.0 / 86400.0;
            sTime = tmp_sTime.secsTo(midday) * 24.0 / 86400.0;

            if (tmp_rTime <= midday)
                rTime = 12.0 - rTime;
            else
                rTime = -12.0 - rTime;

            if (tmp_sTime <= midday)
                sTime = 12.0 - sTime;
            else
                sTime = -12.0 - sTime;
        }
        else
        {
            if (ksp->transitAltitude(kdt, geo).degree() > 0)
            {
                rTime = -24.0;
                sTime = 24.0;
            }
            else
            {
                rTime = 24.0;
                sTime = -24.0;
            }
        }

        tTime = tmp_tTime.secsTo(midday) * 24.0 / 86400.0;
        if (tmp_tTime <= midday)
            tTime = 12.0 - tTime;
        else
            tTime = -12.0 - tTime;

        float dy = kdt.date().daysInYear() - kdt.date().dayOfYear();
        vRise.push_back(QPointF(rTime, dy));
        vSet.push_back(QPointF(sTime, dy));
        vTransit.push_back(QPointF(tTime, dy));
    }

    //Now, find continuous segments in each QVector and add each segment
    //as a separate KPlotObject

    KPlotObject *oRise    = new KPlotObject(pColor, KPlotObject::Lines, 2.0);
    KPlotObject *oSet     = new KPlotObject(pColor, KPlotObject::Lines, 2.0);
    KPlotObject *oTransit = new KPlotObject(pColor, KPlotObject::Lines, 2.0);

    float defaultSetTime  = -10.0;
    float defaultRiseTime = 8.0;
    QString label;
    bool initialRise       = true;
    bool initialSet        = true;
    bool initialTransit    = true;
    bool extraCheckRise    = true;
    bool extraCheckSet     = true;
    bool extraCheckTransit = true;
    bool needRiseLabel     = false;
    bool needSetLabel      = false;
    bool needTransertLabel = false;

    float maxRiseTime = 0.0;
    for (auto &item : vRise)
    {
        if (item.x() >= maxRiseTime)
            maxRiseTime = item.x();
    }
    maxRiseTime = qFloor(maxRiseTime) - 1.0;

    for (uint32_t i = 0; i < vRise.size(); ++i)
    {
        if (initialRise && (vRise.at(i).x() > defaultSetTime && vRise.at(i).x() < defaultRiseTime))
        {
            needRiseLabel = true;
            initialRise   = false;
        }
        else if (vRise.at(i).x() < defaultSetTime || vRise.at(i).x() > defaultRiseTime)
        {
            initialRise   = true;
            needRiseLabel = false;
        }
        else
            needRiseLabel = false;

        if (extraCheckRise && vRise.at(i).x() > defaultRiseTime && vRise.at(i).x() < maxRiseTime)
        {
            needRiseLabel  = true;
            extraCheckRise = false;
        }

        if (initialSet && (vSet.at(i).x() > defaultSetTime && vSet.at(i).x() < defaultRiseTime))
        {
            needSetLabel = true;
            initialSet   = false;
        }
        else if (vSet.at(i).x() < defaultSetTime || vSet.at(i).x() > defaultRiseTime)
        {
            initialSet   = true;
            needSetLabel = false;
        }
        else
            needSetLabel = false;

        if (extraCheckSet && vSet.at(i).x() > defaultRiseTime && vSet.at(i).x() < maxRiseTime)
        {
            needSetLabel  = true;
            extraCheckSet = false;
        }

        if (initialTransit && (vTransit.at(i).x() > defaultSetTime && vTransit.at(i).x() < defaultRiseTime))
        {
            needTransertLabel = true;
            initialTransit    = false;
        }
        else if (vTransit.at(i).x() < defaultSetTime || vTransit.at(i).x() > defaultRiseTime)
        {
            initialTransit    = true;
            needTransertLabel = false;
            ;
        }
        else
            needTransertLabel = false;

        if (extraCheckTransit && vTransit.at(i).x() > defaultRiseTime && vTransit.at(i).x() < maxRiseTime)
        {
            needTransertLabel = true;
            extraCheckTransit = false;
        }

        if (vRise.at(i).x() > -23.0 && vRise.at(i).x() < 23.0)
        {
            if (i > 0 && fabs(vRise.at(i).x() - vRise.at(i - 1).x()) > 6.0)
            {
                scUI->CalendarView->addPlotObject(oRise);
                oRise          = new KPlotObject(pColor, KPlotObject::Lines, 2.0);
                extraCheckRise = true;
            }

            if (needRiseLabel)
                label = i18nc("A planet rises from the horizon", "%1 rises", ksp->name());
            else
                label = QString();
            // Add the current point to KPlotObject
            oRise->addPoint(vRise.at(i), label);
        }
        else
        {
            scUI->CalendarView->addPlotObject(oRise);
            oRise = new KPlotObject(pColor, KPlotObject::Lines, 2.0);
        }

        if (vSet.at(i).x() > -23.0 && vSet.at(i).x() < 23.0)
        {
            if (i > 0 && fabs(vSet.at(i).x() - vSet.at(i - 1).x()) > 6.0)
            {
                scUI->CalendarView->addPlotObject(oSet);
                oSet          = new KPlotObject(pColor, KPlotObject::Lines, 2.0);
                extraCheckSet = true;
            }

            if (needSetLabel)
                label = i18nc("A planet sets from the horizon", "%1 sets", ksp->name());
            else
                label = QString();

            oSet->addPoint(vSet.at(i), label);
        }
        else
        {
            scUI->CalendarView->addPlotObject(oSet);
            oSet = new KPlotObject(pColor, KPlotObject::Lines, 2.0);
        }

        if (vTransit.at(i).x() > -23.0 && vTransit.at(i).x() < 23.0)
        {
            if (i > 0 && fabs(vTransit.at(i).x() - vTransit.at(i - 1).x()) > 6.0)
            {
                scUI->CalendarView->addPlotObject(oTransit);
                oTransit          = new KPlotObject(pColor, KPlotObject::Lines, 2.0);
                extraCheckTransit = true;
            }

            if (needTransertLabel)
                label = i18nc("A planet transits across the meridian", "%1 transits", ksp->name());
            else
                label = QString();

            oTransit->addPoint(vTransit.at(i), label);
        }
        else
        {
            scUI->CalendarView->addPlotObject(oTransit);
            oTransit = new KPlotObject(pColor, KPlotObject::Lines, 2.0);
        }
    }

    // Add the last points
    scUI->CalendarView->addPlotObject(oRise);
    scUI->CalendarView->addPlotObject(oSet);
    scUI->CalendarView->addPlotObject(oTransit);
}

void SkyCalendar::slotPrint()
{
    QPainter p;             // Our painter object
    QPrinter printer;       // Our printer object
    QString str_legend;     // Text legend
    QString str_year;       // Calendar's year
    int text_height = 200;  // Height of legend text zone in points
    QSize calendar_size;    // Initial calendar widget size
    QFont calendar_font;    // Initial calendar font
    int calendar_font_size; // Initial calendar font size

    // Set printer resolution to 300 dpi
    printer.setResolution(300);

    // Open print dialog
    //NOTE Changed from pointer to statically allocated object, what effect will it have?
    //QPointer<QPrintDialog> dialog( KdePrint::createPrintDialog( &printer, this ) );
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(i18nc("@title:window", "Print sky calendar"));
    if (dialog.exec() == QDialog::Accepted)
    {
        // Change mouse cursor
        QApplication::setOverrideCursor(Qt::WaitCursor);

        // Save calendar widget font
        calendar_font = scUI->CalendarView->font();
        // Save calendar widget font size
        calendar_font_size = calendar_font.pointSize();
        // Save calendar widget size
        calendar_size = scUI->CalendarView->size();

        // Set text legend
        str_year.setNum(year());
        str_legend = i18n("Sky Calendar");
        str_legend += '\n';
        str_legend += geo->fullName();
        str_legend += " - ";
        str_legend += str_year;

        // Create a rectangle for legend text zone
        QRect text_rect(0, 0, printer.width(), text_height);

        // Increase calendar widget font size so it looks good in 300 dpi
        calendar_font.setPointSize(calendar_font_size * 3);
        scUI->CalendarView->setFont(calendar_font);
        // Increase calendar widget size to fit the entire page
        scUI->CalendarView->resize(printer.width(), printer.height() - text_height);

        // Create a pixmap and render calendar widget into it
        QPixmap pixmap(scUI->CalendarView->size());
        scUI->CalendarView->render(&pixmap);

        // Begin painting on printer
        p.begin(&printer);
        // Draw legend
        p.drawText(text_rect, Qt::AlignLeft, str_legend);
        // Draw calendar
        p.drawPixmap(0, text_height, pixmap);
        // Ending painting
        p.end();

        // Restore calendar widget font size
        calendar_font.setPointSize(calendar_font_size);
        scUI->CalendarView->setFont(calendar_font);
        // Restore calendar widget size
        scUI->CalendarView->resize(calendar_size);

        // Restore mouse cursor
        QApplication::restoreOverrideCursor();
    }
    //delete dialog;
}

void SkyCalendar::slotLocation()
{
    QPointer<LocationDialog> ld = new LocationDialog(this);
    if (ld->exec() == QDialog::Accepted)
    {
        GeoLocation *newGeo = ld->selectedCity();
        if (newGeo)
        {
            geo = newGeo;
            scUI->LocationButton->setText(geo->fullName());
        }
    }
    delete ld;

    scUI->CalendarView->setHorizon();
    slotFillCalendar();
}

GeoLocation *SkyCalendar::get_geo()
{
    return geo;
}
