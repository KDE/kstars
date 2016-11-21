/*  Astrometry.net Parser
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <QDir>
#include <QFileInfo>

#include <KMessageBox>
#include <KLocalizedString>
#include "Options.h"

#include "auxiliary/kspaths.h"
#include "offlineastrometryparser.h"
#include "align.h"
#include "ksutils.h"

namespace Ekos
{

OfflineAstrometryParser::OfflineAstrometryParser() : AstrometryParser()
{
    astrometryIndex[2.8] = "index-4200";
    astrometryIndex[4.0] = "index-4201";
    astrometryIndex[5.6] = "index-4202";
    astrometryIndex[8] = "index-4203";
    astrometryIndex[11] = "index-4204";
    astrometryIndex[16] = "index-4205";
    astrometryIndex[22] = "index-4206";
    astrometryIndex[30] = "index-4207";
    astrometryIndex[42] = "index-4208";
    astrometryIndex[60] = "index-4209";
    astrometryIndex[85] = "index-4210";
    astrometryIndex[120] = "index-4211";
    astrometryIndex[170] = "index-4212";
    astrometryIndex[240] = "index-4213";
    astrometryIndex[340] = "index-4214";
    astrometryIndex[480] = "index-4215";
    astrometryIndex[680] = "index-4216";
    astrometryIndex[1000] = "index-4217";
    astrometryIndex[1400] = "index-4218";
    astrometryIndex[2000] = "index-4219";

    astrometryFilesOK = false;

}

OfflineAstrometryParser::~OfflineAstrometryParser()
{

}

bool OfflineAstrometryParser::init()
{

    #ifdef Q_OS_OSX
    if(Options::astrometryConfFileIsInternal())
        KSUtils::configureDefaultAstrometry();
    #endif

    if (astrometryFilesOK)
        return true;

    if (astrometryNetOK() == false)
    {
        if (align && align->isEnabled())
            KMessageBox::information(NULL, i18n("Failed to find astrometry.net binaries. Please ensure astrometry.net is installed and try again."),
                                 i18n("Missing astrometry files"), "missing_astrometry_binaries_warning");

        return false;
    }

    astrometryFilesOK = true;
    return true;
}

bool OfflineAstrometryParser::astrometryNetOK()
{
    bool solverOK=false, wcsinfoOK=false;

    if(Options::astrometrySolverIsInternal()){
        QFileInfo solver(QCoreApplication::applicationDirPath()+"/astrometry/bin/solve-field");
        solverOK = solver.exists() && solver.isFile();
    }
    else{
        QFileInfo solver(Options::astrometrySolver());
        solverOK = solver.exists() && solver.isFile();
    }

    if(Options::wcsIsInternal()){
        QFileInfo wcsinfo(QCoreApplication::applicationDirPath()+"/astrometry/bin/wcsinfo");
        wcsinfoOK  = wcsinfo.exists() && wcsinfo.isFile();
    }else{
        QFileInfo wcsinfo(Options::astrometryWCSInfo());
        wcsinfoOK  = wcsinfo.exists() && wcsinfo.isFile();
    }

    return (solverOK && wcsinfoOK);
}

void OfflineAstrometryParser::verifyIndexFiles(double fov_x, double fov_y)
{
    static double last_fov_x=0, last_fov_y=0;

    if (last_fov_x == fov_x && last_fov_y == fov_y)
        return;

    last_fov_x = fov_x;
    last_fov_y = fov_y;
    double fov_lower = 0.10 * fov_x;
    double fov_upper = fov_x;
    QStringList indexFiles;
    QString astrometryDataDir;
    bool indexesOK = true;

    if (getAstrometryDataDir(astrometryDataDir) == false)
        return;

    QStringList nameFilter("*.fits");
    QDir directory(astrometryDataDir);
    QStringList indexList = directory.entryList(nameFilter);
    QString indexSearch = indexList.join(" ");
    QString startIndex, lastIndex;
    unsigned int missingIndexes=0;

    foreach(float skymarksize, astrometryIndex.keys())
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
            align->appendLogText(i18n("Index file %1 is missing. Astrometry.net would not be able to adequately solve plates until you install the missing index files. Download the index files from http://www.astrometry.net",
                                             startIndex));
        else
            align->appendLogText(i18n("Index files %1 to %2 are missing. Astrometry.net would not be able to adequately solve plates until you install the missing index files. Download the index files from http://www.astrometry.net",
                                             startIndex, lastIndex));

    }
}

bool OfflineAstrometryParser::getAstrometryDataDir(QString &dataDir)
{
    QString confPath;

    if(Options::astrometryConfFileIsInternal())
        confPath = QCoreApplication::applicationDirPath()+"/astrometry/bin/astrometry.cfg";
    else
        confPath = Options::astrometryConfFile();

    QFile confFile(confPath);

    if (confFile.open(QIODevice::ReadOnly) == false)
    {
        KMessageBox::error(0, i18n("Astrometry configuration file corrupted or missing: %1\nPlease set the configuration file full path in INDI options.", Options::astrometryConfFile()));
        return false;
    }

    QTextStream in(&confFile);
    QString line;
    while ( !in.atEnd() )
    {
      line = in.readLine();
      if (line.isEmpty() || line.startsWith("#"))
          continue;

      line = line.trimmed();
      if (line.startsWith("add_path"))
      {
          dataDir = line.mid(9).trimmed();
          return true;
      }
   }

    KMessageBox::error(0, i18n("Unable to find data dir in astrometry configuration file."));
    return false;
}

bool OfflineAstrometryParser::startSovler(const QString &filename,  const QStringList &args, bool generated)
{
    INDI_UNUSED(generated);

    #ifdef Q_OS_OSX
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList envlist = env.toStringList();
    envlist.replaceInStrings(QRegularExpression("^(?i)PATH=(.*)"), "PATH=/usr/local/bin:\\1");
    solver.setEnvironment(envlist);
    #endif

    QStringList solverArgs = args;
    // Add parity option if none is give and we already know parity before
    if (parity.isEmpty() == false && args.contains("parity") == false)
        solverArgs << "--parity" << parity;
    QString solutionFile = QDir::tempPath() + "/solution.wcs";
    solverArgs << "-W" <<  solutionFile << filename;

    fitsFile = filename;

    connect(&solver, SIGNAL(finished(int)), this, SLOT(solverComplete(int)));
    connect(&solver, SIGNAL(readyReadStandardOutput()), this, SLOT(logSolver()));

    // Reset parity on solver failure
    connect(this, &OfflineAstrometryParser::solverFailed, this, [&]() { parity = QString();});

#if QT_VERSION > QT_VERSION_CHECK(5,6,0)
    connect(&solver, &QProcess::errorOccurred, this, [&]()
    {
        align->appendLogText(i18n("Error starting solver: %1", solver.errorString()));
        emit solverFailed();
    });
#else
    connect(&solver, SIGNAL(error(QProcess::ProcessError)), this, SIGNAL(solverFailed()));
#endif

    solverTimer.start();

    QString solverPath;

    if(Options::astrometrySolverIsInternal())
        solverPath=QCoreApplication::applicationDirPath()+"/astrometry/bin/solve-field";
    else
        solverPath=Options::astrometrySolver();

    solver.start(solverPath, solverArgs);

    align->appendLogText(i18n("Starting solver..."));

    if (Options::solverVerbose())
    {
        QString command = solverPath + " " + solverArgs.join(" ");
        align->appendLogText(command);
    }

    return true;
}

bool OfflineAstrometryParser::stopSolver()
{
    solver.terminate();
    solver.disconnect();

    return true;
}

void OfflineAstrometryParser::solverComplete(int exist_status)
{
    solver.disconnect();

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

    if(Options::wcsIsInternal())
        wcsPath = QCoreApplication::applicationDirPath()+"/astrometry/bin/wcsinfo";
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

    double ra=0, dec=0, orientation=0, pixscale=0;    

    foreach(QString key, wcskeys)
    {
        key_value = key.split(" ");

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

    int elapsed = (int) round(solverTimer.elapsed()/1000.0);
    align->appendLogText(i18np("Solver completed in %1 second.", "Solver completed in %1 seconds.", elapsed));

    emit solverFinished(orientation,ra,dec, pixscale);

    // Remove files left over by the solver
    //QDir dir("/tmp");
    QDir dir(QDir::tempPath());
    dir.setNameFilters(QStringList() << "fits*" << "tmp.*");
    dir.setFilter(QDir::Files);
    foreach(QString dirFile, dir.entryList())
            dir.remove(dirFile);

}

void OfflineAstrometryParser::logSolver()
{
    if (Options::solverVerbose())
        align->appendLogText(solver.readAll().trimmed());
}

}



