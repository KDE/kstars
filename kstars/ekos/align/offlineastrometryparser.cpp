/*  Astrometry.net Parser
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "offlineastrometryparser.h"

#include "align.h"
#include "ekos_align_debug.h"
#include "ksutils.h"
#include "Options.h"
#include "kspaths.h"

#include <KMessageBox>

namespace Ekos
{
OfflineAstrometryParser::OfflineAstrometryParser() : AstrometryParser()
{
    astrometryIndex[2.8]  = "index-4200";
    astrometryIndex[4.0]  = "index-4201";
    astrometryIndex[5.6]  = "index-4202";
    astrometryIndex[8]    = "index-4203";
    astrometryIndex[11]   = "index-4204";
    astrometryIndex[16]   = "index-4205";
    astrometryIndex[22]   = "index-4206";
    astrometryIndex[30]   = "index-4207";
    astrometryIndex[42]   = "index-4208";
    astrometryIndex[60]   = "index-4209";
    astrometryIndex[85]   = "index-4210";
    astrometryIndex[120]  = "index-4211";
    astrometryIndex[170]  = "index-4212";
    astrometryIndex[240]  = "index-4213";
    astrometryIndex[340]  = "index-4214";
    astrometryIndex[480]  = "index-4215";
    astrometryIndex[680]  = "index-4216";
    astrometryIndex[1000] = "index-4217";
    astrometryIndex[1400] = "index-4218";
    astrometryIndex[2000] = "index-4219";

    // Reset parity on solver failure
    connect(this, &OfflineAstrometryParser::solverFailed, this, [&]()
    {
        parity = QString();
    });

}

bool OfflineAstrometryParser::init()
{
#ifdef Q_OS_OSX
    if (Options::astrometryConfFileIsInternal())
    {
        if(KSUtils::configureAstrometry() == false)
        {
            KMessageBox::information(
                nullptr,
                i18n(
                    "Failed to properly configure astrometry config file.  Please click the options button in the lower right of the Astrometry Tab in Ekos to correct your settings.  Then try starting Ekos again."),
                i18n("Astrometry Config File Error"), "astrometry_configuration_failure_warning");
            return false;
        }
    }
#endif

    if (astrometryFilesOK)
        return true;

    if (astrometryNetOK() == false)
    {
        if (align && align->isEnabled())
            KMessageBox::information(
                nullptr,
                i18n(
                    "Failed to find astrometry.net binaries. Please click the options button in the lower right of the Astrometry Tab in Ekos to correct your settings and make sure that astrometry.net is installed. Then try starting Ekos again."),
                i18n("Missing astrometry files"), "missing_astrometry_binaries_warning");

        return false;
    }

    astrometryFilesOK = true;

    QString solverPath;

    if (Options::astrometrySolverIsInternal())
        solverPath = QCoreApplication::applicationDirPath() + "/astrometry/bin/solve-field";
    else
        solverPath = Options::astrometrySolverBinary();

    QProcess solveField;

    solveField.start("bash", QStringList() << "-c"
                     << (solverPath + " --help | grep Revision"));
    solveField.waitForFinished();
    QString output = solveField.readAllStandardOutput();
    qCDebug(KSTARS_EKOS_ALIGN) << "solve-field Revision" << output;

    if (output.isEmpty() == false)
    {
        QString version = output.mid(9, 4);

        align->appendLogText(i18n("Detected Astrometry.net version %1", version));

        if (version <= "0.67" && Options::astrometryUseNoFITS2FITS() == false)
        {
            Options::setAstrometryUseNoFITS2FITS(true);
            align->appendLogText(i18n("Setting astrometry option --no-fits2fits"));
        }
        else if (version > "0.67" && Options::astrometryUseNoFITS2FITS())
        {
            Options::setAstrometryUseNoFITS2FITS(false);
            align->appendLogText(i18n("Turning off option --no-fits2fits"));
        }
    }

    return true;
}

bool OfflineAstrometryParser::astrometryNetOK()
{
    bool solverOK = false, wcsinfoOK = false;

    if (Options::astrometrySolverIsInternal())
    {
        QFileInfo solverFileInfo(QCoreApplication::applicationDirPath() + "/astrometry/bin/solve-field");
        solverOK = solverFileInfo.exists() && solverFileInfo.isFile();
    }
    else
    {
        QFileInfo solverFileInfo(Options::astrometrySolverBinary());
        solverOK = solverFileInfo.exists() && solverFileInfo.isFile();

#ifdef Q_OS_LINUX
        QString confPath = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Literal("astrometry") + QLatin1Literal("/astrometry.cfg");
        if (QFileInfo(confPath).exists() == false)
            createLocalAstrometryConf();
#endif
    }

    if (Options::astrometryWCSIsInternal())
    {
        QFileInfo wcsFileInfo(QCoreApplication::applicationDirPath() + "/astrometry/bin/wcsinfo");
        wcsinfoOK = wcsFileInfo.exists() && wcsFileInfo.isFile();
    }
    else
    {
        QFileInfo wcsFileInfo(Options::astrometryWCSInfo());
        wcsinfoOK = wcsFileInfo.exists() && wcsFileInfo.isFile();
    }

    return (solverOK && wcsinfoOK);
}

void OfflineAstrometryParser::verifyIndexFiles(double fov_x, double fov_y)
{
    static double last_fov_x = 0, last_fov_y = 0;

    if (last_fov_x == fov_x && last_fov_y == fov_y)
        return;

    last_fov_x       = fov_x;
    last_fov_y       = fov_y;
    double fov_lower = 0.10 * fov_x;
    double fov_upper = fov_x;
    QStringList indexFiles;
    QString astrometryDataDir;
    bool indexesOK = true;

#ifdef Q_OS_OSX
    if (KSUtils::getAstrometryDataDir(astrometryDataDir) == false)
        return;
#endif

    QStringList nameFilter("*.fits");
    QDir directory(astrometryDataDir);
    QStringList indexList = directory.entryList(nameFilter);

    // JM 2018-09-26: Also add locally stored indexes.
#ifdef Q_OS_LINUX
    QDir localAstrometry(KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Literal("astrometry"));
    indexList << localAstrometry.entryList(nameFilter);
#endif

    QString indexSearch   = indexList.join(" ");
    QString startIndex, lastIndex;
    unsigned int missingIndexes = 0;

    foreach (float skymarksize, astrometryIndex.keys())
    {
        if (skymarksize >= fov_lower && skymarksize <= fov_upper)
        {
            indexFiles << astrometryIndex.value(skymarksize);

            if (indexSearch.contains(astrometryIndex.value(skymarksize)) == false)
            {
                if (startIndex.isEmpty())
                    startIndex = astrometryIndex.value(skymarksize);

                lastIndex = astrometryIndex.value(skymarksize);

                indexesOK = false;

                missingIndexes++;
            }
        }
    }

    if (indexesOK == false)
    {
        if (missingIndexes == 1)
            align->appendLogText(
                i18n("Index file %1 is missing. Astrometry.net would not be able to adequately solve plates until you "
                     "install the missing index files. Download the index files from http://www.astrometry.net",
                     startIndex));
        else
            align->appendLogText(i18n("Index files %1 to %2 are missing. Astrometry.net would not be able to "
                                      "adequately solve plates until you install the missing index files. Download the "
                                      "index files from http://www.astrometry.net",
                                      startIndex, lastIndex));
    }
}

bool OfflineAstrometryParser::getAstrometryDataDir(QString &dataDir)
{
    QString confPath;

    if (Options::astrometryConfFileIsInternal())
        confPath = QCoreApplication::applicationDirPath() + "/astrometry/bin/astrometry.cfg";
    else
        confPath = Options::astrometryConfFile();

    QFile confFile(confPath);

    if (confFile.open(QIODevice::ReadOnly) == false)
    {
        KMessageBox::error(nullptr, i18n("Astrometry configuration file corrupted or missing: %1\nPlease set the "
                                         "configuration file full path in INDI options.",
                                         Options::astrometryConfFile()));
        return false;
    }

    QTextStream in(&confFile);
    QString line;
    while (!in.atEnd())
    {
        line = in.readLine();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        line = line.trimmed();
        if (line.startsWith(QLatin1String("add_path")))
        {
            dataDir = line.mid(9).trimmed();
            return true;
        }
    }

    KMessageBox::error(nullptr, i18n("Unable to find data dir in astrometry configuration file."));
    return false;
}

bool OfflineAstrometryParser::startSovler(const QString &filename, const QStringList &args, bool generated)
{
    INDI_UNUSED(generated);

    QStringList solverArgs = args;

    // Use parity if it is: 1. Already known from previous solve. 2. This is NOT a blind solve
    if (Options::astrometryDetectParity() && (parity.isEmpty() == false) && (args.contains("parity") == false) &&
            (args.contains("-3") || args.contains("-L")))
        solverArgs << "--parity" << parity;

    QString confPath;
    if (Options::astrometryConfFileIsInternal())
        confPath = QCoreApplication::applicationDirPath() + "/astrometry/bin/astrometry.cfg";
    else
    {
        // JM 2018-09-26: On Linux, load the local config file.
#ifdef Q_OS_LINUX
        confPath = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Literal("astrometry") + QLatin1Literal("/astrometry.cfg");
#else
        confPath = Options::astrometryConfFile();
#endif
    }
    solverArgs << "--config" << confPath;

    QString solutionFile = QDir::tempPath() + "/solution.wcs";
    solverArgs << "-W" << solutionFile << filename;

    fitsFile = filename;

    solver.clear();
    solver = new QProcess(this);

#ifdef Q_OS_OSX
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path            = env.value("PATH", "");
    if (Options::astrometrySolverIsInternal())
    {
        env.insert("PATH", QCoreApplication::applicationDirPath() + "/netpbm/bin:" + QCoreApplication::applicationDirPath() + "/python/bin:/usr/local/bin:" + path);
        env.insert("PYTHONPATH", QCoreApplication::applicationDirPath() + "/python/bin/site-packages");
    }
    else
    {
        env.insert("PATH", "/usr/local/bin:" + path);
    }
    solver->setProcessEnvironment(env);

    if (Options::alignmentLogging())
    {
        align->appendLogText("export PATH=" + env.value("PATH", ""));
        align->appendLogText("export PYTHONPATH=" + env.value("PYTHONPATH", ""));
    }

#endif

    connect(solver, SIGNAL(finished(int)), this, SLOT(solverComplete(int)));
    connect(solver, SIGNAL(readyReadStandardOutput()), this, SLOT(logSolver()));

#if QT_VERSION > QT_VERSION_CHECK(5, 6, 0)
    connect(solver.data(), &QProcess::errorOccurred, this, [&]()
    {
        align->appendLogText(i18n("Error starting solver: %1", solver->errorString()));
        emit solverFailed();
    });
#else
    connect(solver, SIGNAL(error(QProcess::ProcessError)), this, SIGNAL(solverFailed()));
#endif

    solverTimer.start();

    QString solverPath;

    if (Options::astrometrySolverIsInternal())
        solverPath = QCoreApplication::applicationDirPath() + "/astrometry/bin/solve-field";
    else
        solverPath = Options::astrometrySolverBinary();

    solver->start(solverPath, solverArgs);

    align->appendLogText(i18n("Starting solver..."));

    if (Options::alignmentLogging())
    {
        QString command = solverPath + ' ' + solverArgs.join(' ');

        align->appendLogText(command);
    }

    return true;
}

bool OfflineAstrometryParser::stopSolver()
{
    if (solver.isNull() == false)
    {
        solver->terminate();
        solver->disconnect();
    }

    return true;
}

void OfflineAstrometryParser::solverComplete(int exist_status)
{
    solver->disconnect();

    // TODO use QTemporaryFile later
    QString solutionFile = QDir::tempPath() + "/solution.wcs";
    QFileInfo solution(solutionFile);

    if (exist_status != 0 || solution.exists() == false)
    {
        align->appendLogText(i18n("Solver failed. Try again."));
        emit solverFailed();

        return;
    }

    connect(&wcsinfo, SIGNAL(finished(int)), this, SLOT(wcsinfoComplete(int)));

    QString wcsPath;

    if (Options::astrometryWCSIsInternal())
        wcsPath = QCoreApplication::applicationDirPath() + "/astrometry/bin/wcsinfo";
    else
        wcsPath = Options::astrometryWCSInfo();

    wcsinfo.start(wcsPath, QStringList(solutionFile));
}

void OfflineAstrometryParser::wcsinfoComplete(int exist_status)
{
    wcsinfo.disconnect();

    if (exist_status != 0)
    {
        align->appendLogText(i18n("WCS header missing or corrupted. Solver failed."));
        emit solverFailed();
        return;
    }

    QString wcsinfo_stdout = wcsinfo.readAllStandardOutput();

    QStringList wcskeys = wcsinfo_stdout.split(QRegExp("[\n]"));

    QStringList key_value;

    double ra = 0, dec = 0, orientation = 0, pixscale = 0;

    for (auto &key : wcskeys)
    {
        key_value = key.split(' ');

        if (key_value.size() > 1)
        {
            if (key_value[0] == "ra_center")
                ra = key_value[1].toDouble();
            else if (key_value[0] == "dec_center")
                dec = key_value[1].toDouble();
            else if (key_value[0] == "orientation_center")
                orientation = key_value[1].toDouble();
            else if (key_value[0] == "pixscale")
                pixscale = key_value[1].toDouble();
            else if (key_value[0] == "parity")
                parity = (key_value[1].toInt() == 0) ? "pos" : "neg";
        }
    }

    int elapsed = static_cast<int>(round(solverTimer.elapsed() / 1000.0));
    align->appendLogText(i18np("Solver completed in %1 second.", "Solver completed in %1 seconds.", elapsed));

    emit solverFinished(orientation, ra, dec, pixscale);
}

void OfflineAstrometryParser::logSolver()
{
    if (Options::alignmentLogging())
        align->appendLogText(solver->readAll().trimmed());
}

bool OfflineAstrometryParser::createLocalAstrometryConf()
{
    bool rc = false;

    QString confPath = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Literal("astrometry") + QLatin1Literal("/astrometry.cfg");
    QString systemConfPath = Options::astrometryConfFile();

    // Check if directory already exists, if it doesn't create one
    QDir writableDir(KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Literal("astrometry"));
    if (writableDir.exists() == false)
    {
        rc = writableDir.mkdir(KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Literal("astrometry"));

        if (rc == false)
        {
            qCCritical(KSTARS_EKOS_ALIGN) << "Failed to create local astrometry directory";
            return false;
        }
    }

    // Now copy system astrometry.cfg to local directory
    rc = QFile(systemConfPath).copy(confPath);

    if (rc == false)
    {
        qCCritical(KSTARS_EKOS_ALIGN) << "Failed to copy" << systemConfPath << "to" << confPath;
        return false;
    }

    QFile localConf(confPath);

    // Open file and add our own path to it
    if (localConf.open(QFile::ReadWrite))
    {
        QString all = localConf.readAll();
        QStringList lines = all.split("\n");
        for (int i = 0; i < lines.count(); i++)
        {
            if (lines[i].startsWith("add_path"))
            {
                lines.insert(i + 1, QString("add_path %1astrometry").arg(KSPaths::writableLocation(QStandardPaths::GenericDataLocation)));
                break;
            }
        }

        // Clear contents
        localConf.resize(0);

        // Now write back all the lines including our own inserted above
        QTextStream out(&localConf);
        for(const QString &line : lines)
            out << line << endl;
        localConf.close();
        return true;
    }

    qCCritical(KSTARS_EKOS_ALIGN) << "Failed to open local astrometry config" << confPath;
    return false;
}
}
