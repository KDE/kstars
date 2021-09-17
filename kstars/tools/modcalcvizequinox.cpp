/*
    SPDX-FileCopyrightText: 2007 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "modcalcvizequinox.h"

#include "dms.h"
#include "kstarsdata.h"
#include "ksnotification.h"
#include "skyobjects/kssun.h"
#include "widgets/dmsbox.h"

#include <KLineEdit>
#include <KPlotAxis>
#include <KPlotObject>
#include <KPlotPoint>

#include <cmath>

modCalcEquinox::modCalcEquinox(QWidget *parentSplit) : QFrame(parentSplit), dSpring(), dSummer(), dAutumn(), dWinter()
{
    setupUi(this);

    connect(Year, SIGNAL(valueChanged(int)), this, SLOT(slotCompute()));
    connect(InputFileBatch, SIGNAL(urlSelected(QUrl)), this, SLOT(slotCheckFiles()));
    connect(OutputFileBatch, SIGNAL(urlSelected(QUrl)), this, SLOT(slotCheckFiles()));
    connect(RunButtonBatch, SIGNAL(clicked()), this, SLOT(slotRunBatch()));
    connect(ViewButtonBatch, SIGNAL(clicked()), this, SLOT(slotViewBatch()));

    Plot->axis(KPlotWidget::LeftAxis)->setLabel(i18n("Sun's Declination"));
    Plot->setTopPadding(40);
    //Don't draw Top & Bottom axes; custom axes drawn as plot objects
    Plot->axis(KPlotWidget::BottomAxis)->setVisible(false);
    Plot->axis(KPlotWidget::TopAxis)->setVisible(false);

    //This will call slotCompute():
    Year->setValue(KStarsData::Instance()->lt().date().year());

    RunButtonBatch->setEnabled(false);
    ViewButtonBatch->setEnabled(false);

    show();
}

double modCalcEquinox::dmonth(int i)
{
    Q_ASSERT(i >= 0 && i < 12 && "Month must be in 0 .. 11 range");
    return DMonth[i];
}

void modCalcEquinox::slotCheckFiles()
{
    RunButtonBatch->setEnabled(!InputFileBatch->lineEdit()->text().isEmpty() &&
                               !OutputFileBatch->lineEdit()->text().isEmpty());
}

void modCalcEquinox::slotRunBatch()
{
    QString inputFileName = InputFileBatch->url().toLocalFile();

    if (QFile::exists(inputFileName))
    {
        QFile f(inputFileName);
        if (!f.open(QIODevice::ReadOnly))
        {
            KSNotification::sorry(i18n("Could not open file %1.", f.fileName()), i18n("Could Not Open File"));
            inputFileName.clear();
            return;
        }

        QTextStream istream(&f);
        processLines(istream);

        ViewButtonBatch->setEnabled(true);

        f.close();
    }
    else
    {
        KSNotification::sorry(i18n("Invalid file: %1", inputFileName), i18n("Invalid file"));
        inputFileName.clear();
        return;
    }
}

void modCalcEquinox::processLines(QTextStream &istream)
{
    QFile fOut(OutputFileBatch->url().toLocalFile());
    fOut.open(QIODevice::WriteOnly);
    QTextStream ostream(&fOut);
    int originalYear = Year->value();

    //Write header to output file
    ostream << i18n("# Timing of Equinoxes and Solstices\n") << i18n("# computed by KStars\n#\n")
            << i18n("# Vernal Equinox\t\tSummer Solstice\t\t\tAutumnal Equinox\t\tWinter Solstice\n#\n");

    while (!istream.atEnd())
    {
        QString line = istream.readLine();
        bool ok      = false;
        int year     = line.toInt(&ok);

        //for now I will simply change the value of the Year widget to trigger
        //computation of the Equinoxes and Solstices.
        if (ok)
        {
            //triggers slotCompute(), which sets values of dSpring et al.:
            Year->setValue(year);

            //Write to output file
            ostream << QLocale().toString(dSpring.date(), QLocale::LongFormat) << "\t"
                    << QLocale().toString(dSummer.date(), QLocale::LongFormat) << "\t"
                    << QLocale().toString(dAutumn.date(), QLocale::LongFormat) << "\t"
                    << QLocale().toString(dWinter.date(), QLocale::LongFormat) << '\n';
        }
    }

    if (Year->value() != originalYear)
        Year->setValue(originalYear);
}

void modCalcEquinox::slotViewBatch()
{
    QFile fOut(OutputFileBatch->url().toLocalFile());
    fOut.open(QIODevice::ReadOnly);
    QTextStream istream(&fOut);
    QStringList text;

    while (!istream.atEnd())
        text.append(istream.readLine());

    fOut.close();

    KMessageBox::informationList(nullptr, i18n("Results of Sidereal time calculation"), text,
                                 OutputFileBatch->url().toLocalFile());
}

void modCalcEquinox::slotCompute()
{
    KStarsData *data = KStarsData::Instance();
    KSSun Sun;
    int year0 = Year->value();

    KStarsDateTime dt(QDate(year0, 1, 1), QTime(0, 0, 0));
    long double jd0 = dt.djd(); //save JD on Jan 1st
    for (int imonth = 0; imonth < 12; imonth++)
    {
        KStarsDateTime kdt(QDate(year0, imonth + 1, 1), QTime(0, 0, 0));
        DMonth[imonth] = kdt.djd() - jd0;
    }

    Plot->removeAllPlotObjects();

    //Add the celestial equator, just a single line bisecting the plot horizontally
    KPlotObject *ce = new KPlotObject(data->colorScheme()->colorNamed("EqColor"), KPlotObject::Lines, 2.0);
    ce->addPoint(0.0, 0.0);
    ce->addPoint(366.0, 0.0);
    Plot->addPlotObject(ce);

    // Tropic of cancer
    KPlotObject *tcan = new KPlotObject(data->colorScheme()->colorNamed("EqColor"), KPlotObject::Lines, 2.0);
    tcan->addPoint(0.0, 23.5);
    tcan->addPoint(366.0, 23.5);
    Plot->addPlotObject(tcan);

    // Tropic of capricorn
    KPlotObject *tcap = new KPlotObject(data->colorScheme()->colorNamed("EqColor"), KPlotObject::Lines, 2.0);
    tcap->addPoint(0.0, -23.5);
    tcap->addPoint(366.0, -23.5);
    Plot->addPlotObject(tcap);

    //Add Ecliptic.  This is more complicated than simply incrementing the
    //ecliptic longitude, because we want the x-axis to be time, not RA.
    //For each day in the year, compute the Sun's position.
    KPlotObject *ecl = new KPlotObject(data->colorScheme()->colorNamed("EclColor"), KPlotObject::Lines, 2);
    ecl->setLinePen(QPen(ecl->pen().color(), 4));

    Plot->setLimits(1.0, double(dt.date().daysInYear()), -30.0, 30.0);

    //Add top and bottom axis lines, and custom tickmarks at each month
    addDateAxes();

    for (int i = 1; i <= dt.date().daysInYear(); i++)
    {
        KSNumbers num(dt.djd());
        Sun.findPosition(&num);
        ecl->addPoint(double(i), Sun.dec().Degrees());

        dt = dt.addDays(1);
    }
    Plot->addPlotObject(ecl);

    // Calculate dates for all solstices and equinoxes
    findSolsticeAndEquinox(Year->value());

    //Display the Date/Time of each event in the text fields
    VEquinox->setText(QLocale().toString(dSpring, QLocale::LongFormat));
    SSolstice->setText(QLocale().toString(dSummer, QLocale::LongFormat));
    AEquinox->setText(QLocale().toString(dAutumn, QLocale::LongFormat));
    WSolstice->setText(QLocale().toString(dWinter, QLocale::LongFormat));

    //Add vertical dotted lines at times of the equinoxes and solstices
    KPlotObject *poSpring = new KPlotObject(Qt::white, KPlotObject::Lines, 1);
    poSpring->setLinePen(QPen(Qt::white, 1.0, Qt::DotLine));
    poSpring->addPoint(dSpring.djd() - jd0, Plot->dataRect().top());
    poSpring->addPoint(dSpring.djd() - jd0, Plot->dataRect().bottom());
    Plot->addPlotObject(poSpring);
    KPlotObject *poSummer = new KPlotObject(Qt::white, KPlotObject::Lines, 1);
    poSummer->setLinePen(QPen(Qt::white, 1.0, Qt::DotLine));
    poSummer->addPoint(dSummer.djd() - jd0, Plot->dataRect().top());
    poSummer->addPoint(dSummer.djd() - jd0, Plot->dataRect().bottom());
    Plot->addPlotObject(poSummer);
    KPlotObject *poAutumn = new KPlotObject(Qt::white, KPlotObject::Lines, 1);
    poAutumn->setLinePen(QPen(Qt::white, 1.0, Qt::DotLine));
    poAutumn->addPoint(dAutumn.djd() - jd0, Plot->dataRect().top());
    poAutumn->addPoint(dAutumn.djd() - jd0, Plot->dataRect().bottom());
    Plot->addPlotObject(poAutumn);
    KPlotObject *poWinter = new KPlotObject(Qt::white, KPlotObject::Lines, 1);
    poWinter->setLinePen(QPen(Qt::white, 1.0, Qt::DotLine));
    poWinter->addPoint(dWinter.djd() - jd0, Plot->dataRect().top());
    poWinter->addPoint(dWinter.djd() - jd0, Plot->dataRect().bottom());
    Plot->addPlotObject(poWinter);
}

//Add custom top/bottom axes with tickmarks for each month
void modCalcEquinox::addDateAxes()
{
    KPlotObject *poTopAxis = new KPlotObject(Qt::white, KPlotObject::Lines, 1);
    poTopAxis->addPoint(0.0, Plot->dataRect().bottom()); //y-axis is reversed!
    poTopAxis->addPoint(366.0, Plot->dataRect().bottom());
    Plot->addPlotObject(poTopAxis);

    KPlotObject *poBottomAxis = new KPlotObject(Qt::white, KPlotObject::Lines, 1);
    poBottomAxis->addPoint(0.0, Plot->dataRect().top() + 0.02);
    poBottomAxis->addPoint(366.0, Plot->dataRect().top() + 0.02);
    Plot->addPlotObject(poBottomAxis);

    //Tick mark for each month
    for (int imonth = 0; imonth < 12; imonth++)
    {
        KPlotObject *poMonth = new KPlotObject(Qt::white, KPlotObject::Lines, 1);
        poMonth->addPoint(dmonth(imonth), Plot->dataRect().top());
        poMonth->addPoint(dmonth(imonth), Plot->dataRect().top() + 1.4);
        Plot->addPlotObject(poMonth);
        poMonth = new KPlotObject(Qt::white, KPlotObject::Lines, 1);
        poMonth->addPoint(dmonth(imonth), Plot->dataRect().bottom());
        poMonth->addPoint(dmonth(imonth), Plot->dataRect().bottom() - 1.4);
        Plot->addPlotObject(poMonth);
    }
}

// Calculate and store dates for all solstices and equinoxes
void modCalcEquinox::findSolsticeAndEquinox(uint32_t year)
{
    // All the magic numbers are taken from Meeus - 1991 chapter 27
    qreal m, jdme[4];
    if(year > 1000)
    {
        m = (year-2000.0) / 1000.0;
        jdme[0] = (-0.00057 * qPow(m, 4)) + (-0.00411 * qPow(m, 3)) + (0.05169 * qPow(m, 2)) + (365242.37404 * m) + 2451623.80984;
        jdme[1] = (-0.0003 * qPow(m, 4)) + (0.00888 * qPow(m, 3)) + (0.00325 * qPow(m, 2)) + (365241.62603 * m) + 2451716.56767;
        jdme[2] = (0.00078 * qPow(m, 4)) + (0.00337 * qPow(m, 3)) + (-0.11575 * qPow(m, 2)) + (365242.01767 * m) + 2451810.21715;
        jdme[3] = (0.00032 * qPow(m, 4)) + (-0.00823 * qPow(m, 3)) + (-0.06223 * qPow(m, 2)) + (365242.74049 * m) + 2451900.05952;
    }
    else
    {
        m = year / 1000.0;
        jdme[0] = (-0.00071 * qPow(m, 4)) + (0.00111 * qPow(m, 3)) + (0.06134 * qPow(m, 2)) + (365242.13740 * m) + 1721139.29189;
        jdme[1] = (0.00025 * qPow(m, 4)) + (0.00907 * qPow(m, 3)) + (-0.05323 * qPow(m, 2)) + (365241.72562 * m) + 1721233.25401;
        jdme[2] = (0.00074 * qPow(m, 4)) + (-0.00297 * qPow(m, 3)) + (-0.11677 * qPow(m, 2)) + (365242.49558 * m) + 1721325.70455;
        jdme[3] = (-0.00006 * qPow(m, 4)) + (-0.00933 * qPow(m, 3)) + (-0.00769 * qPow(m, 2)) + (365242.88257 * m) + 1721414.39987;
    }

    qreal t[4];
    for(int i = 0; i < 4; i++)
    {
        t[i] = (jdme[i] - 2451545)/36525;
    }

    qreal a[4][24] = {{485, 203, 199, 182, 156, 136, 77, 74, 70, 58, 52, 50, 45, 44, 29, 18, 17, 16, 14, 12, 12, 12, 9, 8}, {485, 203, 199, 182, 156, 136, 77, 74, 70, 58, 52, 50, 45, 44, 29, 18, 17, 16, 14, 12, 12, 12, 9, 8}, {485, 203, 199, 182, 156, 136, 77, 74, 70, 58, 52, 50, 45, 44, 29, 18, 17, 16, 14, 12, 12, 12, 9, 8}, {485, 203, 199, 182, 156, 136, 77, 74, 70, 58, 52, 50, 45, 44, 29, 18, 17, 16, 14, 12, 12, 12, 9, 8}};

    qreal b[4][24] = {{324.96, 337.23, 342.08, 27.85, 73.14, 171.52, 222.54, 296.72, 243.58, 119.81, 297.17, 21.02, 247.54, 325.15, 60.93, 155.12, 288.79, 198.04, 199.76, 95.39, 287.11, 320.81, 227.73, 15.45}, {324.96, 337.23, 342.08, 27.85, 73.14, 171.52, 222.54, 296.72, 243.58, 119.81, 297.17, 21.02, 247.54, 325.15, 60.93, 155.12, 288.79, 198.04, 199.76, 95.39, 287.11, 320.81, 227.73, 15.45}, {324.96, 337.23, 342.08, 27.85, 73.14, 171.52, 222.54, 296.72, 243.58, 119.81, 297.17, 21.02, 247.54, 325.15, 60.93, 155.12, 288.79, 198.04, 199.76, 95.39, 287.11, 320.81, 227.73, 15.45}, {324.96, 337.23, 342.08, 27.85, 73.14, 171.52, 222.54, 296.72, 243.58, 119.81, 297.17, 21.02, 247.54, 325.15, 60.93, 155.12, 288.79, 198.04, 199.76, 95.39, 287.11, 320.81, 227.73, 15.45}};

    qreal c[] = {1934.136, 32964.467, 20.186, 445267.112, 45036.886, 22518.443, 65928.934, 3034.906, 9037.513, 33718.147, 150.678, 2281.226, 29929.562, 31555.956, 4443.417, 67555.328, 4562.452, 62894.029, 31436.921, 14577.848, 31931.756, 34777.259, 1222.114, 16859.074};

    qreal d[4][24];

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 24; j++)
        {
            d[i][j] = c[j] * t[i];
        }
    }

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 24; j++)
        {
            d[i][j] = qCos(qDegreesToRadians(b[i][j] + d[i][j]));
        }
    }

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 24; j++)
        {
            d[i][j] = d[i][j] * a[i][j];
        }
    }

    qreal e[4], f[4], g[4], jd[4];

    for (int i = 0; i < 4; i++)
    {
        e[i] = 0;
        for (int j = 0; j < 24; j++)
        {
            e[i] += d[i][j];
        }
    }

    for (int i = 0; i < 4; i++)
    {
        f[i] = (35999.373*t[i]-2.47);
    }

    for (int i = 0; i < 4; i++)
    {
        g[i] = 1 + (0.0334 * qCos(qDegreesToRadians(f[i]))) + (0.0007 * qCos(qDegreesToRadians(2*f[i])));
    }

    for (int i = 0; i < 4; i++)
    {
        jd[i] = jdme[i] + (0.00001*e[i]/g[i]);
    }

    // Get correction
    qreal correction = FindCorrection(year);

    for (int i = 0; i < 4; i++)
    {
        KStarsDateTime dt(jd[i]);

        // Apply correction
        dt = dt.addSecs(correction);

        switch(i)
        {
            case 0 :
                dSpring = dt;
                break;
            case 1 :
                dSummer = dt;
                break;
            case 2 :
                dAutumn = dt;
                break;
            case 3 :
                dWinter = dt;
                break;
        }
    }
}

qreal modCalcEquinox::FindCorrection(uint32_t year)
{
    uint32_t tblFirst = 1620, tblLast = 2002;

    // Corrections taken from Meeus -1991 chapter 10
    qreal tbl[] = {/*1620*/ 121,112,103, 95, 88,  82, 77, 72, 68, 63,  60, 56, 53, 51, 48,  46, 44, 42, 40, 38,
            /*1660*/  35, 33, 31, 29, 26,  24, 22, 20, 18, 16,  14, 12, 11, 10,  9,   8,  7,  7,  7,  7,
            /*1700*/   7,  7,  8,  8,  9,   9,  9,  9,  9, 10,  10, 10, 10, 10, 10,  10, 10, 11, 11, 11,
            /*1740*/  11, 11, 12, 12, 12,  12, 13, 13, 13, 14,  14, 14, 14, 15, 15,  15, 15, 15, 16, 16,
            /*1780*/  16, 16, 16, 16, 16,  16, 15, 15, 14, 13,
            /*1800*/ 13.1, 12.5, 12.2, 12.0, 12.0,  12.0, 12.0, 12.0, 12.0, 11.9,  11.6, 11.0, 10.2,  9.2,  8.2,
            /*1830*/  7.1,  6.2,  5.6,  5.4,  5.3,   5.4,  5.6,  5.9,  6.2,  6.5,   6.8,  7.1,  7.3,  7.5,  7.6,
            /*1860*/  7.7,  7.3,  6.2,  5.2,  2.7,   1.4, -1.2, -2.8, -3.8, -4.8,  -5.5, -5.3, -5.6, -5.7, -5.9,
            /*1890*/ -6.0, -6.3, -6.5, -6.2, -4.7,  -2.8, -0.1,  2.6,  5.3,  7.7,  10.4, 13.3, 16.0, 18.2, 20.2,
            /*1920*/ 21.1, 22.4, 23.5, 23.8, 24.3,  24.0, 23.9, 23.9, 23.7, 24.0,  24.3, 25.3, 26.2, 27.3, 28.2,
            /*1950*/ 29.1, 30.0, 30.7, 31.4, 32.2,  33.1, 34.0, 35.0, 36.5, 38.3,  40.2, 42.2, 44.5, 46.5, 48.5,
            /*1980*/ 50.5, 52.5, 53.8, 54.9, 55.8,  56.9, 58.3, 60.0, 61.6, 63.0,  63.8, 64.3};

    qreal deltaT = 0;
    qreal t = (year - 2000.0)/100.0;

    if(year >= tblFirst && year <= tblLast)
    {
        if(year % 2)
        {
            deltaT = (tbl[(year-tblFirst-1)/2] + tbl[(year-tblFirst+1)/2]) / 2;
        }
        else
        {
            deltaT = tbl[(year-tblFirst)/2];
        }
    }
    else if(year < 948)
    {
        deltaT = 2177 + 497*t + 44.1*qPow(t, 2);
    }
    else if(year >= 948)
    {
        deltaT =  102 + 102*t + 25.3*qPow(t, 2);
        if (year>=2000 && year <=2100)
        {
            // Special correction to avoid discontinuity in 2000
            deltaT += 0.37 * ( year - 2100.0 );
        }
    }
    return -deltaT;
}
