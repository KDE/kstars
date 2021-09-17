/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "offlineastrometryparser.h"

#include "align.h"
#include "ekos_align_debug.h"
#include "ksutils.h"
#include "Options.h"
#include "kspaths.h"
#include "ksnotification.h"

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

    QFileInfo solverFileInfo;

    if (Options::astrometrySolverIsInternal())
    {
        KSUtils::configureLocalAstrometryConfIfNecessary();
#if defined(Q_OS_LINUX)
        solverFileInfo = QFileInfo(Options::astrometrySolverBinary());
#elif defined(Q_OS_OSX)
        solverFileInfo = QFileInfo(QCoreApplication::applicationDirPath() + "/astrometry/bin/solve-field");
#endif
    }
    else
        solverFileInfo = QFileInfo(Options::astrometrySolverBinary());

    solverOK = solverFileInfo.exists() && solverFileInfo.isFile();

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
    QStringList astrometryDataDirs;
    bool indexesOK = true;

    astrometryDataDirs = Options::astrometryIndexFolderList();

    QStringList nameFilter("*.fits");
    QStringList indexList;
    for(QString astrometryDataDir : astrometryDataDirs)
    {
        QDir directory(astrometryDataDir);
        indexList << directory.entryList(nameFilter);
    }

    QString indexSearch   = indexList.join(" ");
    QString startIndex, lastIndex;
    unsigned int missingIndexes = 0;

    for (float skymarksize : astrometryIndex.keys())
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
                     "install the missing index files. Download the index files from http://astrometry.net",
                     startIndex));
        else
            align->appendLogText(i18n("Index files %1 to %2 are missing. Astrometry.net would not be able to "
                                      "adequately solve plates until you install the missing index files. Download the "
                                      "index files from http://astrometry.net",
                                      startIndex, lastIndex));
    }
}

bool OfflineAstrometryParser::startSolver(const QString &filename, const QStringList &args, bool generated)
{
    INDI_UNUSED(generated);

    QStringList solverArgs = args;

    // Use parity if it is: 1. Already known from previous solve. 2. This is NOT a blind solve
    if (Options::astrometryDetectParity() && (parity.isEmpty() == false) && (args.contains("parity") == false) &&
            (args.contains("-3") || args.contains("-L")))
        solverArgs << "--parity" << parity;

    QString confPath = KSUtils::getAstrometryConfFilePath();
    solverArgs << "--config" << confPath;

    QString solutionFile = QDir::tempPath() + "/solution.wcs";
    solverArgs << "-W" << solutionFile;

    solver.clear();
    solver = new QProcess(this);

    sextractorProcess.clear();
    sextractorProcess = new QProcess(this);

    QString sextractorBinaryPath=Options::sextractorBinary();

#ifdef Q_OS_OSX
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path            = env.value("PATH", "");
    QString pythonExecPath = "/usr/local/opt/python/libexec/bin";
    if(!Options::useDefaultPython())
        pythonExecPath = Options::pythonExecPath();
    if (Options::astrometrySolverIsInternal())
    {
        env.insert("PATH", QCoreApplication::applicationDirPath() + "/netpbm/bin:" + pythonExecPath + ":/usr/local/bin:" + path);
    }
    else
    {
        env.insert("PATH", pythonExecPath + ":/usr/local/bin:" + path);
    }
    if(Options::sextractorIsInternal())
        sextractorBinaryPath=QCoreApplication::applicationDirPath() + "/astrometry/bin/sex";
    solver->setProcessEnvironment(env);
    sextractorProcess->setProcessEnvironment(env);

    if (Options::alignmentLogging())
    {
        align->appendLogText("export PATH=" + env.value("PATH", ""));
    }

#endif

    fitsFile = filename;

    //These commands use sextractor to make a list of stars to feed into astrometry.net
    if(Options::useSextractor())
    {
        //Sets up the Temp file for the xy list of stars
        QString sextractorFilePath = QDir::tempPath() + "/SextractorList.xyls";
        QFile sextractorFile(sextractorFilePath);
        if(sextractorFile.exists())
            sextractorFile.remove();

        //Configuration arguments for sextractor
        QStringList sextractorArgs;
        sextractorArgs << "-CATALOG_NAME" << sextractorFilePath;
        sextractorArgs << "-CATALOG_TYPE" << "FITS_1.0";
        sextractorArgs << filename;

        //sextractor needs a default.param file in the working directory
        //This creates that file with the options we need for astrometry.net

        QString paramPath =  QDir::tempPath() + "/default.param";
        QFile paramFile(paramPath);
        if(!paramFile.exists())
        {
            if (paramFile.open(QIODevice::WriteOnly) == false)
                KSNotification::error(i18n("Sextractor file write error."));
            else
            {
                QTextStream out(&paramFile);
                out << "MAG_AUTO                 Kron-like elliptical aperture magnitude                   [mag]\n";
                out << "X_IMAGE                  Object position along x                                   [pixel]\n";
                out << "Y_IMAGE                  Object position along y                                   [pixel]\n";
                paramFile.close();
            }
        }

        //sextractor needs a default.conv file in the working directory
        //This creates the default one

        QString convPath =  QDir::tempPath() + "/default.conv";
        QFile convFile(convPath);
        if(!convFile.exists())
        {
            if (convFile.open(QIODevice::WriteOnly) == false)
                KSNotification::error(i18n("Sextractor file write error."));
            else
            {
                QTextStream out(&convFile);
                out << "CONV NORM\n";
                out << "1 2 1\n";
                out << "2 4 2\n";
                out << "1 2 1\n";
                convFile.close();
            }
        }
        sextractorProcess->setWorkingDirectory(QDir::tempPath());
        sextractorProcess->start(sextractorBinaryPath, sextractorArgs);
        align->appendLogText(i18n("Starting sextractor..."));
        align->appendLogText(sextractorBinaryPath + " " + sextractorArgs.join(' '));
        sextractorProcess->waitForFinished();
        if (Options::alignmentLogging())
            align->appendLogText(sextractorProcess->readAllStandardError().trimmed());
        solverArgs << sextractorFilePath;
    }
    else
    {
        solverArgs << fitsFile;
    }

    connect(solver, SIGNAL(finished(int)), this, SLOT(solverComplete(int)));
    solver->setProcessChannelMode(QProcess::MergedChannels);
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

        // 2019-04-25: When inparallel option is enabled in astrometry.cfg
        // astrometry-engine is not killed after solve-field is terminated
        QProcess p;
        p.start("killall astrometry-engine");
        p.waitForFinished();
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

    emit solverFinished(orientation, ra, dec, pixscale, parity != "pos");
}

void OfflineAstrometryParser::logSolver()
{
    if (Options::alignmentLogging())
        align->appendLogText(solver->readAll().trimmed());
}

}
