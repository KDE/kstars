/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ksnumbers.h"
#include "kspaths.h"
#include "kstars_debug.h"
#include "kstarsdata.h"
#include "ksnotification.h"
#include "kstarsdatetime.h"
#include "catalogsdb.h"
#if defined(KSTARS_LITE)
#include "kstarslite.h"
#endif
#include "ksutils.h"
#include "Options.h"
#include "simclock.h"
#include "version.h"
#if !defined(KSTARS_LITE)
#include "kstars.h"
#include "skymap.h"
#endif

#if !defined(KSTARS_LITE)
#include <KAboutData>
#include <KCrash>
#endif
#include <KLocalizedString>

#include <QApplication>
#if !defined(KSTARS_LITE)
#include <QCommandLineParser>
#include <QCommandLineOption>
#endif
#include <QDebug>
#include <QPixmap>
#include <QScreen>
#include <QtGlobal>
#include <QTranslator>

#if defined(Q_OS_LINUX) || defined(Q_OS_OSX)
#include <signal.h>
#endif

#ifndef KSTARS_LITE
static const char description[] = I18N_NOOP("Desktop Planetarium");
static const char notice[]      = I18N_NOOP(
    "Some images in KStars are for non-commercial use only. See README.images.");
#endif

#if defined(Q_OS_ANDROID)
// __attribute__ is needed because clang-based linking removes the main() symbol from the shared library on Android
Q_DECL_EXPORT
#endif
int main(int argc, char *argv[])
{
#if defined(Q_OS_LINUX) || defined(Q_OS_OSX)
    // Ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QApplication app(argc, argv);

#ifdef Q_OS_OSX
    //Note, this function will return true on OS X if the data directories are good to go.  If not, quit with error code 1!
    if (!KSUtils::setupMacKStarsIfNeeded())
    {
        KSNotification::sorry(i18n("Sorry, without a KStars Data Directory, KStars "
                                   "cannot operate. Exiting program now."));
        return 1;
    }
#endif
    Options::setKStarsFirstRun(false);
    app.setApplicationVersion(KSTARS_VERSION);
    /**
    * enable high dpi support
    */
    app.setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    KLocalizedString::setApplicationDomain("kstars");
#if defined(KSTARS_LITE)
#if defined(__ANDROID__)
    KLocalizedString::addDomainLocaleDir(
        "kstars", "/data/data/org.kde.kstars.lite/qt-reserved-files/share/kstars/locale");
#else
    KLocalizedString::addDomainLocaleDir("kstars", "locale");
#endif
#endif

#ifndef KSTARS_LITE

    // Create writable data dir if it does not exist
    QDir writableDir;
    writableDir.mkdir(KSPaths::writableLocation(QStandardPaths::GenericDataLocation));
    writableDir.mkdir(KSPaths::writableLocation(QStandardPaths::TempLocation));

    KCrash::initialize();
    QString versionString =
        QString("%1 %2").arg(KSTARS_VERSION).arg(KSTARS_BUILD_RELEASE);
    KAboutData aboutData(
        "kstars", i18n("KStars"), versionString, i18n(description), KAboutLicense::GPL,
        "2001-" + QString::number(QDate::currentDate().year()) +
            i18n(" (c), The KStars Team\n\nThe Gaussian Process Guider Algorithm: (c) "
                 "2014-2017 Max Planck Society"),
        i18nc("Build number followed by copyright notice", "Build: %1\n\n%2\n\n%3",
              KSTARS_BUILD_TS,
              KSTARS_BUILD_RELEASE == QLatin1String("Beta") ?
                  "Pre-release beta snapshot. Do not use in production." :
                  "Stable release.",
              i18n(notice)),
        "https://edu.kde.org/kstars");
    aboutData.addAuthor(i18n("Jason Harris"), i18n("Original Author"),
                        "jharris@30doradus.org", "http://www.30doradus.org");
    aboutData.addAuthor(i18n("Jasem Mutlaq"), i18n("Current Maintainer"),
                        "mutlaqja@ikarustech.com", "https://www.indilib.org");

    // Active developers
    aboutData.addAuthor(i18n("Akarsh Simha"), QString(), "akarsh@kde.org",
                        "http://www.ph.utexas.edu/~asimha");
    aboutData.addAuthor(i18n("Robert Lancaster"),
                        i18n("FITSViewer & Ekos Improvements. KStars OSX Port"),
                        "rlancaste@gmail.com");
    aboutData.addAuthor(i18n("Csaba Kertesz"), QString(), "csaba.kertesz@gmail.com", "");
    aboutData.addAuthor(i18n("Eric Dejouhanet"), QString(), "eric.dejouhanet@gmail.com",
                        i18n("Ekos Scheduler Improvements"));
    aboutData.addAuthor(i18n("Wolfgang Reissenberger"), QString(),
                        "sterne-jaeger@t-online.de",
                        i18n("Ekos Scheduler & Observatory Improvements"));
    aboutData.addAuthor(i18n("Hy Murveit"), QString(), "murveit@gmail.com",
                        i18n("FITS, Focus, Guide Improvements"));
    aboutData.addAuthor("Valentin Boettcher", QString(), "hiro@protagon.space",
                        i18n("Binary Asteroid List, DSO Database & Catalogs"));

    // Inactive developers
    aboutData.addAuthor(i18n("Artem Fedoskin"), i18n("KStars Lite"),
                        "afedoskin3@gmail.com");
    aboutData.addAuthor(i18n("James Bowlin"), QString(), "bowlin@mindspring.com");
    aboutData.addAuthor(i18n("Pablo de Vicente"), QString(), "pvicentea@wanadoo.es");
    aboutData.addAuthor(i18n("Thomas Kabelmann"), QString(), "tk78@gmx.de");
    aboutData.addAuthor(i18n("Heiko Evermann"), QString(), "heiko@evermann.de",
                        "https://www.evermann.de");
    aboutData.addAuthor(i18n("Carsten Niehaus"), QString(), "cniehaus@gmx.de");
    aboutData.addAuthor(i18n("Mark Hollomon"), QString(), "mhh@mindspring.com");
    aboutData.addAuthor(i18n("Alexey Khudyakov"), QString(), "alexey.skladnoy@gmail.com");
    aboutData.addAuthor(i18n("M&eacute;d&eacute;ric Boquien"), QString(),
                        "mboquien@free.fr");
    aboutData.addAuthor(i18n("J&eacute;r&ocirc;me Sonrier"), QString(),
                        "jsid@emor3j.fr.eu.org");
    aboutData.addAuthor(i18n("Prakash Mohan"), QString(), "prakash.mohan@kdemail.net");
    aboutData.addAuthor(i18n("Victor Cărbune"), QString(), "victor.carbune@kdemail.net");
    aboutData.addAuthor(i18n("Henry de Valence"), QString(), "hdevalence@gmail.com");
    aboutData.addAuthor(i18n("Samikshan Bairagya"), QString(),
                        "samikshan.bairagya@kdemail.net");
    aboutData.addAuthor(i18n("Rafał Kułaga"), QString(), "rl.kulaga@gmail.com");
    aboutData.addAuthor(i18n("Rishab Arora"), QString(), "ra.rishab@gmail.com");

    // Contributors
    aboutData.addCredit(
        i18n("Valery Kharitonov"),
        i18n("Converted labels containing technical terms to links to documentation"));
    aboutData.addCredit(i18n("Ana-Maria Constantin"),
                        i18n("Technical documentation on Astronomy and KStars"));
    aboutData.addCredit(i18n("Andrew Stepanenko"),
                        i18n("Guiding code based on lin_guider"));
    aboutData.addCredit(i18n("Nuno Pinheiro"), i18n("Artwork"));
    aboutData.addCredit(
        i18n("Utkarsh Simha"),
        i18n("Improvements to observation plan execution, star hopper etc."));
    aboutData.addCredit(i18n("Daniel Holler"),
                        i18n("Extensive testing and suggestions for Ekos/INDI."));
    aboutData.addCredit(
        i18n("Stephane Lucas"),
        i18n("Extensive testing and suggestions for Ekos Scheduler. KStars OSX Port"));
    aboutData.addCredit(i18n("Yuri Fabirovsky"),
                        i18n("Splash screen for both regular KStars and KStars Lite."));
    aboutData.addCredit(i18n("Jamie Smith"), i18n("KStars OSX Port."));
    aboutData.addCredit(i18n("Patrick Molenaar"), i18n("Bahtinov Focus Assistant."));

    KAboutData::setApplicationData(aboutData);

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.setApplicationDescription(aboutData.shortDescription());

    //parser.addHelpOption(INSERT_DESCRIPTION_HERE);
    parser.addOption(QCommandLineOption("dump", i18n("Dump sky image to file."), "file"));
    parser.addOption(QCommandLineOption("script", i18n("Script to execute."), "file"));
    parser.addOption(QCommandLineOption("width", i18n("Width of sky image."), "value"));
    parser.addOption(QCommandLineOption("height", i18n("Height of sky image."), "value"));
    parser.addOption(QCommandLineOption("date", i18n("Date and time."), "string"));
    parser.addOption(QCommandLineOption("paused", i18n("Start with clock paused.")));

    // urls to open
    parser.addPositionalArgument(QStringLiteral("urls"), i18n("FITS file(s) to open."),
                                 QStringLiteral("[urls...]"));

    parser.process(app);
    aboutData.processCommandLine(&parser);

    if (parser.isSet("dump"))
    {
        qCDebug(KSTARS) << "Dumping sky image";

        //parse filename and image format
        const char *format = "PNG";
        QString fname      = parser.value("dump");
        QString ext        = fname.mid(fname.lastIndexOf(".") + 1);
        if (ext.toLower() == "png")
        {
            format = "PNG";
        }
        else if (ext.toLower() == "jpg" || ext.toLower() == "jpeg")
        {
            format = "JPG";
        }
        else if (ext.toLower() == "gif")
        {
            format = "GIF";
        }
        else if (ext.toLower() == "pnm")
        {
            format = "PNM";
        }
        else if (ext.toLower() == "bmp")
        {
            format = "BMP";
        }
        else
        {
            qCWarning(KSTARS) << i18n("Could not parse image format of %1; assuming PNG.",
                                      fname);
        }

        //parse width and height
        bool ok(false);
        int w(0), h(0);
        w = parser.value("width").toInt(&ok);
        if (ok)
            h = parser.value("height").toInt(&ok);
        if (!ok)
        {
            qCWarning(KSTARS) << "Unable to parse arguments Width: "
                              << parser.value("width")
                              << "  Height: " << parser.value("height");
            return 1;
        }

        KStarsData *dat = KStarsData::Create();
        QObject::connect(dat, SIGNAL(progressText(QString)), dat,
                         SLOT(slotConsoleMessage(QString)));
        dat->initialize();

        //Set Geographic Location
        dat->setLocationFromOptions();

        //Set color scheme
        dat->colorScheme()->loadFromConfig();

        //set clock now that we have a location:
        //Check to see if user provided a date/time string.  If not, use current CPU time
        QString datestring = parser.value("date");
        KStarsDateTime kdt;
        if (!datestring.isEmpty())
        {
            if (datestring.contains("-")) //assume ISODate format
            {
                if (datestring.contains(":")) //also includes time
                {
                    //kdt = QDateTime::fromString( datestring, QDateTime::ISODate );
                    kdt = KStarsDateTime(QDateTime::fromString(datestring, Qt::ISODate));
                }
                else //string probably contains date only
                {
                    //kdt.setDate( QDate::fromString( datestring, Qt::ISODate ) );
                    kdt.setDate(QDate::fromString(datestring, Qt::ISODate));
                    kdt.setTime(QTime(0, 0, 0));
                }
            }
            else //assume Text format for date string
            {
                kdt = dat->geo()->LTtoUT(
                    KStarsDateTime(QDateTime::fromString(datestring, Qt::TextDate)));
            }

            if (!kdt.isValid())
            {
                qCWarning(KSTARS) << i18n(
                    "Supplied date string is invalid: %1. Using CPU date/time instead.",
                    datestring);

                kdt = KStarsDateTime::currentDateTimeUtc();
            }
        }
        else
        {
            kdt = KStarsDateTime::currentDateTimeUtc();
        }
        dat->clock()->setUTC(kdt);

        SkyMap *map = SkyMap::Create();
        map->resize(w, h);
        QPixmap sky(w, h);

        dat->setFullTimeUpdate();
        dat->updateTime(dat->geo(), map != nullptr);

        SkyPoint dest(Options::focusRA(), Options::focusDec());
        map->setDestination(dest);
        map->destination()->EquatorialToHorizontal(dat->lst(), dat->geo()->lat());
        map->setFocus(map->destination());
        map->focus()->EquatorialToHorizontal(dat->lst(), dat->geo()->lat());

        //Execute the specified script
        QString scriptfile = parser.value("script");
        if (!scriptfile.isEmpty())
        {
            if (dat->executeScript(scriptfile, map))
            {
                std::cout << i18n("Script executed.").toUtf8().data() << std::endl;
            }
            else
            {
                qCWarning(KSTARS) << i18n("Could not execute script.");
            }
        }

        qApp->processEvents();
        map->setupProjector();
        map->exportSkyImage(&sky);
        qApp->processEvents();

        if (!sky.save(fname, format))
            qCWarning(KSTARS) << "Unable to save image: " << fname;
        else
            qCDebug(KSTARS) << "Saved to file: %1" << fname;

        delete map;
        //delete dat;
        return 0;
    }

    //Try to parse the given date string
    QString datestring = parser.value("date");

    if (!datestring.isEmpty() && !KStarsDateTime::fromString(datestring).isValid())
    {
        qWarning() << i18n("Using CPU date/time instead.");
        datestring.clear();
    }

#endif

    // Create writable data dir if it does not exist
    QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).mkpath(".");
    QDir(KSPaths::writableLocation(QStandardPaths::AppConfigLocation)).mkpath(".");
    QDir(KSPaths::writableLocation(QStandardPaths::CacheLocation)).mkpath(".");
    QDir(KSPaths::writableLocation(QStandardPaths::TempLocation)).mkpath(qAppName());

#ifndef KSTARS_LITE
    KStars::createInstance(true, !parser.isSet("paused"), datestring);

    // no session.. just start up normally
    const QStringList urls = parser.positionalArguments();

    // take arguments
    if (!urls.isEmpty())
    {
        foreach (const QString &url, urls)
        {
            const QUrl u = QUrl::fromUserInput(url, QDir::currentPath());

            KStars::Instance()->openFITS(u);
        }
    }
    QObject::connect(qApp, SIGNAL(lastWindowClosed()), qApp, SLOT(quit()));

    app.exec();
#else
    KStarsLite::createInstance(true);

    app.exec();
#endif
    return 0;
}
