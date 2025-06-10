/*
    SPDX-FileCopyrightText: 2024 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "imagingplanner.h"

#include <kio_version.h>
#include <KLocalizedString>

#include "artificialhorizoncomponent.h"
#include "auxiliary/screencapture.h"
#include "auxiliary/thememanager.h"
#include "catalogscomponent.h"
#include "constellationboundarylines.h"
#include "dialogs/detaildialog.h"
#include "dialogs/finddialog.h"
// TODO: replace this. See comment above SchedulerUtils_setupJob().
//#include "ekos/scheduler/schedulerutils.h"
#include "ekos/scheduler/schedulerjob.h"

// These are just for the debugging method checkTargets()
#include "flagmanager.h"
#include "flagcomponent.h"

#include "nameresolver.h"
#include "imageoverlaycomponent.h"
#include "imagingplanneroptions.h"
#include "kplotwidget.h"
#include "kplotobject.h"
#include "kplotaxis.h"
#include "ksalmanac.h"
#include "ksmoon.h"
#include "ksnotification.h"
#include <kspaths.h>
#include "kstars.h"
#include "ksuserdb.h"
#include "kstarsdata.h"
#include "fitsviewer/platesolve.h"
#include "skymap.h"
#include "skymapcomposite.h"

#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QImage>
#include <QNetworkReply>
#include <QPixmap>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStringList>
#include <QWidget>
#include "zlib.h"

#define DPRINTF if (false) fprintf

namespace
{
// Data columns in the model.
// Must agree with the header QStringList COLUMN_HEADERS below
// and the if/else-if test values in addCatalogItem().
enum ColumnEnum
{
    NAME_COLUMN = 0,
    HOURS_COLUMN,
    TYPE_COLUMN,
    SIZE_COLUMN,
    ALTITUDE_COLUMN,
    MOON_COLUMN,
    CONSTELLATION_COLUMN,
    COORD_COLUMN,
    FLAGS_COLUMN,
    NOTES_COLUMN,
    LAST_COLUMN
};

// The columns at the end of enum ColumnEnum above are not displayed
// in the table, and thus have no header name.
const QList<KLocalizedString> COLUMN_HEADERS =
{
    ki18n("Name"), ki18n("Hours"), ki18n("Type"), ki18n("Size"),
    ki18n("Alt"), ki18n("Moon"), ki18n("Const"), ki18n("Coord")
};
}

// These could probably all be Qt::UserRole + 1
#define TYPE_ROLE (Qt::UserRole + 1)
#define HOURS_ROLE (Qt::UserRole + 2)
#define SIZE_ROLE (Qt::UserRole + 3)
#define ALTITUDE_ROLE (Qt::UserRole + 4)
#define MOON_ROLE (Qt::UserRole + 5)
#define FLAGS_ROLE (Qt::UserRole + 6)
#define NOTES_ROLE (Qt::UserRole + 7)

#define PICKED_BIT ImagingPlannerDBEntry::PickedBit
#define IMAGED_BIT ImagingPlannerDBEntry::ImagedBit
#define IGNORED_BIT ImagingPlannerDBEntry::IgnoredBit

/**********************************************************
TODO/Ideas:
- Filter by size
- Log at bottom
- Imaging time constraint in hours calc
- Move some or all of the filtering to menus in the column headers
- Just use UserRole or UserRole+1 for the non-display roles.
- Altitude graph has some replicated code with the scheduler
- Weird timezone stuff when setting kstars to a timezone that's not the system's timezone.
- Add a catalog name, and display it
***********************************************************/

namespace
{

QString capitalize(const QString &str)
{
    QString temp = str.toLower();
    temp[0] = str[0].toUpper();
    return temp;
}

// Checks the appropriate Options variable to see if the object-type
// should be displayed.
bool acceptType(SkyObject::TYPE type)
{
    switch (type)
    {
        case SkyObject::OPEN_CLUSTER:
            return Options::imagingPlannerAcceptOpenCluster();
        case SkyObject::GLOBULAR_CLUSTER:
            return Options::imagingPlannerAcceptGlobularCluster();
        case SkyObject::GASEOUS_NEBULA:
            return Options::imagingPlannerAcceptNebula();
        case SkyObject::PLANETARY_NEBULA:
            return Options::imagingPlannerAcceptPlanetary();
        case SkyObject::SUPERNOVA_REMNANT:
            return Options::imagingPlannerAcceptSupernovaRemnant();
        case SkyObject::GALAXY:
            return Options::imagingPlannerAcceptGalaxy();
        case SkyObject::GALAXY_CLUSTER:
            return Options::imagingPlannerAcceptGalaxyCluster();
        case SkyObject::DARK_NEBULA:
            return Options::imagingPlannerAcceptDarkNebula();
        default:
            return Options::imagingPlannerAcceptOther();
    }
}

bool getFlag(const QModelIndex &index, int bit,  QAbstractItemModel *model)
{
    auto idx = index.siblingAtColumn(FLAGS_COLUMN);
    const bool hasFlags = model->data(idx, FLAGS_ROLE).canConvert<int>();
    if (!hasFlags)
        return false;
    const bool flag = model->data(idx, FLAGS_ROLE).toInt() & bit;
    return flag;
}

void setFlag(const QModelIndex &index, int bit, QAbstractItemModel *model)
{
    auto idx = index.siblingAtColumn(FLAGS_COLUMN);
    const bool hasFlags = model->data(idx, FLAGS_ROLE).canConvert<int>();
    int currentFlags = 0;
    if (hasFlags)
        currentFlags = model->data(idx, FLAGS_ROLE).toInt();
    QVariant val(currentFlags | bit);
    model->setData(idx, val, FLAGS_ROLE);
}

void clearFlag(const QModelIndex &index, int bit, QAbstractItemModel *model)
{
    auto idx = index.siblingAtColumn(FLAGS_COLUMN);
    const bool hasFlags = model->data(idx, FLAGS_ROLE).canConvert<int>();
    if (!hasFlags)
        return;
    const int currentFlags = model->data(idx, FLAGS_ROLE).toInt();
    QVariant val(currentFlags & ~bit);
    model->setData(idx, val, FLAGS_ROLE);
}

QString flagString(int flags)
{
    QString str;
    if (flags & IMAGED_BIT) str.append(i18n("Imaged"));
    if (flags & PICKED_BIT)
    {
        if (str.size() != 0)
            str.append(", ");
        str.append(i18n("Picked"));
    }
    if (flags & IGNORED_BIT)
    {
        if (str.size() != 0)
            str.append(", ");
        str.append(i18n("Ignored"));
    }
    return str;
}

// The next 3 methods condense repeated code needed for the filtering checkboxes.
void setupShowCallback(bool checked,
                       void (*showOption)(bool), void (*showNotOption)(bool),
                       void (*dontCareOption)(bool),
                       QCheckBox *showCheckbox, QCheckBox *showNotCheckbox,
                       QCheckBox *dontCareCheckbox)
{
    Q_UNUSED(showCheckbox);
    if (checked)
    {
        showOption(true);
        showNotOption(false);
        dontCareOption(false);
        showNotCheckbox->setChecked(false);
        dontCareCheckbox->setChecked(false);
        Options::self()->save();
    }
    else
    {
        showOption(false);
        showNotOption(false);
        dontCareOption(true);
        showNotCheckbox->setChecked(false);
        dontCareCheckbox->setChecked(true);
        Options::self()->save();
    }
}

void setupShowNotCallback(bool checked,
                          void (*showOption)(bool), void (*showNotOption)(bool), void (*dontCareOption)(bool),
                          QCheckBox *showCheckbox, QCheckBox *showNotCheckbox, QCheckBox *dontCareCheckbox)
{
    Q_UNUSED(showNotCheckbox);
    if (checked)
    {
        showOption(false);
        showNotOption(true);
        dontCareOption(false);
        showCheckbox->setChecked(false);
        dontCareCheckbox->setChecked(false);
        Options::self()->save();
    }
    else
    {
        showOption(false);
        showNotOption(false);
        dontCareOption(true);
        showCheckbox->setChecked(false);
        dontCareCheckbox->setChecked(true);
        Options::self()->save();
    }
}

void setupDontCareCallback(bool checked,
                           void (*showOption)(bool), void (*showNotOption)(bool), void (*dontCareOption)(bool),
                           QCheckBox *showCheckbox, QCheckBox *showNotCheckbox, QCheckBox *dontCareCheckbox)
{
    if (checked)
    {
        showOption(false);
        showNotOption(false);
        dontCareOption(true);
        showCheckbox->setChecked(false);
        showNotCheckbox->setChecked(false);
        Options::self()->save();
    }
    else
    {
        // Yes, the user just set this to false, but
        // there's no obvious way to tell what the user wants.
        showOption(false);
        showNotOption(false);
        dontCareOption(true);
        showCheckbox->setChecked(false);
        showNotCheckbox->setChecked(false);
        dontCareCheckbox->setChecked(true);
        Options::self()->save();
    }
}

// Find the nth url inside a QString. Returns an empty QString if none is found.
QString findUrl(const QString &input, int nth = 1)
{
    // Got the RE by asking Google's AI!
    QRegularExpression re("https?:\\/\\/(www\\.)?[-a-zA-Z0-9@:%._"
                          "\\+~#=]{1,256}\\.[a-zA-Z0-9()]{1,6}\\b([-a-zA-Z0-9()@:%_\\+.~#?&//=]*)");

    re.setPatternOptions(QRegularExpression::MultilineOption |
                         QRegularExpression::DotMatchesEverythingOption |
                         QRegularExpression::CaseInsensitiveOption);
    auto match = re.match(input);
    if (!match.hasMatch())
        return QString();
    else if (nth == 1)
        return(match.captured(0));

    QString inp = input;
    while (--nth >= 1)
    {
        inp = inp.mid(match.capturedEnd());
        match = re.match(inp);
        if (!match.hasMatch())
            return QString();
        else if (nth == 1)
            return (match.captured(0));
    }
    return QString();
}

// Make some guesses about possible input-name confusions.
// Used in the table's search box and when reading the file of already-imaged objects.
QString tweakNames(const QString &input)
{
    QString fixed = input;
    if (fixed.startsWith("sharpless", Qt::CaseInsensitive))
        fixed.replace(QRegularExpression("^sharpless-?2", QRegularExpression::CaseInsensitiveOption), "sh2");
    if (fixed.startsWith("messier", Qt::CaseInsensitive))
        fixed.replace(QRegularExpression("^messier", QRegularExpression::CaseInsensitiveOption), "m");

    fixed.replace(QRegularExpression("^(ngc|ic|abell|ldn|lbn|m|sh2|vdb)\\s*(\\d)",
                                     QRegularExpression::CaseInsensitiveOption), "\\1 \\2");
    if (fixed.startsWith("sh2-", Qt::CaseInsensitive))
        fixed.replace(QRegularExpression("^sh2-\\s*(\\d)", QRegularExpression::CaseInsensitiveOption), "sh2 \\1");
    return fixed;
}

// Return true if left side is less than right side (values are floats)
// As opposed to the below, we want the non-reversed sort to be 9 -> 0.
// This is used when sorting the table by floating point columns.
double floatCompareFcn( const QModelIndex &left, const QModelIndex &right,
                        int column, int role)
{
    const double l = left.siblingAtColumn(column).data(role).toDouble();
    const double r = right.siblingAtColumn(column).data(role).toDouble();
    return l - r;
}

// Return true if left side is less than right side
// Values can be simple strings or object names like "M 31" where the 2nd part is sorted arithmatically.
// We want the non-reversed sort to be A -> Z and 0 -> 9, which is why all the returns have minus signs.
// This is used when sorting the table by string columns.
int stringCompareFcn( const QModelIndex &left, const QModelIndex &right, int column, int role)
{
    const QString l = left.siblingAtColumn(column).data(role).toString();
    const QString r = right.siblingAtColumn(column).data(role).toString();
    const QStringList lList = l.split(' ', Qt::SkipEmptyParts, Qt::CaseInsensitive);
    const QStringList rList = r.split(' ', Qt::SkipEmptyParts, Qt::CaseInsensitive);

    if (lList.size() == 0 || rList.size() == 0)
        return - QString::compare(l, r, Qt::CaseInsensitive);

    // Both sides have at least one item. If the first item is not the same,
    // return the string compare value for those.
    const int comp = QString::compare(lList[0], rList[0], Qt::CaseInsensitive);
    if (comp != 0)
        return -comp;

    // Here we deal with standard object names, like comparing "M 100" and "M 33"
    // I'm assuming here that our object names have spaces, as is standard in kstars.
    if (lList.size() >= 2 && rList.size() >= 2)
    {
        int lInt = lList[1].toInt();
        int rInt = rList[1].toInt();
        // If they're not ints, then toInt returns 0.
        // Not expecting negative numbers here.
        if (lInt > 0 && rInt > 0)
            return -(lInt - rInt);
    }
    // Go back to the original string compare
    return -QString::compare(l, r, Qt::CaseInsensitive);
}

// TODO: This is copied from schedulerutils.h/cpp because for some reason the build failed when
// including
void SchedulerUtils_setupJob(Ekos::SchedulerJob &job, const QString &name, bool isLead, const QString &group,
                             const QString &train, const dms &ra, const dms &dec, double djd, double rotation, const QUrl &sequenceUrl,
                             const QUrl &fitsUrl, Ekos::StartupCondition startup, const QDateTime &startupTime, Ekos::CompletionCondition completion,
                             const QDateTime &completionTime, int completionRepeats, double minimumAltitude, double minimumMoonSeparation,
                             double maxMoonAltitude,
                             bool enforceTwilight, bool enforceArtificialHorizon, bool track, bool focus, bool align, bool guide)
{
    /* Configure or reconfigure the observation job */

    job.setIsLead(isLead);
    job.setOpticalTrain(train);
    job.setPositionAngle(rotation);

    if (isLead)
    {
        job.setName(name);
        job.setGroup(group);
        job.setLeadJob(nullptr);
        // djd should be ut.djd
        job.setTargetCoords(ra, dec, djd);
        job.setFITSFile(fitsUrl);

        // #1 Startup conditions
        job.setStartupCondition(startup);
        if (startup == Ekos::START_AT)
        {
            job.setStartupTime(startupTime);
        }
        /* Store the original startup condition */
        job.setFileStartupCondition(job.getStartupCondition());
        job.setStartAtTime(job.getStartupTime());

        // #2 Constraints
        job.setMinAltitude(minimumAltitude);
        job.setMinMoonSeparation(minimumMoonSeparation);
        job.setMaxMoonAltitude(maxMoonAltitude);

        // Check enforce weather constraints
        // twilight constraints
        job.setEnforceTwilight(enforceTwilight);
        job.setEnforceArtificialHorizon(enforceArtificialHorizon);

        // Job steps
        job.setStepPipeline(Ekos::SchedulerJob::USE_NONE);
        if (track)
            job.setStepPipeline(static_cast<Ekos::SchedulerJob::StepPipeline>(job.getStepPipeline() | Ekos::SchedulerJob::USE_TRACK));
        if (focus)
            job.setStepPipeline(static_cast<Ekos::SchedulerJob::StepPipeline>(job.getStepPipeline() | Ekos::SchedulerJob::USE_FOCUS));
        if (align)
            job.setStepPipeline(static_cast<Ekos::SchedulerJob::StepPipeline>(job.getStepPipeline() | Ekos::SchedulerJob::USE_ALIGN));
        if (guide)
            job.setStepPipeline(static_cast<Ekos::SchedulerJob::StepPipeline>(job.getStepPipeline() | Ekos::SchedulerJob::USE_GUIDE));

        /* Store the original startup condition */
        job.setFileStartupCondition(job.getStartupCondition());
        job.setStartAtTime(job.getStartupTime());
    }

    /* Consider sequence file is new, and clear captured frames map */
    job.setCapturedFramesMap(Ekos::CapturedFramesMap());
    job.setSequenceFile(sequenceUrl);
    job.setCompletionCondition(completion);
    if (completion == Ekos::FINISH_AT)
        job.setFinishAtTime(completionTime);
    else if (completion == Ekos::FINISH_REPEAT)
    {
        job.setRepeatsRequired(completionRepeats);
        job.setRepeatsRemaining(completionRepeats);
    }

    /* Reset job state to evaluate the changes */
    job.reset();
}

// Sets up a SchedulerJob, used by getRunTimes to see when the target can be imaged.
void setupJob(Ekos::SchedulerJob &job, const QString name, double minAltitude, double minMoonSeparation,
              double maxMoonAltitude, dms ra, dms dec,
              bool useArtificialHorizon)
{
    double djd = KStars::Instance()->data()->ut().djd();
    double rotation = 0.0;
    QString train = "";
    QUrl sequenceURL; // is this needed?

    // TODO: Hopefully go back to calling SchedulerUtils::setupJob()
    //Ekos::SchedulerUtils::setupJob(job, name, true, "",
    SchedulerUtils_setupJob(job, name, true, "",
                            train, ra, dec, djd,
                            rotation, sequenceURL, QUrl(),
                            Ekos::START_ASAP, QDateTime(),
                            Ekos::FINISH_LOOP, QDateTime(), 1,
                            minAltitude, minMoonSeparation, maxMoonAltitude,
                            true, useArtificialHorizon,
                            true, true, true, true);
}

// Computes the times when the given coordinates can be imaged on the date.
void getRunTimes(const QDate &date, const GeoLocation &geo, double minAltitude, double minMoonSeparation,
                 double maxMoonAltitude, const dms &ra, const dms &dec, bool useArtificialHorizon, QVector<QDateTime> *jobStartTimes,
                 QVector<QDateTime> *jobEndTimes)
{
    jobStartTimes->clear();
    jobEndTimes->clear();
    constexpr int SCHEDULE_RESOLUTION_MINUTES = 10;
    Ekos::SchedulerJob job;
    setupJob(job, "temp", minAltitude, minMoonSeparation, maxMoonAltitude, ra, dec, useArtificialHorizon);

    auto tz = QTimeZone(geo.TZ() * 3600);

    // Find possible imaging times between noon and the next noon.
    QDateTime  startTime(QDateTime(date,            QTime(12, 0, 1)));
    QDateTime  stopTime( QDateTime(date.addDays(1), QTime(12, 0, 1)));
    startTime.setTimeZone(tz);
    stopTime.setTimeZone(tz);

    QString constraintReason;
    int maxIters = 10;
    while (--maxIters >= 0)
    {
        QDateTime s = job.getNextPossibleStartTime(startTime, SCHEDULE_RESOLUTION_MINUTES, false, stopTime);
        if (!s.isValid())
            return;
        s.setTimeZone(tz);

        QDateTime e = job.getNextEndTime(s, SCHEDULE_RESOLUTION_MINUTES, &constraintReason, stopTime);
        if (!e.isValid())
            return;
        e.setTimeZone(tz);

        jobStartTimes->push_back(s);
        jobEndTimes->push_back(e);

        if (e.secsTo(stopTime) < 600)
            return;

        startTime = e.addSecs(60);
        startTime.setTimeZone(tz);
    }
}

// Computes the times when the given catalog object can be imaged on the date.
double getRunHours(const CatalogObject &object, const QDate &date, const GeoLocation &geo, double minAltitude,
                   double minMoonSeparation, double maxMoonAltitude, bool useArtificialHorizon)
{
    QVector<QDateTime> jobStartTimes, jobEndTimes;
    getRunTimes(date, geo, minAltitude, minMoonSeparation, maxMoonAltitude, object.ra0(), object.dec0(), useArtificialHorizon,
                &jobStartTimes,
                &jobEndTimes);
    if (jobStartTimes.size() == 0 || jobEndTimes.size() == 0)
        return 0;
    else
    {
        double totalHours = 0.0;
        for (int i = 0; i < jobStartTimes.size(); ++i)
            totalHours += jobStartTimes[i].secsTo(jobEndTimes[i]) * 1.0 / 3600.0;
        return totalHours;
    }
}

// Pack is needed to generate the Astrobin search URLs.
// This implementation was inspired by
// https://github.com/romixlab/qmsgpack/blob/master/src/private/pack_p.cpp
// Returns the size it would have, or actually did, pack.
int packString(const QString &input, quint8 *p, bool reallyPack)
{
    QByteArray str_data = input.toUtf8();
    quint32 len = str_data.length();
    const char *str = str_data.data();
    constexpr bool compatibilityMode = false;
    const quint8 *origP = p;
    if (len <= 31)
    {
        if (reallyPack) *p = 0xa0 | len;
        p++;
    }
    else if (len <= std::numeric_limits<quint8>::max() &&
             compatibilityMode == false)
    {
        if (reallyPack) *p = 0xd9;
        p++;
        if (reallyPack) *p = len;
        p++;
    }
    else if (len <= std::numeric_limits<quint16>::max())
    {
        if (reallyPack) *p = 0xda;
        p++;
        if (reallyPack)
        {
            quint16 val = len;
            memcpy(p, &val, 2);
        }
        p += 2;
    }
    else return 0; // Bailing if the url is longer than 64K--shouldn't happen.

    if (reallyPack) memcpy(p, str, len);
    return (p - origP) + len;
}

QByteArray pack(const QString &input)
{
    QVector<QByteArray> user_data;
    // first run, calculate size
    int size = packString(input, nullptr, false);
    QByteArray arr;
    arr.resize(size);
    // second run, pack it
    packString(input, reinterpret_cast<quint8*>(arr.data()), true);
    return arr;
}


// Turn the first space to a dash and remove the rest of the spaces
QString replaceSpaceWith(const QString &name, const QString &replacement)
{
    QString result = name;

    // Replace the first space with a dash
    QRegularExpression firstSpaceRegex(QStringLiteral(" "));
    QRegularExpressionMatch firstMatch = firstSpaceRegex.match(result);
    if (firstMatch.hasMatch())
    {
        result.replace(firstMatch.capturedStart(), 1, replacement);
        // Remove all remaining spaces
        QRegularExpression remainingSpacesRegex(QStringLiteral(" "));
        result.replace(remainingSpacesRegex, QStringLiteral(""));
    }
    return result;
}

// This function is just used in catalog development. Output to stderr.
bool downsampleImageFiles(const QString &baseDir, int maxHeight)
{
    QString fn = "Test.txt";
    QFile file( fn );
    if ( file.open(QIODevice::ReadWrite) )
    {
        QTextStream stream( &file );
        stream << "hello" << Qt::endl;
    }
    file.close();

    const QString subDir = "REDUCED";
    QDir directory(baseDir);
    if (!directory.exists())
    {
        fprintf(stderr, "downsampleImageFiles: Base directory doesn't exist\n");
        return false;
    }
    QDir outDir = QDir(directory.absolutePath().append(QDir::separator()).append(subDir));
    if (!outDir.exists())
    {
        if (!outDir.mkpath("."))
        {
            fprintf(stderr, "downsampleImageFiles: Failed making the output directory\n");
            return false;
        }
    }

    int numSaved = 0;
    QStringList files = directory.entryList(QStringList() << "*.jpg" << "*.JPG" << "*.png", QDir::Files);
    foreach (QString filename, files)
    {
        QString fullPath = QString("%1%2%3").arg(baseDir).arg(QDir::separator()).arg(filename);
        QImage img(fullPath);
        QImage scaledImg;
        if (img.height() > maxHeight)
            scaledImg = img.scaledToHeight(maxHeight, Qt::SmoothTransformation);
        else
            scaledImg = img;

        QString writeFilename = outDir.absolutePath().append(QDir::separator()).append(filename);
        QFileInfo info(writeFilename);
        QString jpgFilename = info.path() + QDir::separator() + info.completeBaseName() + ".jpg";

        if (!scaledImg.save(jpgFilename, "JPG"))
            fprintf(stderr, "downsampleImageFiles: Failed saving \"%s\"\n", writeFilename.toLatin1().data());
        else
        {
            numSaved++;
            fprintf(stderr, "downsampleImageFiles: saved \"%s\"\n", writeFilename.toLatin1().data());
        }
    }
    fprintf(stderr, "downsampleImageFiles: Wrote %d files\n", numSaved);
    return true;
}

// Seaches for all the occurances of the byte cc in the QByteArray, and replaces each
// of them with the sequence of bytes in the QByteArray substitute.
// There's probably a QByteArray method that does this.
void replaceByteArrayChars(QByteArray &bInput, char cc, const QByteArray &substitute)
{
    while (true)
    {
        const int len = bInput.size();
        int pos = -1;
        for (int i = 0; i < len; ++i)
        {
            if (bInput[i] == cc)
            {
                pos = i;
                break;
            }
        }
        if (pos < 0)
            break;
        bInput.replace(pos, 1, substitute);
    }
}

// Look in the app directories in case a .png or .jpg file exists that ends
// with the object name.
QString findObjectImage(const QString &name)
{
    // Remove any spaces, but "sh2 " becomes "sh2-"
    auto massagedName = name;
    if (massagedName.startsWith("sh2 ", Qt::CaseInsensitive))
        massagedName = massagedName.replace(0, 4, "sh2-");
    massagedName =  massagedName.replace(' ', "");

    QDir dir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    QStringList nameFilter;
    nameFilter << QString("*%1.png").arg(massagedName) << QString("*%1.jpg").arg(massagedName);
    QFileInfoList files = dir.entryInfoList(nameFilter, QDir::Files);
    if (files.size() > 0)
        return files[0].absoluteFilePath();

    QFileInfoList subDirs = dir.entryInfoList(nameFilter, QDir::NoDotAndDotDot | QDir::AllDirs);
    for (int i = 0; i < subDirs.size(); i++)
    {
        QDir subDir(subDirs[i].absoluteFilePath());
        QFileInfoList files = subDir.entryInfoList(nameFilter, QDir::NoDotAndDotDot | QDir::Files);
        if (files.size() > 0)
            return files[0].absoluteFilePath();
    }
    return QString();
}

QString creativeCommonsString(const QString &astrobinAbbrev)
{
    if (astrobinAbbrev == "ACC")
        return "CC-BY";
    else if (astrobinAbbrev == "ASACC")
        return "CC-BY-SA";
    else if (astrobinAbbrev == "ANCCC")
        return "CC-BY-NC";
    else if (astrobinAbbrev == "ANCSACC")
        return "CC-BY-SA-NC";
    else return "";
}

QString creativeCommonsTooltipString(const QString &astrobinAbbrev)
{
    if (astrobinAbbrev == "ACC")
        return "Atribution Creative Commons";
    else if (astrobinAbbrev == "ASACC")
        return "Atribution Share-Alike Creative Commons";
    else if (astrobinAbbrev == "ANCCC")
        return "Atribution Non-Commercial Creative Commons";
    else if (astrobinAbbrev == "ANCSACC")
        return "Atribution Non-Commercial Share-Alike Creative Commons";
    else return "";
}

QString shortCoordString(const dms &ra, const dms &dec)
{
    return QString("%1h%2' %3%4°%5'").arg(ra.hour()).arg(ra.minute())
           .arg(dec.Degrees() < 0 ? "-" : "").arg(abs(dec.degree())).arg(abs(dec.arcmin()));
}

double getAltitude(GeoLocation *geo, SkyPoint &p, const QDateTime &time)
{
    auto ut2 = geo->LTtoUT(KStarsDateTime(time));
    CachingDms LST = geo->GSTtoLST(ut2.gst());
    p.EquatorialToHorizontal(&LST, geo->lat());
    return p.alt().Degrees();
}

double getMaxAltitude(const KSAlmanac &ksal, const QDate &date, GeoLocation *geo, const SkyObject &object,
                      double hoursAfterDusk = 0, double hoursBeforeDawn = 0)
{
    auto tz = QTimeZone(geo->TZ() * 3600);
    KStarsDateTime midnight = KStarsDateTime(date.addDays(1), QTime(0, 1));
    midnight.setTimeZone(tz);

    QDateTime dawn = midnight.addSecs(24 * 3600 * ksal.getDawnAstronomicalTwilight());
    dawn.setTimeZone(tz);
    QDateTime dusk = midnight.addSecs(24 * 3600 * ksal.getDuskAstronomicalTwilight());
    dusk.setTimeZone(tz);

    QDateTime start = dusk.addSecs(hoursAfterDusk * 3600);
    start.setTimeZone(tz);

    auto end = dawn.addSecs(-hoursBeforeDawn * 3600);
    end.setTimeZone(tz);

    SkyPoint coords = object;
    double maxAlt = -90;
    auto t = start;
    t.setTimeZone(tz);
    QDateTime maxTime = t;

    while (t.secsTo(end) > 0)
    {
        double alt = getAltitude(geo, coords, t);
        if (alt > maxAlt)
        {
            maxAlt = alt;
            maxTime = t;
        }
        t = t.addSecs(60 * 20);
    }
    return maxAlt;
}

}  // namespace

CatalogFilter::CatalogFilter(QObject* parent) : QSortFilterProxyModel(parent)
{
    m_SortColumn = HOURS_COLUMN;
}

// This method decides whether a row is shown in the object table.
bool CatalogFilter::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    const QModelIndex typeIndex = sourceModel()->index(row, TYPE_COLUMN, parent);
    const SkyObject::TYPE type = static_cast<SkyObject::TYPE>(sourceModel()->data(typeIndex, TYPE_ROLE).toInt());
    if (!acceptType(type)) return false;

    const QModelIndex hoursIndex = sourceModel()->index(row, HOURS_COLUMN, parent);
    const bool hasEnoughHours = sourceModel()->data(hoursIndex, Qt::DisplayRole).toDouble() >= m_MinHours;
    if (!hasEnoughHours) return false;

    const QModelIndex flagsIndex = sourceModel()->index(row, FLAGS_COLUMN, parent);

    const bool isImaged = sourceModel()->data(flagsIndex, FLAGS_ROLE).toInt() & IMAGED_BIT;
    const bool passesImagedConstraints = !m_ImagedConstraintsEnabled || (isImaged == m_ImagedRequired);
    if (!passesImagedConstraints) return false;

    const bool isIgnored = sourceModel()->data(flagsIndex, FLAGS_ROLE).toInt() & IGNORED_BIT;
    const bool passesIgnoredConstraints = !m_IgnoredConstraintsEnabled || (isIgnored == m_IgnoredRequired);
    if (!passesIgnoredConstraints) return false;

    const bool isPicked = sourceModel()->data(flagsIndex, FLAGS_ROLE).toInt() & PICKED_BIT;
    const bool passesPickedConstraints = !m_PickedConstraintsEnabled || (isPicked == m_PickedRequired);
    if (!passesPickedConstraints) return false;

    // keyword constraint is inactive without a keyword.
    if (m_Keyword.isEmpty() || !m_KeywordConstraintsEnabled) return true;
    const QModelIndex notesIndex = sourceModel()->index(row, NOTES_COLUMN, parent);
    const QString notes = sourceModel()->data(notesIndex, NOTES_ROLE).toString();

    const bool REMatches = m_KeywordRE.match(notes).hasMatch();
    return (m_KeywordRequired == REMatches);
}

void CatalogFilter::setMinHours(double hours)
{
    m_MinHours = hours;
}

void CatalogFilter::setImagedConstraints(bool enabled, bool required)
{
    m_ImagedConstraintsEnabled = enabled;
    m_ImagedRequired = required;
}

void CatalogFilter::setPickedConstraints(bool enabled, bool required)
{
    m_PickedConstraintsEnabled = enabled;
    m_PickedRequired = required;
}

void CatalogFilter::setIgnoredConstraints(bool enabled, bool required)
{
    m_IgnoredConstraintsEnabled = enabled;
    m_IgnoredRequired = required;
}

void CatalogFilter::setKeywordConstraints(bool enabled, bool required, const QString &keyword)
{
    m_KeywordConstraintsEnabled = enabled;
    m_KeywordRequired = required;
    m_Keyword = keyword;
    m_KeywordRE = QRegularExpression(keyword);
}

void CatalogFilter::setSortColumn(int column)
{
    // Restore the original column header
    sourceModel()->setHeaderData(m_SortColumn, Qt::Horizontal, COLUMN_HEADERS[m_SortColumn].toString());

    if (column == m_SortColumn)
        m_ReverseSort = !m_ReverseSort;
    m_SortColumn = column;

    // Adjust the new column's header with the appropriate up/down arrow.
    QChar arrowChar;
    switch (m_SortColumn)
    {
        case SIZE_COLUMN:
        case ALTITUDE_COLUMN:
        case MOON_COLUMN:
        case HOURS_COLUMN:
            // The float compare is opposite of the string compare.
            arrowChar = m_ReverseSort ? QChar(0x25B2) : QChar(0x25BC);
            break;
        default:
            arrowChar = m_ReverseSort ? QChar(0x25BC) : QChar(0x25B2);
    }
    sourceModel()->setHeaderData(
        m_SortColumn, Qt::Horizontal,
        QString("%1%2").arg(arrowChar).arg(COLUMN_HEADERS[m_SortColumn].toString()));
}

// The main function used when sorting the table by a column (which is stored in m_SortColumn).
// The secondary-sort columns are hard-coded below (and commented) for each primary-sort column.
// When reversing the sort, we only reverse the primary column. The secondary sort column's
// sort is not reversed.
bool CatalogFilter::lessThan ( const QModelIndex &left, const QModelIndex &right) const
{
    double compareVal = 0;
    switch(m_SortColumn)
    {
        case NAME_COLUMN:
            // Name. There shouldn't be any ties, so no secondary sort.
            compareVal = stringCompareFcn(left, right, NAME_COLUMN, Qt::DisplayRole);
            if (m_ReverseSort) compareVal = -compareVal;
            break;
        case TYPE_COLUMN:
            // Type then hours then name. There can be plenty of ties in type and hours.
            compareVal = stringCompareFcn(left, right, TYPE_COLUMN, Qt::DisplayRole);
            if (m_ReverseSort) compareVal = -compareVal;
            if (compareVal != 0) break;
            compareVal = floatCompareFcn(left, right, HOURS_COLUMN, HOURS_ROLE);
            if (compareVal != 0) break;
            compareVal = stringCompareFcn(left, right, NAME_COLUMN, Qt::DisplayRole);
            break;
        case SIZE_COLUMN:
            // Size then hours then name. Size mostly has ties when size is unknown (== 0).
            compareVal = floatCompareFcn(left, right, SIZE_COLUMN, SIZE_ROLE);
            if (m_ReverseSort) compareVal = -compareVal;
            if (compareVal != 0) break;
            compareVal = floatCompareFcn(left, right, HOURS_COLUMN, HOURS_ROLE);
            if (compareVal != 0) break;
            compareVal = stringCompareFcn(left, right, NAME_COLUMN, Qt::DisplayRole);
            break;
        case ALTITUDE_COLUMN:
            // Altitude then hours then name. Probably altitude rarely ties.
            compareVal = floatCompareFcn(left, right, ALTITUDE_COLUMN, ALTITUDE_ROLE);
            if (m_ReverseSort) compareVal = -compareVal;
            if (compareVal != 0) break;
            compareVal = floatCompareFcn(left, right, HOURS_COLUMN, HOURS_ROLE);
            if (compareVal != 0) break;
            compareVal = stringCompareFcn(left, right, NAME_COLUMN, Qt::DisplayRole);
            break;
        case MOON_COLUMN:
            // Moon then hours then name. Probably moon rarely ties.
            compareVal = floatCompareFcn(left, right, MOON_COLUMN, MOON_ROLE);
            if (m_ReverseSort) compareVal = -compareVal;
            if (compareVal != 0) break;
            compareVal = floatCompareFcn(left, right, HOURS_COLUMN, HOURS_ROLE);
            if (compareVal != 0) break;
            compareVal = stringCompareFcn(left, right, NAME_COLUMN, Qt::DisplayRole);
            break;
        case CONSTELLATION_COLUMN:
            // Constellation, then hours, then name.
            compareVal = stringCompareFcn(left, right, CONSTELLATION_COLUMN, Qt::DisplayRole);
            if (m_ReverseSort) compareVal = -compareVal;
            if (compareVal != 0) break;
            compareVal = floatCompareFcn(left, right, HOURS_COLUMN, HOURS_ROLE);
            if (compareVal != 0) break;
            compareVal = stringCompareFcn(left, right, NAME_COLUMN, Qt::DisplayRole);
            break;
        case COORD_COLUMN:
            // Coordinate string is a weird thing to sort. Anyway, Coord, then hours, then name.
            compareVal = stringCompareFcn(left, right, COORD_COLUMN, Qt::DisplayRole);
            if (m_ReverseSort) compareVal = -compareVal;
            if (compareVal != 0) break;
            compareVal = floatCompareFcn(left, right, HOURS_COLUMN, HOURS_ROLE);
            if (compareVal != 0) break;
            compareVal = stringCompareFcn(left, right, NAME_COLUMN, Qt::DisplayRole);
            break;
        case HOURS_COLUMN:
        default:
            // In all other conditions (e.g. HOURS) sort by hours. Secondary sort is name.
            compareVal = floatCompareFcn(left, right, HOURS_COLUMN, HOURS_ROLE);
            if (m_ReverseSort) compareVal = -compareVal;
            if (compareVal != 0) break;
            compareVal = stringCompareFcn(left, right, NAME_COLUMN, Qt::DisplayRole);
            break;
    }
    return compareVal < 0;
}

ImagingPlannerUI::ImagingPlannerUI(QWidget * p) : QFrame(p)
{
    setupUi(this);
    setupIcons();
}

// Icons can't just be set up in the .ui file for Mac, so explicitly doing it here.
void ImagingPlannerUI::setupIcons()
{
    SearchB->setIcon(QIcon::fromTheme("edit-find"));
    backOneDay->setIcon(QIcon::fromTheme("arrow-left"));
    forwardOneDay->setIcon(QIcon::fromTheme("arrow-right"));
    optionsButton->setIcon(QIcon::fromTheme("open-menu-symbolic"));
    helpButton->setIcon(QIcon::fromTheme("help-about"));
    userNotesEditButton->setIcon(QIcon::fromTheme("document-edit"));
    userNotesDoneButton->setIcon(QIcon::fromTheme("checkmark"));
    userNotesOpenLink->setIcon(QIcon::fromTheme("link"));
    userNotesOpenLink2->setIcon(QIcon::fromTheme("link"));
    userNotesOpenLink3->setIcon(QIcon::fromTheme("link"));
    hideAltitudeGraphB->setIcon(QIcon::fromTheme("window-minimize"));
    showAltitudeGraphB->setIcon(QIcon::fromTheme("window-maximize"));
    hideAstrobinDetailsButton->setIcon(QIcon::fromTheme("window-minimize"));
    showAstrobinDetailsButton->setIcon(QIcon::fromTheme("window-maximize"));
    hideFilterTypesButton->setIcon(QIcon::fromTheme("window-minimize"));
    showFilterTypesButton->setIcon(QIcon::fromTheme("window-maximize"));
    hideImageButton->setIcon(QIcon::fromTheme("window-minimize"));
    showImageButton->setIcon(QIcon::fromTheme("window-maximize"));
}

GeoLocation *ImagingPlanner::getGeo()
{
    return KStarsData::Instance()->geo();
}

QDate ImagingPlanner::getDate() const
{
    return ui->DateEdit->date();
}

ImagingPlanner::ImagingPlanner() : QDialog(nullptr), m_networkManager(this), m_manager{ CatalogsDB::dso_db_path() }
{
    ui = new ImagingPlannerUI(this);

    // Seem to need these or when the user stretches the window width, the widgets
    // don't take advantage of the width.
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setLayout(mainLayout);

    setWindowTitle(i18nc("@title:window", "Imaging Planner"));
    setFocusPolicy(Qt::StrongFocus);

    if (Options::imagingPlannerIndependentWindow())
    {
        // Removing the Dialog bit (but neet to add back the window bit) allows
        // the window to go below other windows.
        setParent(nullptr, (windowFlags() & ~Qt::Dialog) | Qt::Window);
    }
    else
    {
#ifdef Q_OS_MACOS
        setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    }
    initialize();
}

// Sets up the hide/show buttons that minimize/maximize the plot/search/filters/image sections.
void ImagingPlanner::setupHideButtons(bool(*option)(), void(*setOption)(bool),
                                      QPushButton * hideButton, QPushButton * showButton,
                                      QFrame * widget, QFrame * hiddenWidget)
{
    hiddenWidget->setVisible(option());
    widget->setVisible(!option());

    connect(hideButton, &QAbstractButton::clicked, this, [this, setOption, hiddenWidget, widget]()
    {
        setOption(true);
        Options::self()->save();
        hiddenWidget->setVisible(true);
        widget->setVisible(false);
        focusOnTable();
        adjustWindowSize();
    });
    connect(showButton, &QAbstractButton::clicked, this, [this, setOption, hiddenWidget, widget]()
    {
        setOption(false);
        Options::self()->save();
        hiddenWidget->setVisible(false);
        widget->setVisible(true);
        focusOnTable();
    });
}

// Gives the keyboard focus to the CatalogView object table.
void ImagingPlanner::focusOnTable()
{
    ui->CatalogView->setFocus();
}

void ImagingPlanner::adjustWindowSize()
{
    const int keepWidth = width();
    adjustSize();
    const int newHeight = height();
    resize(keepWidth, newHeight);
}

// Sets up the galaxy/nebula/... filter buttons.
void ImagingPlanner::setupFilterButton(QCheckBox * checkbox, bool(*option)(), void(*setOption)(bool))
{
    checkbox->setChecked(option());
    connect(checkbox, &QCheckBox::toggled, [this, setOption](bool checked)
    {
        setOption(checked);
        Options::self()->save();
        m_CatalogSortModel->invalidate();
        updateDisplays();
        ui->CatalogView->resizeColumnsToContents();
        focusOnTable();
    });
}

// Sets up the picked/imaged/ignored/keyword buttons
void ImagingPlanner::setupFilter2Buttons(
    QCheckBox * yes, QCheckBox * no, QCheckBox * dontCare,
    bool(*yesOption)(), bool(*noOption)(), bool(*dontCareOption)(),
    void(*setYesOption)(bool), void(*setNoOption)(bool), void(*setDontCareOption)(bool))
{

    // Use clicked, not toggled to avoid callbacks when the state is changed programatically.
    connect(yes, &QCheckBox::clicked, [this, setYesOption, setNoOption, setDontCareOption, yes, no, dontCare](bool checked)
    {
        setupShowCallback(checked, setYesOption, setNoOption, setDontCareOption, yes, no, dontCare);
        updateSortConstraints();
        m_CatalogSortModel->invalidate();
        ui->CatalogView->resizeColumnsToContents();
        updateDisplays();
        focusOnTable();
    });
    connect(no, &QCheckBox::clicked, [this, setYesOption, setNoOption, setDontCareOption, yes, no, dontCare](bool checked)
    {
        setupShowNotCallback(checked, setYesOption, setNoOption, setDontCareOption, yes, no, dontCare);
        updateSortConstraints();
        m_CatalogSortModel->invalidate();
        ui->CatalogView->resizeColumnsToContents();
        updateDisplays();
        focusOnTable();
    });
    connect(dontCare, &QCheckBox::clicked, [this, setYesOption, setNoOption, setDontCareOption, yes, no, dontCare](bool checked)
    {
        setupDontCareCallback(checked, setYesOption, setNoOption, setDontCareOption, yes, no, dontCare);
        updateSortConstraints();
        m_CatalogSortModel->invalidate();
        ui->CatalogView->resizeColumnsToContents();
        updateDisplays();
        focusOnTable();
    });

    yes->setChecked(yesOption());
    no->setChecked(noOption());
    dontCare->setChecked(dontCareOption());
}

// Updates the QSortFilterProxyModel with new picked/imaged/ignore settings.
void ImagingPlanner::updateSortConstraints()
{
    m_CatalogSortModel->setPickedConstraints(!ui->dontCarePickedCB->isChecked(),
            ui->pickedCB->isChecked());
    m_CatalogSortModel->setImagedConstraints(!ui->dontCareImagedCB->isChecked(),
            ui->imagedCB->isChecked());
    m_CatalogSortModel->setIgnoredConstraints(!ui->dontCareIgnoredCB->isChecked(),
            ui->ignoredCB->isChecked());
    m_CatalogSortModel->setKeywordConstraints(!ui->dontCareKeywordCB->isChecked(),
            ui->keywordCB->isChecked(), ui->keywordEdit->toPlainText().trimmed());
}

// Called once, at the first viewing of the tool, to initalize all the widgets.
void ImagingPlanner::initialize()
{
    if (KStarsData::Instance() == nullptr)
    {
        QTimer::singleShot(200, this, &ImagingPlanner::initialize);
        return;
    }

    // Connects the threaded catalog loader to the UI.
    connect(this, &ImagingPlanner::popupSorry, this, &ImagingPlanner::sorry);

    // Setup the Table Views
    m_CatalogModel = new QStandardItemModel(0, LAST_COLUMN);

    QStringList columns;
    for (auto &oneColumn : COLUMN_HEADERS)
        columns << oneColumn.toString();

    // Setup the labels and tooltips for the header row of the table.
    m_CatalogModel->setHorizontalHeaderLabels(columns);
    m_CatalogModel->horizontalHeaderItem(NAME_COLUMN)->setToolTip(
        i18n("Object Name--click header to sort ascending/descending."));
    m_CatalogModel->horizontalHeaderItem(
        HOURS_COLUMN)->setToolTip(i18n("Number of hours the object can be imaged--click header to sort ascending/descending."));
    m_CatalogModel->horizontalHeaderItem(TYPE_COLUMN)->setToolTip(
        i18n("Object Type--click header to sort ascending/descending."));
    m_CatalogModel->horizontalHeaderItem(
        SIZE_COLUMN)->setToolTip(i18n("Maximum object dimension (arcmin)--click header to sort ascending/descending."));
    m_CatalogModel->horizontalHeaderItem(
        ALTITUDE_COLUMN)->setToolTip(i18n("Maximum altitude--click header to sort ascending/descending."));
    m_CatalogModel->horizontalHeaderItem(
        MOON_COLUMN)->setToolTip(i18n("Moon angular separation at midnight--click header to sort ascending/descending."));
    m_CatalogModel->horizontalHeaderItem(
        CONSTELLATION_COLUMN)->setToolTip(i18n("Constellation--click header to sort ascending/descending."));
    m_CatalogModel->horizontalHeaderItem(
        COORD_COLUMN)->setToolTip(i18n("RA/DEC coordinates--click header to sort ascending/descending."));

    m_CatalogSortModel = new CatalogFilter(this);
    m_CatalogSortModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_CatalogSortModel->setSourceModel(m_CatalogModel.data());
    m_CatalogSortModel->setDynamicSortFilter(true);

    ui->CatalogView->setModel(m_CatalogSortModel.data());
    ui->CatalogView->setSortingEnabled(false); // We explicitly control the clicking on headers.
    ui->CatalogView->horizontalHeader()->setStretchLastSection(false);
    ui->CatalogView->resizeColumnsToContents();
    ui->CatalogView->verticalHeader()->setVisible(false); // Remove the row-number display.
    ui->CatalogView->setColumnHidden(FLAGS_COLUMN, true);
    // Need to do this twice, because this is an up/down toggle when the user clicks the column header
    // and we don't want to change the sort order here.
    m_CatalogSortModel->setSortColumn(HOURS_COLUMN);
    m_CatalogSortModel->setSortColumn(HOURS_COLUMN);

    connect(ui->CatalogView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &ImagingPlanner::selectionChanged);

    // Initialize the date to KStars' date.
    if (getGeo())
    {
        auto utc = KStarsData::Instance()->clock()->utc();
        auto localTime = getGeo()->UTtoLT(utc);
        ui->DateEdit->setDate(localTime.date());
        updateMoon();
    }

    setStatus("");

    setupHideButtons(&Options::imagingPlannerHideAltitudeGraph, &Options::setImagingPlannerHideAltitudeGraph,
                     ui->hideAltitudeGraphB, ui->showAltitudeGraphB,
                     ui->AltitudeGraphFrame, ui->HiddenAltitudeGraphFrame);

    // Date buttons
    connect(ui->backOneDay, &QPushButton::clicked, this, &ImagingPlanner::moveBackOneDay);
    connect(ui->forwardOneDay, &QPushButton::clicked, this, &ImagingPlanner::moveForwardOneDay);
    connect(ui->DateEdit, &QDateTimeEdit::dateChanged, this, [this]()
    {
        QString selection = currentObjectName();
        updateMoon();
        recompute();
        updateDisplays();
        scrollToName(selection);
    });

    // Setup the section with Web search and Astrobin search details.

    // Setup Web Search buttons
    connect(ui->astrobinButton, &QPushButton::clicked, this, &ImagingPlanner::searchAstrobin);
    connect(ui->astrobinButton2, &QPushButton::clicked, this, &ImagingPlanner::searchAstrobin);
    connect(ui->searchWikipedia, &QPushButton::clicked, this, &ImagingPlanner::searchWikipedia);
    connect(ui->searchWikipedia2, &QPushButton::clicked, this, &ImagingPlanner::searchWikipedia);
    connect(ui->searchSpecialWebPageImages, &QPushButton::clicked, this, &ImagingPlanner::searchSpecialWebPageImages);
    connect(ui->searchSpecialWebPageImages2, &QPushButton::clicked, this, &ImagingPlanner::searchSpecialWebPageImages);
    connect(ui->searchSimbad, &QPushButton::clicked, this, &ImagingPlanner::searchSimbad);
    connect(ui->searchSimbad2, &QPushButton::clicked, this, &ImagingPlanner::searchSimbad);

    // These buttons are in the "hidden" catalog development section
    connect(ui->DevelCheckTargetsButton, &QPushButton::clicked, this, &ImagingPlanner::checkTargets);
    connect(ui->DevelCheckCatalogButton, &QPushButton::clicked, this, [this]()
    {
        checkTargets(true);
    });
    connect(ui->DevelCheckTargetsNextButton, &QPushButton::clicked, this, &ImagingPlanner::checkTargets2);
    connect(ui->DevelCheckTargetsPrevButton, &QPushButton::clicked, this, [this]()
    {
        checkTargets2(true);
    });
    connect(ui->DevelDownsampleButton, &QPushButton::clicked, this, [this]()
    {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Downsample Directory"));
        if (!dir.isEmpty())
        {
            if (downsampleImageFiles(dir, 300))
                fprintf(stderr, "downsampling succeeded\n");
            else
                fprintf(stderr, "downsampling failed\n");
        }
    });

    // Always start with hiding the details.
    Options::setImagingPlannerHideAstrobinDetails(true);
    setupHideButtons(&Options::imagingPlannerHideAstrobinDetails, &Options::setImagingPlannerHideAstrobinDetails,
                     ui->hideAstrobinDetailsButton, ui->showAstrobinDetailsButton,
                     ui->AstrobinSearchFrame, ui->HiddenAstrobinSearchFrame);
    ui->AstrobinAward->setChecked(Options::astrobinAward());
    connect(ui->AstrobinAward, &QAbstractButton::clicked, [this](bool checked)
    {
        Options::setAstrobinAward(checked);
        Options::self()->save();
        focusOnTable();
    });
    ui->AstrobinMinRadius->setValue(Options::astrobinMinRadius());
    connect(ui->AstrobinMinRadius, &QDoubleSpinBox::editingFinished, [this]()
    {
        Options::setAstrobinMinRadius(ui->AstrobinMinRadius->value());
        Options::self()->save();
        focusOnTable();
    });
    ui->AstrobinMaxRadius->setValue(Options::astrobinMaxRadius());
    connect(ui->AstrobinMaxRadius, &QDoubleSpinBox::editingFinished, [this]()
    {
        Options::setAstrobinMaxRadius(ui->AstrobinMaxRadius->value());
        Options::self()->save();
        focusOnTable();
    });

    // Initialize image and catalog section
    m_NoImagePixmap =
        QPixmap(":/images/noimage.png").scaled(ui->ImagePreview->width(), ui->ImagePreview->height(), Qt::KeepAspectRatio,
                Qt::FastTransformation);
    setDefaultImage();
    connect(ui->LoadCatalogButton, &QPushButton::clicked, this, &ImagingPlanner::loadCatalogViaMenu);
    connect(ui->LoadCatalogButton2, &QPushButton::clicked, this, &ImagingPlanner::loadCatalogViaMenu);
    setupHideButtons(&Options::imagingPlannerHideImage, &Options::setImagingPlannerHideImage,
                     ui->hideImageButton, ui->showImageButton,
                     ui->ImageFrame, ui->HiddenImageFrame);

    // Initialize filter section
    Options::setImagingPlannerHideFilters(true);
    setupHideButtons(&Options::imagingPlannerHideFilters, &Options::setImagingPlannerHideFilters,
                     ui->hideFilterTypesButton, ui->showFilterTypesButton,
                     ui->FilterTypesFrame, ui->HiddenFilterTypesFrame);
    setupFilterButton(ui->OpenClusterCB, &Options::imagingPlannerAcceptOpenCluster,
                      &Options::setImagingPlannerAcceptOpenCluster);
    setupFilterButton(ui->NebulaCB, &Options::imagingPlannerAcceptNebula, &Options::setImagingPlannerAcceptNebula);
    setupFilterButton(ui->GlobularClusterCB, &Options::imagingPlannerAcceptGlobularCluster,
                      &Options::setImagingPlannerAcceptGlobularCluster);
    setupFilterButton(ui->PlanetaryCB, &Options::imagingPlannerAcceptPlanetary, &Options::setImagingPlannerAcceptPlanetary);
    setupFilterButton(ui->SupernovaRemnantCB, &Options::imagingPlannerAcceptSupernovaRemnant,
                      &Options::setImagingPlannerAcceptSupernovaRemnant);
    setupFilterButton(ui->GalaxyCB, &Options::imagingPlannerAcceptGalaxy, &Options::setImagingPlannerAcceptGalaxy);
    setupFilterButton(ui->GalaxyClusterCB, &Options::imagingPlannerAcceptGalaxyCluster,
                      &Options::setImagingPlannerAcceptGalaxyCluster);
    setupFilterButton(ui->DarkNebulaCB, &Options::imagingPlannerAcceptDarkNebula, &Options::setImagingPlannerAcceptDarkNebula);
    setupFilterButton(ui->OtherCB, &Options::imagingPlannerAcceptOther, &Options::setImagingPlannerAcceptOther);

    setupFilter2Buttons(ui->pickedCB, ui->notPickedCB, ui->dontCarePickedCB,
                        &Options::imagingPlannerShowPicked, &Options::imagingPlannerShowNotPicked, &Options::imagingPlannerDontCarePicked,
                        &Options::setImagingPlannerShowPicked, &Options::setImagingPlannerShowNotPicked, &Options::setImagingPlannerDontCarePicked);

    setupFilter2Buttons(ui->imagedCB, ui->notImagedCB, ui->dontCareImagedCB,
                        &Options::imagingPlannerShowImaged, &Options::imagingPlannerShowNotImaged, &Options::imagingPlannerDontCareImaged,
                        &Options::setImagingPlannerShowImaged, &Options::setImagingPlannerShowNotImaged, &Options::setImagingPlannerDontCareImaged);

    setupFilter2Buttons(ui->ignoredCB, ui->notIgnoredCB, ui->dontCareIgnoredCB,
                        &Options::imagingPlannerShowIgnored, &Options::imagingPlannerShowNotIgnored, &Options::imagingPlannerDontCareIgnored,
                        &Options::setImagingPlannerShowIgnored, &Options::setImagingPlannerShowNotIgnored,
                        &Options::setImagingPlannerDontCareIgnored);

    ui->keywordEdit->setText(Options::imagingPlannerKeyword());
    ui->keywordEdit->setAcceptRichText(false);
    m_Keyword = Options::imagingPlannerKeyword();
    setupFilter2Buttons(ui->keywordCB, ui->notKeywordCB, ui->dontCareKeywordCB,
                        &Options::imagingPlannerShowKeyword, &Options::imagingPlannerShowNotKeyword, &Options::imagingPlannerDontCareKeyword,
                        &Options::setImagingPlannerShowKeyword, &Options::setImagingPlannerShowNotKeyword,
                        &Options::setImagingPlannerDontCareKeyword);

    ui->keywordEdit->setFocusPolicy(Qt::StrongFocus);

    // Initialize the altitude/moon/hours inputs
    ui->useArtificialHorizon->setChecked(Options::imagingPlannerUseArtificialHorizon());
    m_UseArtificialHorizon = Options::imagingPlannerUseArtificialHorizon();
    ui->minMoon->setValue(Options::imagingPlannerMinMoonSeparation());
    m_MinMoon = Options::imagingPlannerMinMoonSeparation();
    ui->maxMoonAltitude->setValue(Options::imagingPlannerMaxMoonAltitude());
    m_MaxMoonAltitude = Options::imagingPlannerMaxMoonAltitude();
    ui->minAltitude->setValue(Options::imagingPlannerMinAltitude());
    m_MinAltitude = Options::imagingPlannerMinAltitude();
    ui->minHours->setValue(Options::imagingPlannerMinHours());
    m_MinHours = Options::imagingPlannerMinHours();
    m_CatalogSortModel->setMinHours(Options::imagingPlannerMinHours());
    connect(ui->useArtificialHorizon, &QCheckBox::toggled, [this]()
    {
        if (m_UseArtificialHorizon == ui->useArtificialHorizon->isChecked())
            return;
        m_UseArtificialHorizon = ui->useArtificialHorizon->isChecked();
        Options::setImagingPlannerUseArtificialHorizon(ui->useArtificialHorizon->isChecked());
        Options::self()->save();
        recompute();
        updateDisplays();
    });
    connect(ui->minMoon, &QDoubleSpinBox::editingFinished, [this]()
    {
        if (m_MinMoon == ui->minMoon->value())
            return;
        m_MinMoon = ui->minMoon->value();
        Options::setImagingPlannerMinMoonSeparation(ui->minMoon->value());
        Options::self()->save();
        recompute();
        updateDisplays();
    });
    connect(ui->maxMoonAltitude, &QDoubleSpinBox::editingFinished, [this]()
    {
        if (m_MaxMoonAltitude == ui->maxMoonAltitude->value())
            return;
        m_MaxMoonAltitude = ui->maxMoonAltitude->value();
        Options::setImagingPlannerMaxMoonAltitude(ui->maxMoonAltitude->value());
        Options::self()->save();
        recompute();
        updateDisplays();
    });
    connect(ui->minAltitude, &QDoubleSpinBox::editingFinished, [this]()
    {
        if (m_MinAltitude == ui->minAltitude->value())
            return;
        m_MinAltitude = ui->minAltitude->value();
        Options::setImagingPlannerMinAltitude(ui->minAltitude->value());
        Options::self()->save();
        recompute();
        updateDisplays();
    });
    connect(ui->minHours, &QDoubleSpinBox::editingFinished, [this]()
    {
        if (m_MinHours == ui->minHours->value())
            return;
        m_MinHours = ui->minHours->value();
        Options::setImagingPlannerMinHours(ui->minHours->value());
        Options::self()->save();
        m_CatalogSortModel->setMinHours(Options::imagingPlannerMinHours());
        m_CatalogSortModel->invalidate();
        ui->CatalogView->resizeColumnsToContents();
        updateDisplays();
    });

    updateSortConstraints();

    m_CatalogSortModel->setMinHours(ui->minHours->value());

    ui->CatalogView->setColumnHidden(NOTES_COLUMN, true);

    initUserNotes();

    connect(ui->userNotesDoneButton, &QAbstractButton::clicked, this, &ImagingPlanner::userNotesEditFinished);
    ui->userNotesEdit->setFocusPolicy(Qt::StrongFocus);

    connect(ui->userNotesEditButton, &QAbstractButton::clicked, this, [this]()
    {
        ui->userNotesLabel->setVisible(true);
        ui->userNotesEdit->setText(ui->userNotes->text());
        ui->userNotesEdit->setVisible(true);
        ui->userNotesEditButton->setVisible(false);
        ui->userNotesDoneButton->setVisible(true);
        ui->userNotes->setVisible(false);
        ui->userNotesLabel->setVisible(true);
        ui->userNotesOpenLink->setVisible(false);
        ui->userNotesOpenLink2->setVisible(false);
        ui->userNotesOpenLink3->setVisible(false);
    });

    connect(ui->userNotesOpenLink, &QAbstractButton::clicked, this, [this]()
    {
        focusOnTable();
        QString urlString = findUrl(ui->userNotes->text());
        if (urlString.isEmpty())
            return;
        QDesktopServices::openUrl(QUrl(urlString));
    });
    connect(ui->userNotesOpenLink2, &QAbstractButton::clicked, this, [this]()
    {
        focusOnTable();
        QString urlString = findUrl(ui->userNotes->text(), 2);
        if (urlString.isEmpty())
            return;
        QDesktopServices::openUrl(QUrl(urlString));
    });
    connect(ui->userNotesOpenLink3, &QAbstractButton::clicked, this, [this]()
    {
        focusOnTable();
        QString urlString = findUrl(ui->userNotes->text(), 3);
        if (urlString.isEmpty())
            return;
        QDesktopServices::openUrl(QUrl(urlString));
    });

    connect(ui->loadImagedB, &QPushButton::clicked, this, &ImagingPlanner::loadImagedFile);

    connect(ui->SearchB, &QPushButton::clicked, this, &ImagingPlanner::searchSlot);

    connect(ui->CatalogView->horizontalHeader(), &QHeaderView::sectionPressed, this, [this](int column)
    {
        m_CatalogSortModel->setSortColumn(column);
        m_CatalogSortModel->invalidate();
        ui->CatalogView->resizeColumnsToContents();
    });

    adjustWindowSize();

    connect(ui->optionsButton, &QPushButton::clicked, this, &ImagingPlanner::openOptionsMenu);

    // Since we thread the loading of catalogs, need to connect the thread back to UI.
    qRegisterMetaType<QList<QStandardItem * >> ("QList<QStandardItem *>");
    connect(this, &ImagingPlanner::addRow, this, &ImagingPlanner::addRowSlot);

    // Needed to fix weird bug on Windows that started with Qt 5.9 that makes the title bar
    // not visible and therefore dialog not movable.
#ifdef Q_OS_WIN
    move(100, 100);
#endif

    // Install the event filters. Put them at the end of initialize so
    // the event filter isn't called until initialize is complete.
    installEventFilters();

    m_PlateSolve.reset(new PlateSolve(this));
    m_PlateSolve->enableAuxButton("Retake screenshot",
                                  "Retake the screenshot of the object if you're having issues solving.");
    connect(m_PlateSolve.get(), &PlateSolve::clicked, this, &ImagingPlanner::extractImage, Qt::UniqueConnection);
    connect(m_PlateSolve.get(), &PlateSolve::auxClicked, this, [this]()
    {
        m_PlateSolve->abort();
        takeScreenshot();
    });
}

void ImagingPlanner::installEventFilters()
{
    // Install the event filters. Put them at the end of initialize so
    // the event filter isn't called until initialize is complete.
    ui->SearchText->installEventFilter(this);
    ui->userNotesEdit->installEventFilter(this);
    ui->keywordEdit->installEventFilter(this);
    ui->ImagePreviewCreditLink->installEventFilter(this);
    ui->ImagePreviewCredit->installEventFilter(this);
    ui->ImagePreview->installEventFilter(this);
    ui->CatalogView->viewport()->installEventFilter(this);
    ui->CatalogView->installEventFilter(this);
    ui->helpButton->installEventFilter(this);
}

void ImagingPlanner::removeEventFilters()
{
    ui->SearchText->removeEventFilter(this);
    ui->userNotesEdit->removeEventFilter(this);
    ui->keywordEdit->removeEventFilter(this);
    ui->ImagePreviewCreditLink->removeEventFilter(this);
    ui->ImagePreviewCredit->removeEventFilter(this);
    ui->ImagePreview->removeEventFilter(this);
    ui->CatalogView->viewport()->removeEventFilter(this);
    ui->helpButton->removeEventFilter(this);
}

void ImagingPlanner::openOptionsMenu()
{
    QSharedPointer<ImagingPlannerOptions> options(new ImagingPlannerOptions(this));
    options->exec();
    focusOnTable();
}

// KDE KHelpClient::invokeHelp() doesn't seem to work.
void ImagingPlanner::getHelp()
{
    focusOnTable();
    const QUrl url("https://kstars-docs.kde.org/en/user_manual/tool-imaging-planner.html");
    if (!url.isEmpty())
        QDesktopServices::openUrl(url);
}

KSMoon *ImagingPlanner::getMoon()
{
    if (KStarsData::Instance() == nullptr)
        return nullptr;

    KSMoon *moon = dynamic_cast<KSMoon *>(KStarsData::Instance()->skyComposite()->findByName(i18n("Moon")));
    if (moon)
    {
        auto tz = QTimeZone(getGeo()->TZ() * 3600);
        KStarsDateTime midnight = KStarsDateTime(getDate().addDays(1), QTime(0, 1));
        midnight.setTimeZone(tz);
        CachingDms LST = getGeo()->GSTtoLST(getGeo()->LTtoUT(midnight).gst());
        KSNumbers numbers(midnight.djd());
        moon->updateCoords(&numbers, true, getGeo()->lat(), &LST, true);
    }
    return moon;
}

// Setup the moon image.
void ImagingPlanner::updateMoon()
{
    KSMoon *moon = getMoon();
    if (!moon)
        return;

    // You need to know the sun's position in order to get the right phase of the moon.
    auto tz = QTimeZone(getGeo()->TZ() * 3600);
    KStarsDateTime midnight = KStarsDateTime(getDate().addDays(1), QTime(0, 1));
    CachingDms LST = getGeo()->GSTtoLST(getGeo()->LTtoUT(midnight).gst());
    KSNumbers numbers(midnight.djd());
    KSSun *sun = dynamic_cast<KSSun *>(KStarsData::Instance()->skyComposite()->findByName(i18n("Sun")));
    sun->updateCoords(&numbers, true, getGeo()->lat(), &LST, true);
    moon->findPhase(sun);

    ui->moonImage->setPixmap(QPixmap::fromImage(moon->image().scaled(32, 32, Qt::KeepAspectRatio)));
    ui->moonPercentLabel->setText(QString("%1%").arg(moon->illum() * 100.0 + 0.5, 0, 'f', 0));
}

bool ImagingPlanner::scrollToName(const QString &name)
{
    if (name.isEmpty())
        return false;
    QModelIndexList matchList = ui->CatalogView->model()->match(ui->CatalogView->model()->index(0, 0), Qt::EditRole,
                                name, -1,  Qt::MatchFlags(Qt::MatchContains | Qt::MatchWrap));
    if(matchList.count() >= 1)
    {
        int bestIndex = 0;
        for (int i = 0; i < matchList.count(); i++)
        {
            QString nn = ui->CatalogView->model()->data(matchList[i], Qt::DisplayRole).toString();
            if (nn.compare(name, Qt::CaseInsensitive) == 0)
            {
                bestIndex = i;
                break;
            }
        }
        ui->CatalogView->scrollTo(matchList[bestIndex]);
        ui->CatalogView->setCurrentIndex(matchList[bestIndex]);
        return true;
    }
    return false;
}

void ImagingPlanner::searchSlot()
{
    if (m_loadingCatalog)
        return;
    QString origName = ui->SearchText->toPlainText().trimmed();
    QString name = tweakNames(origName);
    ui->SearchText->setPlainText(name);
    if (name.isEmpty())
        return;

    if (!scrollToName(name))
        KSNotification::sorry(i18n("No match for \"%1\"", origName));

    // Still leaves around some </p> in the html unfortunaltely. Don't know how to remove that.
    ui->SearchText->clear();
    ui->SearchText->setPlainText("");
}

void ImagingPlanner::initUserNotes()
{
    ui->userNotesLabel->setVisible(true);
    ui->userNotesEdit->setVisible(false);
    ui->userNotesEditButton->setVisible(true);
    ui->userNotesDoneButton->setVisible(false);
    ui->userNotes->setVisible(true);
    ui->userNotesLabel->setVisible(true);
    ui->userNotesOpenLink->setVisible(false);
    ui->userNotesOpenLink2->setVisible(false);
    ui->userNotesOpenLink3->setVisible(false);
}

void ImagingPlanner::disableUserNotes()
{
    ui->userNotesEdit->setVisible(false);
    ui->userNotesEditButton->setVisible(false);
    ui->userNotesDoneButton->setVisible(false);
    ui->userNotes->setVisible(false);
    ui->userNotesLabel->setVisible(false);
    ui->userNotesOpenLink->setVisible(false);
    ui->userNotesOpenLink2->setVisible(false);
    ui->userNotesOpenLink3->setVisible(false);
}

void ImagingPlanner::userNotesEditFinished()
{
    const QString &notes = ui->userNotesEdit->toPlainText().trimmed();
    ui->userNotes->setText(notes);
    ui->userNotesLabel->setVisible(notes.isEmpty());
    ui->userNotesEdit->setVisible(false);
    ui->userNotesEditButton->setVisible(true);
    ui->userNotesDoneButton->setVisible(false);
    ui->userNotes->setVisible(true);
    ui->userNotesLabel->setVisible(true);
    setCurrentObjectNotes(notes);
    setupNotesLinks(notes);
    focusOnTable();
    auto o = currentCatalogObject();
    if (!o) return;
    saveToDB(currentObjectName(), currentObjectFlags(), notes);
}

void ImagingPlanner::updateNotes(const QString &notes)
{
    ui->userNotes->setMaximumWidth(ui->RightPanel->width() - 125);
    initUserNotes();
    ui->userNotes->setText(notes);
    ui->userNotesLabel->setVisible(notes.isEmpty());
    setupNotesLinks(notes);
}

void ImagingPlanner::setupNotesLinks(const QString &notes)
{
    QString link = findUrl(notes);
    ui->userNotesOpenLink->setVisible(!link.isEmpty());
    if (!link.isEmpty())
        ui->userNotesOpenLink->setToolTip(i18n("Open a browser with the 1st link in this note: %1", link));

    link = findUrl(notes, 2);
    ui->userNotesOpenLink2->setVisible(!link.isEmpty());
    if (!link.isEmpty())
        ui->userNotesOpenLink2->setToolTip(i18n("Open a browser with the 2nd link in this note: %1", link));

    link = findUrl(notes, 3);
    ui->userNotesOpenLink3->setVisible(!link.isEmpty());
    if (!link.isEmpty())
        ui->userNotesOpenLink3->setToolTip(i18n("Open a browser with the 3rd link in this note: %1", link));
}

bool ImagingPlanner::internetNameSearch(const QString &name, bool abellPlanetary, int abellNumber,
                                        CatalogObject * catObject)
{
    DPRINTF(stderr, "***** internetNameSearch(%s)\n", name.toLatin1().data());
    QElapsedTimer timer;
    timer.start();
    QString filteredName = name;
    // The resolveName search is touchy about the dash.
    if (filteredName.startsWith("sh2", Qt::CaseInsensitive))
        filteredName.replace(QRegularExpression("sh2\\s*-?", QRegularExpression::CaseInsensitiveOption), "sh2-");
    QString resolverName = filteredName;
    if (abellPlanetary)
    {
        // Use "PN A66 ##" instead of "Abell ##" for name resolver
        resolverName = QString("PN A66 %1").arg(abellNumber);
    }

    const auto &cedata = NameResolver::resolveName(resolverName);
    if (!cedata.first)
        return false;

    CatalogObject object = cedata.second;
    if (abellPlanetary)
    {
        if (object.name() == object.name2())
            object.setName2(filteredName);
        object.setName(filteredName);
    }

    m_manager.add_object(CatalogsDB::user_catalog_id, object);
    const auto &added_object =
        m_manager.get_object(object.getId(), CatalogsDB::user_catalog_id);

    if (added_object.first)
    {
        *catObject = KStarsData::Instance()
                     ->skyComposite()
                     ->catalogsComponent()
                     ->insertStaticObject(added_object.second);
    }

    DPRINTF(stderr, "***** Found %s using name resolver (%.1fs)\n", name.toLatin1().data(),
            timer.elapsed() / 1000.0);
    return true;
}

bool isAbellPlanetary(const QString &name, int *number)
{
    *number = -1;
    if (name.startsWith("Abell", Qt::CaseInsensitive))
    {
        QRegularExpression abellRE("Abell\\s*(\\d+)\\s*", QRegularExpression::CaseInsensitiveOption);
        auto match = abellRE.match(name);
        if (match.hasMatch())
        {
            *number = match.captured(1).toInt();
            if (*number <= 86)
                return true;
        }
    }
    return false;
}

// Given an object name, return the KStars catalog object.
bool ImagingPlanner::getKStarsCatalogObject(const QString &name, CatalogObject * catObject)
{
    DPRINTF(stderr, "getKStarsCatalogObject(%s)\n", name.toLatin1().data());
    // find_objects_by_name is much faster with exactMatchOnly=true.
    // Therefore, since most will match exactly given the string pre-processing,
    // first try exact=true, and if that fails, follow up with exact=false.
    QString filteredName = FindDialog::processSearchText(name).toUpper();
    std::list<CatalogObject> objs =
        m_manager.find_objects_by_name(filteredName, 1, true);

    // Don't accept objects that are Abell, have number <= 86 and are galaxy clusters.
    // Those were almost definitely planetary nebulae confused by Simbad/NameResolver.
    int abellNumber = 0;
    bool abellPlanetary = isAbellPlanetary(name, &abellNumber);
    if (objs.size() > 0 && abellPlanetary && objs.front().type() == SkyObject::GALAXY_CLUSTER)
        objs.clear();

    if (objs.size() == 0 && filteredName.size() > 0)
    {
        // Try capitalizing
        const QString capitalized = capitalize(filteredName);
        objs = m_manager.find_objects_by_name(capitalized, 1, true);
        if (objs.size() > 0 && abellPlanetary && objs.front().type() == SkyObject::GALAXY_CLUSTER)
            objs.clear();

        if (objs.size() == 0)
        {
            // Try lowercase
            const QString lowerCase = filteredName.toLower();
            objs = m_manager.find_objects_by_name(lowerCase, 1, true);
            if (objs.size() > 0 && abellPlanetary && objs.front().type() == SkyObject::GALAXY_CLUSTER)
                objs.clear();
        }
    }

    // If we didn't find it and it's Sharpless, try sh2 with a space instead of a dash
    // and vica versa
    if (objs.size() == 0 && filteredName.startsWith("sh2-", Qt::CaseInsensitive))
    {
        QString name2 = filteredName;
        name2.replace(QRegularExpression("sh2-", QRegularExpression::CaseInsensitiveOption), "sh2 ");
        objs = m_manager.find_objects_by_name(name2, 1, true);
    }
    if (objs.size() == 0 && filteredName.startsWith("sh2 ", Qt::CaseInsensitive))
    {
        QString name2 = filteredName;
        name2.replace(QRegularExpression("sh2 ", QRegularExpression::CaseInsensitiveOption), "sh2-");
        objs = m_manager.find_objects_by_name(name2, 1, true);
    }

    if (objs.size() == 0 && !abellPlanetary)
        objs = m_manager.find_objects_by_name(filteredName.toLower(), 20, false);
    if (objs.size() == 0)
        return internetNameSearch(filteredName, abellPlanetary,  abellNumber, catObject);

    if (objs.size() == 0)
        return false;

    // If there's a match, see if there's an exact match in name, name2, or longname.
    *catObject = objs.front();
    if (objs.size() >= 1)
    {
        bool foundIt = false;
        QString addSpace = filteredName;
        addSpace.append(" ");
        QString addComma = filteredName;
        addComma.append(",");
        QString sh2Fix = filteredName;
        sh2Fix.replace(QRegularExpression("sh2 ", QRegularExpression::CaseInsensitiveOption), "sh2-");
        for (const auto &obj : objs)
        {
            if ((filteredName.compare(obj.name(), Qt::CaseInsensitive) == 0) ||
                    (filteredName.compare(obj.name2(), Qt::CaseInsensitive) == 0) ||
                    obj.longname().contains(addSpace, Qt::CaseInsensitive) ||
                    obj.longname().contains(addComma, Qt::CaseInsensitive) ||
                    obj.longname().endsWith(filteredName, Qt::CaseInsensitive) ||
                    (sh2Fix.compare(obj.name(), Qt::CaseInsensitive) == 0) ||
                    (sh2Fix.compare(obj.name2(), Qt::CaseInsensitive) == 0)
               )
            {
                *catObject = obj;
                foundIt = true;
                break;
            }
        }
        if (!foundIt)
        {
            if (objs.size() == 1)
                DPRINTF(stderr, " ========> \"%s\" had 1 match \"%s\", but not trusting it!!!!\n", name.toLatin1().data(),
                        objs.front().name().toLatin1().data());

            if (internetNameSearch(filteredName, abellPlanetary,  abellNumber, catObject))
                return true;

            DPRINTF(stderr, "Didn't find %s (%s) -- Not using name \"%s\" name2 \"%s\" longname \"%s\"\n",
                    name.toLatin1().data(), filteredName.toLatin1().data(), catObject->name().toLatin1().data(),
                    catObject->name2().toLatin1().data(),
                    catObject->longname().toLatin1().data());
            return false;
        }
    }
    return true;
}

CatalogObject *ImagingPlanner::getObject(const QString &name)
{
    if (name.isEmpty())
        return nullptr;
    QString lName = name.toLower();
    auto o = m_CatalogHash.find(lName);
    if (o == m_CatalogHash.end())
        return nullptr;
    return &(*o);
}

void ImagingPlanner::clearObjects()
{
    // Important to tell SkyMap that our objects are gone.
    // We give SkyMap points to these objects in ImagingPlanner::centerOnSkymap()
    SkyMap::Instance()->setClickedObject(nullptr);
    SkyMap::Instance()->setFocusObject(nullptr);
    m_CatalogHash.clear();
}

CatalogObject *ImagingPlanner::addObject(const QString &name)
{
    if (name.isEmpty())
        return nullptr;
    QString lName = name.toLower();
    if (getObject(lName) != nullptr)
    {
        DPRINTF(stderr, "Didn't add \"%s\" because it's already there\n", name.toLatin1().data());
        return nullptr;
    }

    CatalogObject o;
    if (!getKStarsCatalogObject(lName, &o))
    {
        DPRINTF(stderr, "************* Couldn't find \"%s\"\n", lName.toLatin1().data());
        return nullptr;
    }
    m_CatalogHash[lName] = o;
    return &(m_CatalogHash[lName]);
}

// Adds the object to the catalog model, assuming a KStars catalog object can be found
// for that name.
bool ImagingPlanner::addCatalogItem(const KSAlmanac &ksal, const QString &name, int flags)
{
    CatalogObject *object = addObject(name);
    if (object == nullptr)
        return false;

    auto getItemWithUserRole = [](const QString & itemText) -> QStandardItem *
    {
        QStandardItem *ret = new QStandardItem(itemText);
        ret->setData(itemText, Qt::UserRole);
        ret->setTextAlignment(Qt::AlignHCenter);
        return ret;
    };

    // Build the data. The columns must be the same as the #define columns at the top of this file.
    QList<QStandardItem *> itemList;
    for (int i = 0; i < LAST_COLUMN; ++i)
    {
        if (i == NAME_COLUMN)
        {
            itemList.append(getItemWithUserRole(name));
        }
        else if (i == HOURS_COLUMN)
        {
            double runHours = getRunHours(*object, getDate(), *getGeo(), ui->minAltitude->value(), ui->minMoon->value(),
                                          ui->maxMoonAltitude->value(), ui->useArtificialHorizon->isChecked());
            auto hoursItem = getItemWithUserRole(QString("%1").arg(runHours, 0, 'f', 1));
            hoursItem->setData(runHours, HOURS_ROLE);
            itemList.append(hoursItem);
        }
        else if (i == TYPE_COLUMN)
        {
            auto typeItem = getItemWithUserRole(QString("%1").arg(SkyObject::typeShortName(object->type())));
            typeItem->setData(object->type(), TYPE_ROLE);
            itemList.append(typeItem);
        }
        else if (i == SIZE_COLUMN)
        {
            double size = std::max(object->a(), object->b());
            auto sizeItem = getItemWithUserRole(QString("%1'").arg(size, 0, 'f', 1));
            sizeItem->setData(size, SIZE_ROLE);
            itemList.append(sizeItem);
        }
        else if (i == ALTITUDE_COLUMN)
        {
            const auto time = KStarsDateTime(QDateTime(getDate(), QTime(12, 0)));
            const double altitude = getMaxAltitude(ksal, getDate(), getGeo(), *object, 0, 0);
            auto altItem = getItemWithUserRole(QString("%1º").arg(altitude, 0, 'f', 0));
            altItem->setData(altitude, ALTITUDE_ROLE);
            itemList.append(altItem);
        }
        else if (i == MOON_COLUMN)
        {
            KSMoon *moon = getMoon();
            if (moon)
            {
                SkyPoint o;
                o.setRA0(object->ra0());
                o.setDec0(object->dec0());
                auto tz = QTimeZone(getGeo()->TZ() * 3600);
                KStarsDateTime midnight = KStarsDateTime(getDate().addDays(1), QTime(0, 1));
                midnight.setTimeZone(tz);
                KSNumbers numbers(midnight.djd());
                o.updateCoordsNow(&numbers);

                double const separation = moon->angularDistanceTo(&o).Degrees();
                auto moonItem = getItemWithUserRole(QString("%1º").arg(separation, 0, 'f', 0));
                moonItem->setData(separation, MOON_ROLE);
                itemList.append(moonItem);
            }
            else
            {
                auto moonItem = getItemWithUserRole(QString(""));
                moonItem->setData(-1, MOON_ROLE);
            }
        }
        else if (i == CONSTELLATION_COLUMN)
        {
            QString cname = KStarsData::Instance()
                            ->skyComposite()
                            ->constellationBoundary()
                            ->constellationName(object);
            cname = cname.toLower().replace(0, 1, cname[0].toUpper());
            auto constellationItem = getItemWithUserRole(cname);
            itemList.append(constellationItem);
        }
        else if (i == COORD_COLUMN)
        {
            itemList.append(getItemWithUserRole(shortCoordString(object->ra0(), object->dec0())));
        }
        else if (i == FLAGS_COLUMN)
        {
            QStandardItem *flag = getItemWithUserRole("flag");
            flag->setData(flags, FLAGS_ROLE);
            itemList.append(flag);
        }
        else if (i == NOTES_COLUMN)
        {
            QStandardItem *notes = getItemWithUserRole("notes");
            notes->setData(QString(), NOTES_ROLE);
            itemList.append(notes);
        }
        else
        {
            DPRINTF(stderr, "Bug in addCatalogItem() !\n");
        }
    }

    // Can't do UI in this thread, must move back to the UI thread.
    emit addRow(itemList);
    return true;
}

void ImagingPlanner::addRowSlot(QList<QStandardItem *> itemList)
{
    m_CatalogModel->appendRow(itemList);
    updateCounts();
}

void ImagingPlanner::recompute()
{
    setStatus(i18n("Updating tables..."));

    // Disconnect the filter from the model, or else we'll re-filter numRows squared times.
    m_CatalogSortModel->setSourceModel(nullptr);

    QElapsedTimer timer;
    timer.start();

    auto tz = QTimeZone(getGeo()->TZ() * 3600);
    KStarsDateTime midnight = KStarsDateTime(getDate().addDays(1), QTime(0, 1));
    KStarsDateTime ut  = getGeo()->LTtoUT(KStarsDateTime(midnight));
    KSAlmanac ksal(ut, getGeo());

    for (int i = 0; i < m_CatalogModel->rowCount(); ++i)
    {
        const QString &name = m_CatalogModel->item(i, 0)->text();
        const CatalogObject *catalogEntry = getObject(name);
        if (catalogEntry == nullptr)
        {
            DPRINTF(stderr, "************* Couldn't find \"%s\"\n", name.toLatin1().data());
            return;
        }
        double runHours = getRunHours(*catalogEntry, getDate(), *getGeo(), ui->minAltitude->value(),
                                      ui->minMoon->value(), ui->maxMoonAltitude->value(), ui->useArtificialHorizon->isChecked());
        QString hoursText = QString("%1").arg(runHours, 0, 'f', 1);
        QStandardItem *hItem = new QStandardItem(hoursText);
        hItem->setData(hoursText, Qt::UserRole);
        hItem->setTextAlignment(Qt::AlignHCenter);
        hItem->setData(runHours, HOURS_ROLE);
        m_CatalogModel->setItem(i, HOURS_COLUMN, hItem);


        const auto time = KStarsDateTime(QDateTime(getDate(), QTime(12, 0)));
        const double altitude = getMaxAltitude(ksal, getDate(), getGeo(), *catalogEntry, 0, 0);
        QString altText = QString("%1º").arg(altitude, 0, 'f', 0);
        auto altItem = new QStandardItem(altText);
        altItem->setData(altText, Qt::UserRole);
        altItem->setData(altitude, ALTITUDE_ROLE);
        m_CatalogModel->setItem(i, ALTITUDE_COLUMN, altItem);

        KSMoon *moon = getMoon();
        if (moon)
        {
            SkyPoint o;
            o.setRA0(catalogEntry->ra0());
            o.setDec0(catalogEntry->dec0());
            auto tz = QTimeZone(getGeo()->TZ() * 3600);
            KStarsDateTime midnight = KStarsDateTime(getDate().addDays(1), QTime(0, 1));
            midnight.setTimeZone(tz);
            KSNumbers numbers(midnight.djd());
            o.updateCoordsNow(&numbers);

            double const separation = moon->angularDistanceTo(&o).Degrees();
            QString moonText = QString("%1º").arg(separation, 0, 'f', 0);
            auto moonItem = new QStandardItem(moonText);
            moonItem->setData(moonText, Qt::UserRole);
            moonItem->setData(separation, MOON_ROLE);
            m_CatalogModel->setItem(i, MOON_COLUMN, moonItem);
        }
        else
        {
            auto moonItem = new QStandardItem("");
            moonItem->setData("", Qt::UserRole);
            moonItem->setData(-1, MOON_ROLE);
            m_CatalogModel->setItem(i, MOON_COLUMN, moonItem);
        }

        // Don't lose the imaged background highlighting.
        const bool imaged = m_CatalogModel->item(i, FLAGS_COLUMN)->data(FLAGS_ROLE).toInt() & IMAGED_BIT;
        if (imaged)
            highlightImagedObject(m_CatalogModel->index(i, NAME_COLUMN), true);
        const bool picked = m_CatalogModel->item(i, FLAGS_COLUMN)->data(FLAGS_ROLE).toInt() & PICKED_BIT;
        if (picked)
            highlightPickedObject(m_CatalogModel->index(i, NAME_COLUMN), true);
    }
    // Reconnect the filter to the model.
    m_CatalogSortModel->setSourceModel(m_CatalogModel.data());
    // Need to do this again, I guess because we disconnected/reconnected above.
    ui->CatalogView->setColumnHidden(FLAGS_COLUMN, true);
    ui->CatalogView->setColumnHidden(NOTES_COLUMN, true);
    ui->CatalogView->resizeColumnsToContents();

    DPRINTF(stderr, "Recompute took %.1fs\n", timer.elapsed() / 1000.0);
    updateStatus();
}

// This section used in catalog development--methods checkTargets() and checkTargets2().
// CheckTargets() either reads in a file of target names, one per line, and sees if they
// can be found, and also computes distances between these targets and existing catalog
// targets, and other new targets. It can also work with just catalog targets.
// CheckTargets2() helps with browsing these objects on the SkyMap using flags.
namespace
{
bool ALREADY_CHECKING = false;
int ALREADY_CHECKING_INDEX = -1;
QList<CatalogObject> addedObjects;
struct ObjectNeighbor
{
    CatalogObject object;
    double distance;
    QString neighbor;
    ObjectNeighbor(CatalogObject o, double d, QString nei) : object(o), distance(d), neighbor(nei) {}
};
QList<ObjectNeighbor> sortedAddedObjects;
}  // namespace

// CheckTargets2() browses the objects read in in checkTargets().
void ImagingPlanner::checkTargets2(bool backwards)
{
    if (ALREADY_CHECKING)
    {
        if (backwards)
            ALREADY_CHECKING_INDEX--;
        else
            ALREADY_CHECKING_INDEX++;

        if (sortedAddedObjects.size() == 0)
        {
            fprintf(stderr, "No TARGETS\n");
            return;
        }
        if (ALREADY_CHECKING_INDEX >= sortedAddedObjects.size())
            ALREADY_CHECKING_INDEX = 0;
        else if (ALREADY_CHECKING_INDEX < 0)
            ALREADY_CHECKING_INDEX = sortedAddedObjects.size() - 1;
        KStarsDateTime time = KStarsData::Instance()->clock()->utc();
        dms lst = getGeo()->GSTtoLST(time.gst());
        CatalogObject &o = sortedAddedObjects[ALREADY_CHECKING_INDEX].object;
        o.EquatorialToHorizontal(&lst, getGeo()->lat());
        fprintf(stderr, "%d: %s\n", ALREADY_CHECKING_INDEX, o.name().toLatin1().data());

        // Doing this to avoid the pop-up warning that an object is below the ground.
        bool keepGround = Options::showGround();
        bool keepAnimatedSlew = Options::useAnimatedSlewing();
        Options::setShowGround(false);
        Options::setUseAnimatedSlewing(false);
        SkyMap::Instance()->setClickedObject(&o);
        SkyMap::Instance()->setClickedPoint(&o);
        SkyMap::Instance()->slotCenter();
        Options::setShowGround(keepGround);
        Options::setUseAnimatedSlewing(keepAnimatedSlew);
    }
}

// Puts flags on all existing and proposed catalog targets, computes distances,
// and sets up some browsing in the above method.
void ImagingPlanner::checkTargets(bool justCheckCurrentCatalog)
{
    if (ALREADY_CHECKING)
    {
        checkTargets2(false);
        return;
    }
    ALREADY_CHECKING = true;

    // Put flags for all existing targets.
    FlagComponent *flags = KStarsData::Instance()->skyComposite()->flags();
    for (int i = flags->size() - 1; i >= 0; --i) flags->remove(i);
    int rows = m_CatalogModel->rowCount();

    int numFlags = 0;
    for (int i = 0; i < rows; ++i)
    {
        const QString &name = m_CatalogModel->item(i, NAME_COLUMN)->text();
        auto object = getObject(name);
        if (object)
        {
            numFlags++;
            flags->add(SkyPoint(object->ra(), object->dec()), "J2000.0", "", name, Qt::red);
        }
    }
    fprintf(stderr, "Added %d flags\n", numFlags);


    // Read a file with a list of proposed new targets.
    QList<QString> targets;
    QList<QString> newObjects;
    if (!justCheckCurrentCatalog)
    {
        QString fileName = QFileDialog::getOpenFileName(this,
                           tr("Targets Filename"), QDir::homePath(), tr("Any files (*)"));
        if (fileName.isEmpty())
            return;
        QFile inputFile(fileName);
        if (inputFile.open(QIODevice::ReadOnly))
        {
            QTextStream in(&inputFile);
            while (!in.atEnd())
            {
                const QString line = in.readLine().trimmed();
                if (line.size() > 0 && line[0] != '#' && newObjects.indexOf(line) == -1)
                    newObjects.push_back(line);
            }
            inputFile.close();
        }
        if (newObjects.size() == 0)
        {
            fprintf(stderr, "No New Targets\n");
            return;
        }
    }

    QList<CatalogObject> addedObjects;
    sortedAddedObjects.clear();

    int count = 0, good = 0;
    for (int i = 0; i < rows; ++i)
    {
        const QString &name = m_CatalogModel->item(i, NAME_COLUMN)->text();
        count++;
        auto o = getObject(name);
        if (o != nullptr)
        {
            good++;
            addedObjects.push_back(*o);
        }
        targets.push_back(name);
    }
    fprintf(stderr, "********** %d/%d targets found. %d unique test objects\n", good, count, newObjects.size());

    // First we add all the new objects that aren't already existing, and that can be found by KStars, to a
    // list. This is done so we can find distances to the nearest other one.
    if (!justCheckCurrentCatalog)
    {
        fprintf(stderr, "Adding: ");
        for (const auto &name : newObjects)
        {
            if (getObject(name) != nullptr)
            {
                fprintf(stderr, "0 %s ** EXISTS!\n", name.toLatin1().data());
                continue;
            }
            CatalogObject object;
            if (!getKStarsCatalogObject(name, &object))
            {
                fprintf(stderr, "0 %s ** COULD NOT FIND IT\n", name.toLatin1().data());
                continue;
            }
            object.setRA(object.ra0());
            object.setDec(object.dec0());
            if (name.compare(object.name(), Qt::CaseInsensitive) != 0)
            {
                fprintf(stderr, "%s had primary name %s -- reverting.\n",
                        name.toLatin1().data(), object.name().toLatin1().data());
                object.setName(name);
            }
            fprintf(stderr, "%s ", name.toLatin1().data());
            fflush(stderr);
            addedObjects.push_back(object);
        }
        fprintf(stderr, "\n--------------------------------------------------------\n");
    }

    // AddedObjects may actually contain the current catalog if justCheckCurrentCatalog is true.
    for (int i = 0; i < addedObjects.size(); ++i)
    {
        auto &object = addedObjects[i];
        double closest = 1e9;
        QString closestName;
        for (int j = 0; j < targets.size(); ++j)
        {
            if (justCheckCurrentCatalog && i == j)
                continue;
            auto name2 = targets[j];
            auto object2 = getObject(name2);
            if (object2 == nullptr)
            {
                fprintf(stderr, "********************************************************* O2 for targets[%d]: %s null!\n", j,
                        name2.toLatin1().data());
                break;
            }
            object2->setRA(object2->ra0());
            object2->setDec(object2->dec0());
            const dms dist = object.angularDistanceTo(object2);
            const double arcsecDist = dist.Degrees() * 3600.0;
            if (closest > arcsecDist)
            {
                closest = arcsecDist;
                closestName = name2;
            }
        }

        sortedAddedObjects.push_back(ObjectNeighbor(addedObjects[i], closest, closestName));
        // Also check the new objects -- not quite right see line below:
        // 702.8 c 7 ** OK (closest 703' ic 5148) (closestNew 0' IC 1459)

        if (justCheckCurrentCatalog)
        {
            fprintf(stderr, "%7.1f %-10s closest %s\n", closest / 60.0, object.name().toLatin1().data(),
                    closestName.toLatin1().data());
        }
        else
        {
            double closestNew = 1e9;
            QString closestNewName;
            for (int j = 0; j < addedObjects.size() - 1; ++j)
            {
                if (i == j) continue;
                auto object2 = addedObjects[j];
                object2.setRA(object2.ra0());
                object2.setDec(object2.dec0());
                const dms dist = object.angularDistanceTo(&object2);
                const double arcsecDist = dist.Degrees() * 3600.0;
                if (closestNew > arcsecDist)
                {
                    closestNew = arcsecDist;
                    closestNewName = object2.name();
                }
            }
            fprintf(stderr, "%7.1f %-10s (closest %s) (closestNew %5.0f' %-10s)\n",
                    closest / 60.0, object.name().toLatin1().data(), closestName.toLatin1().data(),
                    closestNew / 60, closestNewName.toLatin1().data());
            flags->add(SkyPoint(object.ra(), object.dec()), "J2000.0", "", QString("%1").arg(object.name()), Qt::yellow);
        }
    }
    std::sort(sortedAddedObjects.begin(), sortedAddedObjects.end(),
              [](const ObjectNeighbor & a, const ObjectNeighbor & b)
    {
        return a.distance > b.distance;
    });
    if (justCheckCurrentCatalog)
    {
        fprintf(stderr, "Sorted: ------------------------------------------\n");
        for (const auto &o : sortedAddedObjects)
            fprintf(stderr, "%7.1f %-10s closest %s\n",
                    o.distance / 60.0, o.object.name().toLatin1().data(),
                    o.neighbor.toLatin1().data());
    }
    fprintf(stderr, "DONE. ------------------------------------------\n");
}

// This is the top-level ImagingPlanning catalog directory.
QString ImagingPlanner::defaultDirectory() const
{
    return KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
           + QDir::separator() + "ImagingPlanner";
}

namespace
{

bool sortOldest(const QFileInfo &file1, const QFileInfo &file2)
{
    return file1.birthTime() < file2.birthTime();
}

QFileInfoList findDefaultDirectories()
{
    QString kstarsDir = KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir kDir(kstarsDir);
    kDir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList nameFilters;
    nameFilters << "ImagingPlanner*";
    QFileInfoList dirs1 = kDir.entryInfoList(nameFilters);

    QString ipDir = kstarsDir + QDir::separator() + "ImagingPlanner";
    QFileInfoList dirs2 = QDir(ipDir).entryInfoList(QStringList(), QDir::NoDotAndDotDot | QDir::AllDirs);

    dirs1.append(dirs2);
    std::sort(dirs1.begin(), dirs1.end(), sortOldest);
    return dirs1;
}
}  // namespace

// The default catalog is one loaded by the "Data -> Download New Data..." menu.
// Search the default directory for a ImagingPlanner subdirectory
QString ImagingPlanner::findDefaultCatalog() const
{
    QFileInfoList subDirs = findDefaultDirectories();
    for( const auto &dd : subDirs)
    {
        QDir subDir(dd.absoluteFilePath());
        const QStringList csvFilter({"*.csv"});
        const QFileInfoList files = subDir.entryInfoList(csvFilter, QDir::NoDotAndDotDot | QDir::Files);
        if (files.size() > 0)
        {
            QString firstFile;
            // Look through all the .csv files. Pick all.csv if it exists,
            // otherwise one of the other .csv files.
            for (const auto &file : files)
            {
                if (firstFile.isEmpty())
                    firstFile = file.absoluteFilePath();
                if (!file.baseName().compare("all", Qt::CaseInsensitive))
                    return file.absoluteFilePath();
            }
            if (!firstFile.isEmpty())
                return firstFile;
        }
    }
    return QString();
}

void ImagingPlanner::loadInitialCatalog()
{
    QString catalog = Options::imagingPlannerCatalogPath();
    if (catalog.isEmpty())
        catalog = findDefaultCatalog();
    if (catalog.isEmpty())
    {
        KSNotification::sorry(
            i18n("You need to load a catalog to start using this tool.\n"
                 "Use the Load Catalog button if you have one.\n"
                 "See Data -> Download New Data if not..."));
        setStatus(i18n("No Catalog!"));
    }
    else
        loadCatalog(catalog);
}

void ImagingPlanner::setStatus(const QString &message)
{
    ui->statusLabel->setText(message);
}

void ImagingPlanner::catalogLoaded()
{
    DPRINTF(stderr, "All catalogs loaded: %d of %d have catalog images\n", m_numWithImage, m_numWithImage + m_numMissingImage);
    // This cannot go in the threaded loadInitialCatalog()!
    loadFromDB();

    // TODO: At this point we'd read in various files (picked/imaged/deleted targets ...)
    // Can't do this in initialize() as we don't have columns yet.
    ui->CatalogView->setColumnHidden(FLAGS_COLUMN, true);
    ui->CatalogView->setColumnHidden(NOTES_COLUMN, true);

    m_CatalogSortModel->invalidate();
    ui->CatalogView->sortByColumn(HOURS_COLUMN, Qt::DescendingOrder);
    ui->CatalogView->resizeColumnsToContents();

    // Select the first row and give it the keyboard focus (so up/down keyboard keys work).
    auto index = ui->CatalogView->model()->index(0, 0);
    //ui->CatalogView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select |QItemSelectionModel::Current| QItemSelectionModel::Rows);
    ui->CatalogView->selectionModel()->select(index,
            QItemSelectionModel::Select | QItemSelectionModel::Current | QItemSelectionModel::Rows);
    ui->CatalogView->setFocus();
    updateDisplays();

    updateStatus();
    adjustWindowSize();
}

void ImagingPlanner::updateStatus()
{
    if (currentObjectName().isEmpty())
    {
        const int numDisplayedObjects = m_CatalogSortModel->rowCount();
        const int totalCatalogObjects = m_CatalogModel->rowCount();

        if (numDisplayedObjects > 0)
            setStatus(i18n("Select an object."));
        else if (totalCatalogObjects > 0)
            setStatus(i18n("Check Filters to unhide objects."));
        else
            setStatus(i18n("Load a Catalog."));
    }
    else
        setStatus("");
}

// This runs when the window gets a show event.
void ImagingPlanner::showEvent(QShowEvent * e)
{
    // ONLY run for first ever show
    if (m_initialShow == false)
    {
        m_initialShow = true;
        const int ht = height();
        resize(1000, ht);
        QWidget::showEvent(e);
    }
}

//FIXME: On close, we will need to close any open Details/AVT windows
void ImagingPlanner::slotClose()
{
}

// Reverse engineering of the Astrobin search URL (with permission from Salvatore).
// See https://github.com/astrobin/astrobin/blob/master/common/encoded_search_viewset.py#L15
QUrl ImagingPlanner::getAstrobinUrl(const QString &target, bool requireAwards, bool requireSomeFilters, double minRadius,
                                    double maxRadius)
{
    QString myQuery = QString("text={\"value\":\"%1\",\"matchType\":\"ALL\"}").arg(target);

    // This is a place where the actual date, not the date in the widget, is the right one to find.
    auto localTime = getGeo()->UTtoLT(KStarsData::Instance()->clock()->utc());
    QDate today = localTime.date();
    myQuery.append(QString("&date_acquired={\"min\":\"2018-01-01\",\"max\":\"%1\"}").arg(today.toString("yyyy-MM-dd")));

    if (requireAwards)
        myQuery.append(QString("&award=[\"iotd\",\"top-pick\",\"top-pick-nomination\"]"));

    if (requireSomeFilters)
        myQuery.append(QString("&filter_types={\"value\":[\"H_ALPHA\",\"SII\",\"OIII\",\"R\",\"G\",\"B\"],\"matchType\":\"ANY\"}"));

    if ((minRadius > 0 || maxRadius > 0) && (maxRadius > minRadius))
        myQuery.append(QString("&field_radius={\"min\":%1,\"max\":%2}").arg(minRadius).arg(maxRadius));

    QByteArray b(myQuery.toLatin1().data());

    // See quick pack implmentation in anonymous namespace above.
    QByteArray packed = pack(b);

    QByteArray compressed = qCompress(packed).remove(0, 4);

    QByteArray b64 = compressed.toBase64();

    replaceByteArrayChars(b64, '+', QByteArray("%2B"));
    replaceByteArrayChars(b64, '=', QByteArray("%3D"));
    replaceByteArrayChars(b, '"', QByteArray("%22"));
    replaceByteArrayChars(b, ':', QByteArray("%3A"));
    replaceByteArrayChars(b, '[', QByteArray("%5B"));
    replaceByteArrayChars(b, ']', QByteArray("%5D"));
    replaceByteArrayChars(b, ',', QByteArray("%2C"));
    replaceByteArrayChars(b, '\'', QByteArray("%27"));
    replaceByteArrayChars(b, '{', QByteArray("%7B"));
    replaceByteArrayChars(b, '}', QByteArray("%7D"));

    QString url = QString("https://app.astrobin.com/search?p=%1").arg(b64.toStdString().c_str());
    return QUrl(url);
}

void ImagingPlanner::popupAstrobin(const QString &target)
{
    QString newStr = replaceSpaceWith(target, "-");
    if (newStr.isEmpty()) return;

    const QUrl url = getAstrobinUrl(newStr, Options::astrobinAward(), false, Options::astrobinMinRadius(),
                                    Options::astrobinMaxRadius());
    if (!url.isEmpty())
        QDesktopServices::openUrl(url);
}

// Returns true if the url will result in a successful get.
// Times out after 3 seconds.
// Used for the special search button because some of the object
// web pages for vdb don't exist.
bool ImagingPlanner::checkIfPageExists(const QString &urlString)
{
    if (urlString.isEmpty())
        return false;

    QUrl url(urlString);
    QNetworkRequest request(url);
    QNetworkReply *reply = m_networkManager.get(request);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(3000); // 3 seconds timeout

    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start();
    loop.exec();
    timer.stop();

    if (reply->error() == QNetworkReply::NoError)
    {
        reply->deleteLater();
        return true;
    }
    else if (timer.isActive() )
    {
        reply->deleteLater();
        return false;
    }
    else
    {
        reply->deleteLater();
        return false;
    }
}

// Changes the label and tooltop on the searchSpecialWebPages buttons,
// depending on the current object, whose name is passed in.
void ImagingPlanner::adjustSpecialWebPageButton(const QString &name)
{
    QString catalog, toolTip, label;
    if (name.startsWith("ngc", Qt::CaseInsensitive))
    {
        catalog = "ngc";
        toolTip = i18n("Search the Professor Seligman online site for NGC images.");
    }
    else if (name.startsWith("ic", Qt::CaseInsensitive))
    {
        catalog = "ic";
        toolTip = i18n("Search the Professor Seligman online site for information about IC objects..");
    }
    else if (name.startsWith("sh2", Qt::CaseInsensitive))
    {
        catalog = "sh2";
        label = "Sharpless";
        toolTip = i18n("Search the galaxymap.org online site for information about Sharpless2 objects.");
    }
    else if (name.startsWith("m", Qt::CaseInsensitive))
    {
        catalog = "m";
        label = "Messier";
        toolTip = i18n("Search Nasa's online site for information about Messier objects..");
    }
    else if (name.startsWith("vdb", Qt::CaseInsensitive))
    {
        catalog = "vdb";
        toolTip = i18n("Search Emil Ivanov's online site for information about VDB objects.");
    }
    if (!catalog.isEmpty())
    {
        const QString numberPart = name.mid(catalog.size()).trimmed();
        if (!label.isEmpty()) catalog = label;
        bool ok;
        const int num = numberPart.toInt(&ok);
        Q_UNUSED(num);
        if (ok)
        {
            ui->searchSpecialWebPageImages->setText(catalog);
            ui->searchSpecialWebPageImages2->setText(catalog);
            ui->searchSpecialWebPageImages->setEnabled(true);
            ui->searchSpecialWebPageImages2->setEnabled(true);
            ui->searchSpecialWebPageImages->setToolTip(toolTip);
            ui->searchSpecialWebPageImages2->setToolTip(toolTip);
            return;
        }
    }
    ui->searchSpecialWebPageImages->setText("");
    ui->searchSpecialWebPageImages2->setText("");
    ui->searchSpecialWebPageImages->setEnabled(false);
    ui->searchSpecialWebPageImages2->setEnabled(false);
    ui->searchSpecialWebPageImages->setToolTip("");
    ui->searchSpecialWebPageImages2->setToolTip("");

}

void ImagingPlanner::searchSpecialWebPageImages()
{
    focusOnTable();
    const QString objectName = currentObjectName();
    QString urlString;
    bool ok;
    if (objectName.startsWith("ngc", Qt::CaseInsensitive))
    {
        const QString numberPart = objectName.mid(3).trimmed();
        const int num = numberPart.toInt(&ok);
        if (ok)
            urlString = QString("https://cseligman.com/text/atlas/ngc%1%2.htm#%3")
                        .arg(num / 100).arg(num % 100 < 50 ? "" : "a").arg(num);
    }
    else if (objectName.startsWith("ic", Qt::CaseInsensitive))
    {
        const QString numberPart = objectName.mid(2).trimmed();
        const int num = numberPart.toInt(&ok);
        if (ok)
            urlString = QString("https://cseligman.com/text/atlas/ic%1%2.htm#ic%3")
                        .arg(num / 100).arg(num % 100 < 50 ? "" : "a").arg(num);
    }
    else if (objectName.startsWith("sh2", Qt::CaseInsensitive))
    {
        const QString numberPart = objectName.mid(3).trimmed();
        const int num = numberPart.toInt(&ok);
        if (ok)
            urlString = QString("http://galaxymap.org/cat/view/sharpless/%1").arg(num);
    }
    else if (objectName.startsWith("m", Qt::CaseInsensitive))
    {
        const QString numberPart = objectName.mid(1).trimmed();
        const int num = numberPart.toInt(&ok);
        if (ok)
            urlString = QString("https://science.nasa.gov/mission/hubble/science/"
                                "explore-the-night-sky/hubble-messier-catalog/messier-%1").arg(num);
    }
    else if (objectName.startsWith("vdb", Qt::CaseInsensitive))
    {
        const QString numberPart = objectName.mid(3).trimmed();
        const int num = numberPart.toInt(&ok);
        if (ok)
        {
            urlString = QString("https://www.irida-observatory.org/CCD/VdB%1/VdB%1.html").arg(num);
            if (!checkIfPageExists(urlString))
                urlString = "https://www.emilivanov.com/CCD%20Images/Catalog_VdB.htm";
        }

    }
    if (!urlString.isEmpty())
        QDesktopServices::openUrl(QUrl(urlString));
}
void ImagingPlanner::searchSimbad()
{
    focusOnTable();
    QString name = currentObjectName();

    int abellNumber = 0;
    bool abellPlanetary = isAbellPlanetary(name, &abellNumber);
    if (abellPlanetary)
        name = QString("PN A66 %1").arg(abellNumber);
    else if (name.startsWith("sh2", Qt::CaseInsensitive))
        name.replace(QRegularExpression("sh2\\s*"), "sh2-");
    else if (name.startsWith("hickson", Qt::CaseInsensitive))
    {
        name.replace(0, 7, "HCG");
        name.replace(' ', "");
    }
    else
        name.replace(' ', "");

    QString urlStr = QString("https://simbad.cds.unistra.fr/simbad/sim-id?Ident=%1&NbIdent=1"
                             "&Radius=20&Radius.unit=arcmin&submit=submit+id").arg(name);
    QDesktopServices::openUrl(QUrl(urlStr));
}


// Crude massaging to conform to wikipedia standards
void ImagingPlanner::searchWikipedia()
{
    focusOnTable();
    QString wikipediaAddress = "https://en.wikipedia.org";
    QString name = currentObjectName();
    if (name.isEmpty())
    {
        DPRINTF(stderr, "NULL object sent to Wikipedia.\n");
        return;
    }

    if (name.startsWith("m ", Qt::CaseInsensitive))
        name = QString("Messier_%1").arg(name.mid(2, -1));
    QString urlStr = QString("%1/w/index.php?search=%2").arg(wikipediaAddress).arg(replaceSpaceWith(name, "_"));
    QDesktopServices::openUrl(QUrl(urlStr));
    return;
}

void ImagingPlanner::searchAstrobin()
{
    focusOnTable();
    QString name = currentObjectName();
    if (name.isEmpty())
        return;
    popupAstrobin(name);
}

bool ImagingPlanner::eventFilter(QObject * obj, QEvent * event)
{
    if (m_loadingCatalog)
        return false;

    // Load the catalog on tool startup.
    if (m_InitialLoad && event->type() == QEvent::Paint)
    {
        m_InitialLoad = false;
        setStatus(i18n("Loading Catalogs..."));
        QTimer::singleShot(100, this, &ImagingPlanner::loadInitialCatalog);
        return false;
    }

    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);

    if ((obj == ui->helpButton) &&
            (mouseEvent->button() == Qt::LeftButton) &&
            (event->type() == QEvent::MouseButtonRelease) &&
            (mouseEvent->modifiers() & Qt::KeyboardModifier::ShiftModifier) &&
            (mouseEvent->modifiers() & Qt::KeyboardModifier::ControlModifier) &&
            (mouseEvent->modifiers() & Qt::KeyboardModifier::AltModifier))
    {
        // Development code to help browse new catalog entries.
        // "Secret" keyboard modifies hijack the help button to reveal/hide the development buttons.
        ui->DevelFrame->setVisible(!ui->DevelFrame->isVisible());
        return false;
    }
    else if (obj == ui->helpButton && (event->type() == QEvent::MouseButtonRelease))
    {
        // Standard use of the help button.
        getHelp();
        return false;
    }

    // Right click on object in catalog view brings up this menu.
    else if ((obj == ui->CatalogView->viewport()) &&
             (event->type() == QEvent::MouseButtonRelease) &&
             (mouseEvent->button() == Qt::RightButton))
    {
        int numImaged = 0, numNotImaged = 0, numPicked = 0, numNotPicked = 0, numIgnored = 0, numNotIgnored = 0;
        QStringList selectedNames;
        for (const auto &r : ui->CatalogView->selectionModel()->selectedRows())
        {
            selectedNames.append(r.siblingAtColumn(0).data().toString());
            bool isPicked = getFlag(r, PICKED_BIT, ui->CatalogView->model());
            if (isPicked) numPicked++;
            else numNotPicked++;
            bool isImaged = getFlag(r, IMAGED_BIT, ui->CatalogView->model());
            if (isImaged) numImaged++;
            else numNotImaged++;
            bool isIgnored = getFlag(r, IGNORED_BIT, ui->CatalogView->model());
            if (isIgnored) numIgnored++;
            else numNotIgnored++;
        }

        if (selectedNames.size() == 0)
            return false;

        if (!m_PopupMenu)
            m_PopupMenu = new ImagingPlannerPopup;

        const bool imaged = numImaged > 0;
        const bool picked = numPicked > 0;
        const bool ignored = numIgnored > 0;
        m_PopupMenu->init(this, selectedNames,
                          (numImaged > 0 && numNotImaged > 0) ? nullptr : &imaged,
                          (numPicked > 0 && numNotPicked > 0) ? nullptr : &picked,
                          (numIgnored > 0 && numNotIgnored > 0) ? nullptr : &ignored);
        QPoint pos(mouseEvent->globalX(), mouseEvent->globalY());
        m_PopupMenu->popup(pos);
    }

    else if (obj == ui->userNotesEdit && event->type() == QEvent::FocusOut)
        userNotesEditFinished();

    else if (obj == ui->keywordEdit && event->type() == QEvent::FocusOut)
        keywordEditFinished();

    else if (obj == ui->keywordEdit && (event->type() == QEvent::KeyPress))
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        auto key = keyEvent->key();
        switch(key)
        {
            case Qt::Key_Enter:
            case Qt::Key_Tab:
            case Qt::Key_Return:
                keywordEditFinished();
                ui->keywordEdit->clearFocus();
                break;
            default:
                ;
        }
    }

    else if (obj == ui->SearchText && (event->type() == QEvent::KeyPress))
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        auto key = keyEvent->key();
        switch(key)
        {
            case Qt::Key_Enter:
            case Qt::Key_Tab:
            case Qt::Key_Return:
                searchSlot();
                break;
            default:
                ;
        }
    }

    else if ((obj == ui->ImagePreview ||
              obj == ui->ImagePreviewCredit ||
              obj == ui->ImagePreviewCreditLink) &&
             event->type() == QEvent::MouseButtonPress)
    {
        if (!ui->ImagePreviewCreditLink->text().isEmpty())
        {
            QUrl url(ui->ImagePreviewCreditLink->text());
            QDesktopServices::openUrl(url);
        }
    }

    return false;
}

void ImagingPlanner::keywordEditFinished()
{
    QString kwd = ui->keywordEdit->toPlainText().trimmed();
    ui->keywordEdit->clear();
    ui->keywordEdit->setText(kwd);
    if (m_Keyword != kwd)
    {
        m_Keyword = kwd;
        Options::setImagingPlannerKeyword(kwd);
        Options::self()->save();
        updateSortConstraints();
        m_CatalogSortModel->invalidate();
        ui->CatalogView->resizeColumnsToContents();
        updateDisplays();
    }

}
void ImagingPlanner::setDefaultImage()
{
    ui->ImagePreview->setPixmap(m_NoImagePixmap);
    ui->ImagePreview->update();
    ui->ImagePreviewCredit->setText("");
    ui->ImagePreviewCreditLink->setText("");
}

void ImagingPlanner::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    if (m_loadingCatalog)
        return;

    Q_UNUSED(deselected);
    if (selected.indexes().size() == 0)
    {
        disableUserNotes();
        return;
    }

    initUserNotes();
    updateStatus();
    auto selection = selected.indexes()[0];
    QString name = selection.data().toString();
    CatalogObject *object = getObject(name);
    if (object == nullptr)
        return;

    // This assumes current object and current selection are the same.
    // Could pass in "selected" if necessary.
    updateDisplays();

    ui->ImagePreviewCredit->setText("");
    ui->ImagePreviewCreditLink->setText("");
    // clear the image too?

    CatalogImageInfo catalogImageInfo;
    if (findCatalogImageInfo(name, &catalogImageInfo))
    {
        QString filename = catalogImageInfo.m_Filename;
        if (!filename.isEmpty() && !Options::imagingPlannerCatalogPath().isEmpty())
        {
            QString imageFullPath = filename;
            if (QFileInfo(filename).isRelative())
            {
                QString catDir = QFileInfo(Options::imagingPlannerCatalogPath()).absolutePath();
                imageFullPath = QString("%1%2%3").arg(catDir)
                                .arg(QDir::separator()).arg(filename);
            }
            if (!QFile(imageFullPath).exists())
                DPRINTF(stderr, "Image for \"%s\" -- \"%s\" doesn't exist\n",
                        name.toLatin1().data(), imageFullPath.toLatin1().data());

            ui->ImagePreview->setPixmap(QPixmap::fromImage(QImage(imageFullPath)));
            if (!catalogImageInfo.m_Link.isEmpty())
            {
                ui->ImagePreviewCreditLink->setText(catalogImageInfo.m_Link);
                ui->ImagePreview->setToolTip("Click to see original");
                ui->ImagePreviewCreditLink->setToolTip("Click to see original");
            }
            else
            {
                ui->ImagePreviewCreditLink->setText("");
                ui->ImagePreview->setToolTip("");
                ui->ImagePreviewCreditLink->setToolTip("");
            }

            if (!catalogImageInfo.m_Author.isEmpty() && !catalogImageInfo.m_License.isEmpty())
            {
                ui->ImagePreviewCredit->setText(
                    QString("Credit: %1 (with license %2)").arg(catalogImageInfo.m_Author)
                    .arg(creativeCommonsString(catalogImageInfo.m_License)));
                ui->ImagePreviewCredit->setToolTip(
                    QString("Original image license: %1")
                    .arg(creativeCommonsTooltipString(catalogImageInfo.m_License)));
            }
            else if (!catalogImageInfo.m_Author.isEmpty())
            {
                ui->ImagePreviewCredit->setText(
                    QString("Credit: %1").arg(catalogImageInfo.m_Author));
                ui->ImagePreviewCredit->setToolTip("");
            }
            else if (!catalogImageInfo.m_License.isEmpty())
            {
                ui->ImagePreviewCredit->setText(
                    QString("(license %1)").arg(creativeCommonsString(catalogImageInfo.m_License)));
                ui->ImagePreviewCredit->setToolTip(
                    QString("Original image license: %1")
                    .arg(creativeCommonsTooltipString(catalogImageInfo.m_License)));
            }
            else
            {
                ui->ImagePreviewCredit->setText("");
                ui->ImagePreviewCredit->setToolTip("");
            }
        }
    }
    else
    {
        object->load_image();
        auto image = object->image();
        if (!image.first)
        {
            // As a backup, see if the image is stored elsewhere...
            // I've seen many images stored in ~/.local/share/kstars/ZZ/ZZ-name.png,
            // e.g. kstars/thumb_ngc/thumb_ngc-m1.png
            const QString foundFilename = findObjectImage(name);
            if (!name.isEmpty())
            {
                constexpr int thumbHeight = 300, thumbWidth = 400;
                const QImage img = QImage(foundFilename);
                const bool scale = img.width() > thumbWidth || img.height() > thumbHeight;
                if (scale)
                    ui->ImagePreview->setPixmap(
                        QPixmap::fromImage(img.scaled(thumbWidth, thumbHeight, Qt::KeepAspectRatio)));
                else
                    ui->ImagePreview->setPixmap(QPixmap::fromImage(img));
            }
            else
                setDefaultImage();
        }
        else
            ui->ImagePreview->setPixmap(QPixmap::fromImage(image.second));
    }
    adjustSpecialWebPageButton(currentObjectName());
}

void ImagingPlanner::updateDisplays()
{
    updateCounts();

    // If nothing is selected, then select the first thing.
    if (!currentCatalogObject())
    {
        if (ui->CatalogView->model()->rowCount() > 0)
        {
            auto index = ui->CatalogView->model()->index(0, 0);
            ui->CatalogView->selectionModel()->select(index,
                    QItemSelectionModel::Select | QItemSelectionModel::Current | QItemSelectionModel::Rows);
        }
    }

    auto object = currentCatalogObject();
    if (object)
    {
        updateDetails(*object, currentObjectFlags());
        updateNotes(currentObjectNotes());
        plotAltitudeGraph(getDate(), object->ra0(), object->dec0());
        centerOnSkymap();
    }
    updateStatus();
    focusOnTable();
}

void ImagingPlanner::updateDetails(const CatalogObject &object, int flags)
{
    ui->infoObjectName->setText(object.name());
    ui->infoSize->setText(QString("%1' x %2'").arg(object.a(), 0, 'f', 1).arg(object.b(), 0, 'f', 1));

    QPalette palette = ui->infoObjectLongName->palette();
    //palette.setColor(ui->infoObjectLongName->backgroundRole(), Qt::darkGray);
    palette.setColor(ui->infoObjectLongName->foregroundRole(), Qt::darkGray);
    ui->infoObjectLongName->setPalette(palette);
    if (object.longname().isEmpty() || (object.longname() == object.name()))
        ui->infoObjectLongName->clear();
    else
        ui->infoObjectLongName->setText(QString("(%1)").arg(object.longname()));

    ui->infoObjectType->setText(SkyObject::typeName(object.type()));

    auto noon = KStarsDateTime(getDate(), QTime(12, 0, 0));
    QTime riseTime = object.riseSetTime(noon, getGeo(), true);
    QTime setTime = object.riseSetTime(noon, getGeo(), false);
    QTime transitTime = object.transitTime(noon, getGeo());
    dms transitAltitude = object.transitAltitude(noon, getGeo());

    QString moonString;
    KSMoon *moon = getMoon();
    if (moon)
    {
        const double separation = ui->CatalogView->selectionModel()->currentIndex()
                                  .siblingAtColumn(MOON_COLUMN).data(MOON_ROLE).toDouble();
        // The unicode character is the angle sign.
        if (separation >= 0)
            moonString = QString("%1 \u2220 %3º").arg(i18n("Moon")).arg(separation, 0, 'f', 1);
    }

    QString riseSetString;
    if (!riseTime.isValid() && !setTime.isValid() && transitTime.isValid())
        riseSetString = QString("%1 %2 @ %3º")
                        .arg(i18n("Transits"))
                        .arg(transitTime.toString("h:mm"))
                        .arg(transitAltitude.Degrees(), 0, 'f', 1);
    else if (!riseTime.isValid() && setTime.isValid() && !transitTime.isValid())
        riseSetString = QString("%1 %2")
                        .arg(i18n("Sets at"))
                        .arg(setTime.toString("h:mm"));
    else if (!riseTime.isValid() && setTime.isValid() && transitTime.isValid())
        riseSetString = QString("%1 %2 %3 %4 @ %5º")
                        .arg(i18n("Sets at"))
                        .arg(setTime.toString("h:mm"))
                        .arg(i18n("Transit"))
                        .arg(transitTime.toString("h:mm"))
                        .arg(transitAltitude.Degrees(), 0, 'f', 1);
    else if (riseTime.isValid() && !setTime.isValid() && !transitTime.isValid())
        riseSetString = QString("%1 %2")
                        .arg(i18n("Rises at"))
                        .arg(riseTime.toString("h:mm"));
    else if (riseTime.isValid() && !setTime.isValid() && transitTime.isValid())
        riseSetString = QString("%1 %2 %3 %4 @ %5º")
                        .arg(i18n("Rises at"))
                        .arg(riseTime.toString("h:mm"))
                        .arg(i18n("Transit"))
                        .arg(transitTime.toString("h:mm"))
                        .arg(transitAltitude.Degrees(), 0, 'f', 1);
    else if (riseTime.isValid() && setTime.isValid() && !transitTime.isValid())
        riseSetString = QString("%1 %2 %3 %4")
                        .arg(i18n("Rises"))
                        .arg(riseTime.toString("h:mm"))
                        .arg(i18n("Sets"))
                        .arg(setTime.toString("h:mm"));
    else if (riseTime.isValid() && setTime.isValid() && transitTime.isValid())
        riseSetString = QString("%1 %2 %3 %4 %5 %6 @ %7º")
                        .arg(i18n("Rises"))
                        .arg(riseTime.toString("h:mm"))
                        .arg(i18n("Sets"))
                        .arg(setTime.toString("h:mm"))
                        .arg(i18n("Transit"))
                        .arg(transitTime.toString("h:mm"))
                        .arg(transitAltitude.Degrees(), 0, 'f', 1);
    if (moonString.size() > 0)
        riseSetString.append(QString(", %1").arg(moonString));
    ui->infoRiseSet->setText(riseSetString);

    palette = ui->infoObjectFlags->palette();
    palette.setColor(ui->infoObjectFlags->foregroundRole(), Qt::darkGray);
    ui->infoObjectFlags->setPalette(palette);
    ui->infoObjectFlags->setText(flagString(flags));
}

// TODO: This code needs to be shared with the scheduler somehow.
// Right now 2 very similar copies at the end of scheduler.cpp and here.
//
// Clearly below I had timezone issues. The problem was running this code using a timezone
// that was not the local timezone of the machine. E.g. setting KStars to australia
// when I'm in california.
void ImagingPlanner::plotAltitudeGraph(const QDate &date, const dms &ra, const dms &dec)
{
    auto altitudeGraph = ui->altitudeGraph;
    altitudeGraph->setAltitudeAxis(-20.0, 90.0);
    //altitudeGraph->axis(KPlotWidget::TopAxis)->setVisible(false);

    QVector<QDateTime> jobStartTimes, jobEndTimes;
    getRunTimes(date, *getGeo(), ui->minAltitude->value(), ui->minMoon->value(), ui->maxMoonAltitude->value(), ra, dec,
                ui->useArtificialHorizon->isChecked(),
                &jobStartTimes, &jobEndTimes);

    auto tz = QTimeZone(getGeo()->TZ() * 3600);
    KStarsDateTime midnight = KStarsDateTime(date.addDays(1), QTime(0, 1));
    midnight.setTimeZone(tz);

    KStarsDateTime ut  = getGeo()->LTtoUT(KStarsDateTime(midnight));
    KSAlmanac ksal(ut, getGeo());
    QDateTime dawn = midnight.addSecs(24 * 3600 * ksal.getDawnAstronomicalTwilight());
    dawn.setTimeZone(tz);
    QDateTime dusk = midnight.addSecs(24 * 3600 * ksal.getDuskAstronomicalTwilight());
    dusk.setTimeZone(tz);

    Ekos::SchedulerJob job;
    setupJob(job, "temp", ui->minAltitude->value(), ui->minMoon->value(), ui->maxMoonAltitude->value(), ra, dec,
             ui->useArtificialHorizon->isChecked());

    QVector<double> times, alts;
    QDateTime plotStart = dusk;
    plotStart.setTimeZone(tz);


    // Start the plot 1 hour before dusk and end it an hour after dawn.
    plotStart = plotStart.addSecs(-1 * 3600);
    auto t = plotStart;
    t.setTimeZone(tz);
    auto plotEnd = dawn.addSecs(1 * 3600);
    plotEnd.setTimeZone(tz);

    while (t.secsTo(plotEnd) > 0)
    {
        SkyPoint coords = job.getTargetCoords();
        double alt = getAltitude(getGeo(), coords, t);
        alts.push_back(alt);
        double hour = midnight.secsTo(t) / 3600.0;
        times.push_back(hour);
        t = t.addSecs(60 * 10);
    }

    altitudeGraph->plot(getGeo(), &ksal, times, alts);

    for (int i = 0; i < jobStartTimes.size(); ++i)
    {
        auto startTime = jobStartTimes[i];
        auto stopTime = jobEndTimes[i];
        if (startTime < plotStart) startTime = plotStart;
        if (stopTime > plotEnd) stopTime = plotEnd;

        startTime.setTimeZone(tz);
        stopTime.setTimeZone(tz);

        QVector<double> runTimes, runAlts;
        auto t = startTime;
        t.setTimeZone(tz);
        //t.setTimeZone(jobStartTimes[0].timeZone());

        while (t.secsTo(stopTime) > 0)
        {
            SkyPoint coords = job.getTargetCoords();
            double alt = getAltitude(getGeo(), coords, t);
            runAlts.push_back(alt);
            double hour = midnight.secsTo(t) / 3600.0;
            runTimes.push_back(hour);
            t = t.addSecs(60 * 10);
        }
        altitudeGraph->plotOverlay(runTimes, runAlts);
    }
}

void ImagingPlanner::updateCounts()
{
    const int numDisplayedObjects = m_CatalogSortModel->rowCount();
    const int totalCatalogObjects = m_CatalogModel->rowCount();
    if (numDisplayedObjects == 1)
        ui->tableCount->setText(QString("1/%1 %2").arg(totalCatalogObjects).arg(i18n("object")));
    else
        ui->tableCount->setText(QString("%1/%2 %3").arg(numDisplayedObjects).arg(totalCatalogObjects).arg(i18n("objects")));
}

void ImagingPlanner::moveBackOneDay()
{
    // Try to keep the object.
    QString selection = currentObjectName();
    ui->DateEdit->setDate(ui->DateEdit->date().addDays(-1));
    // Don't need to call recompute(), called by dateChanged callback.
    updateDisplays();
    updateMoon();
    scrollToName(selection);
}

void ImagingPlanner::moveForwardOneDay()
{
    QString selection = currentObjectName();
    ui->DateEdit->setDate(ui->DateEdit->date().addDays(1));
    // Don't need to call recompute(), called by dateChanged callback.
    updateDisplays();
    updateMoon();
    scrollToName(selection);
}

QString ImagingPlanner::currentObjectName() const
{
    QString name = ui->CatalogView->selectionModel()->currentIndex().siblingAtColumn(NAME_COLUMN).data(
                       Qt::DisplayRole).toString();
    return name;
}

CatalogObject *ImagingPlanner::currentCatalogObject()
{
    QString name = currentObjectName();
    return getObject(name);
}

//FIXME: This will open multiple Detail windows for each object;
//Should have one window whose target object changes with selection
void ImagingPlanner::objectDetails()
{
    CatalogObject *current = currentCatalogObject();
    if (current == nullptr)
        return;
    auto ut = KStarsData::Instance()->ut();
    ut.setDate(getDate());
    QPointer<DetailDialog> dd =
        new DetailDialog(current, ut, getGeo(), KStars::Instance());
    dd->exec();
    delete dd;
}

void ImagingPlanner::centerOnSkymap()
{
    if (!Options::imagingPlannerCenterOnSkyMap())
        return;
    reallyCenterOnSkymap();
}

void ImagingPlanner::reallyCenterOnSkymap()
{
    CatalogObject *current = currentCatalogObject();
    if (current == nullptr)
        return;

    // These shouldn't happen anymore--seemed to happen when I let in null objects.
    if (current->ra().Degrees() == 0 && current->dec().Degrees() == 0)
    {
        DPRINTF(stderr, "found a 0,0 object\n");
        return;
    }

    // Set up the Alt/Az coordinates that SkyMap needs.
    KStarsDateTime time = KStarsData::Instance()->clock()->utc();
    dms lst = getGeo()->GSTtoLST(time.gst());
    current->EquatorialToHorizontal(&lst, getGeo()->lat());


    // Doing this to avoid the pop-up warning that an object is below the ground.
    bool keepGround = Options::showGround();
    bool keepAnimatedSlew = Options::useAnimatedSlewing();
    Options::setShowGround(false);
    Options::setUseAnimatedSlewing(false);

    SkyMap::Instance()->setClickedObject(current);
    SkyMap::Instance()->setClickedPoint(current);
    SkyMap::Instance()->slotCenter();

    Options::setShowGround(keepGround);
    Options::setUseAnimatedSlewing(keepAnimatedSlew);
}

void ImagingPlanner::setSelection(int flag, bool enabled)
{
    auto rows = ui->CatalogView->selectionModel()->selectedRows();

    // We can't use the selection for processing, because it may change on the fly
    // as we modify flags (e.g. if the view is set to showing picked objects only
    // and we are disabling the picked flag, as a selected object with a picked flag
    // gets de-picked, it will also get deselected.
    // So, we store a list of the source model indeces, and operate on the source model.

    // Find the source model indeces.
    QList<QModelIndex> sourceIndeces;
    for (int i = 0; i < rows.size(); ++i)
    {
        auto proxyIndex  = rows[i].siblingAtColumn(FLAGS_COLUMN);
        auto sourceIndex = m_CatalogSortModel->mapToSource(proxyIndex);
        sourceIndeces.append(sourceIndex);
    }

    for (int i = 0; i < sourceIndeces.size(); ++i)
    {
        auto &sourceIndex = sourceIndeces[i];

        // Set or clear the flags using the source model.
        if (enabled)
            setFlag(sourceIndex, flag, m_CatalogModel.data());
        else
            clearFlag(sourceIndex, flag, m_CatalogModel.data());

        QString name = m_CatalogModel->data(sourceIndex.siblingAtColumn(NAME_COLUMN)).toString();
        int flags = m_CatalogModel->data(sourceIndex.siblingAtColumn(FLAGS_COLUMN), FLAGS_ROLE).toInt();
        QString notes = m_CatalogModel->data(sourceIndex.siblingAtColumn(NOTES_COLUMN), NOTES_ROLE).toString();
        saveToDB(name, flags, notes);

        if (flag == IMAGED_BIT)
            highlightImagedObject(sourceIndex, enabled);
        if (flag == PICKED_BIT)
            highlightPickedObject(sourceIndex, enabled);
    }
    updateDisplays();
}

void ImagingPlanner::highlightImagedObject(const QModelIndex &index, bool imaged)
{
    // TODO: Ugly, for now. Figure out how to use the color schemes the right way.
    QColor m_DefaultCellBackground(36, 35, 35);
    QColor m_ImagedObjectBackground(10, 65, 10);
    QString themeName = KSTheme::Manager::instance()->currentThemeName().toLatin1().data();
    if (themeName == "High Key" || themeName == "Default" || themeName == "White Balance")
    {
        m_DefaultCellBackground = QColor(240, 240, 240);
        m_ImagedObjectBackground = QColor(180, 240, 180);
    }
    for (int col = 0; col < LAST_COLUMN; ++col)
    {
        auto colIndex = index.siblingAtColumn(col);
        m_CatalogModel->setData(colIndex, imaged ? m_ImagedObjectBackground : m_DefaultCellBackground, Qt::BackgroundRole);
    }
}

void ImagingPlanner::highlightPickedObject(const QModelIndex &index, bool picked)
{
    for (int col = 0; col < LAST_COLUMN; ++col)
    {
        auto colIndex = index.siblingAtColumn(col);
        auto font = m_CatalogModel->data(colIndex, Qt::FontRole);
        auto ff = qvariant_cast<QFont>(font);
        ff.setBold(picked);
        ff.setItalic(picked);
        ff.setUnderline(picked);
        font = ff;
        m_CatalogModel->setData(colIndex, font, Qt::FontRole);
    }
}

void ImagingPlanner::setSelectionPicked()
{
    setSelection(PICKED_BIT, true);
}

void ImagingPlanner::setSelectionNotPicked()
{
    setSelection(PICKED_BIT, false);
}

void ImagingPlanner::setSelectionImaged()
{
    setSelection(IMAGED_BIT, true);
}

void ImagingPlanner::setSelectionNotImaged()
{
    setSelection(IMAGED_BIT, false);
}

void ImagingPlanner::setSelectionIgnored()
{
    setSelection(IGNORED_BIT, true);
}

void ImagingPlanner::setSelectionNotIgnored()
{
    setSelection(IGNORED_BIT, false);
}

int ImagingPlanner::currentObjectFlags()
{
    auto index = ui->CatalogView->selectionModel()->currentIndex().siblingAtColumn(FLAGS_COLUMN);
    const bool hasFlags = ui->CatalogView->model()->data(index, FLAGS_ROLE).canConvert<int>();
    if (!hasFlags)
        return 0;
    return ui->CatalogView->model()->data(index, FLAGS_ROLE).toInt();
}

QString ImagingPlanner::currentObjectNotes()
{
    auto index = ui->CatalogView->selectionModel()->currentIndex().siblingAtColumn(NOTES_COLUMN);
    const bool hasNotes = ui->CatalogView->model()->data(index, NOTES_ROLE).canConvert<QString>();
    if (!hasNotes)
        return QString();
    return ui->CatalogView->model()->data(index, NOTES_ROLE).toString();
}

void ImagingPlanner::setCurrentObjectNotes(const QString &notes)
{
    auto index = ui->CatalogView->selectionModel()->currentIndex();
    if (!index.isValid())
        return;
    auto sibling = index.siblingAtColumn(NOTES_COLUMN);

    auto sourceIndex = m_CatalogSortModel->mapToSource(sibling);
    QVariant n(notes);
    m_CatalogModel->setData(sourceIndex, n, NOTES_ROLE);
}

ImagingPlannerPopup::ImagingPlannerPopup() : QMenu(nullptr)
{
}

// The bools are pointers to we can have a 3-valued input parameter.
// If the pointer is a nullptr, then we say, for example it is neigher imaged, not not imaged.
// That is, really, some of the selection are imaged and some not imaged.
// If the pointer does point to a bool, then the value of that bool tells you if all the selection
// is (e.g.) imaged, or if all of it is not imaged.
void ImagingPlannerPopup::init(ImagingPlanner * planner, const QStringList &names,
                               const bool * imaged, const bool * picked, const bool * ignored)
{
    clear();
    if (names.size() == 0) return;

    QString title;
    if (names.size() == 1)
        title = names[0];
    else if (names.size() <= 3)
    {
        title = names[0];
        for (int i = 1; i < names.size(); i++)
            title.append(QString(", %1").arg(names[i]));
    }
    else
        title = i18n("%1, %2 and %3 other objects", names[0], names[1], names.size() - 2);

    addSection(title);

    QString word = names.size() == 1 ? names[0] : i18n("objects");

    if (imaged == nullptr)
    {
        addAction(i18n("Mark %1 as NOT imaged", word), planner, &ImagingPlanner::setSelectionNotImaged);
        addAction(i18n("Mark %1 as already imaged", word), planner, &ImagingPlanner::setSelectionImaged);
    }
    else if (*imaged)
        addAction(i18n("Mark %1 as NOT imaged", word), planner, &ImagingPlanner::setSelectionNotImaged);
    else
        addAction(i18n("Mark %1 as already imaged", word), planner, &ImagingPlanner::setSelectionImaged);

    if (picked == nullptr)
    {
        addAction(i18n("Un-pick %1", word), planner, &ImagingPlanner::setSelectionNotPicked);
        addAction(i18n("Pick %1", word), planner, &ImagingPlanner::setSelectionPicked);
    }
    else if (*picked)
        addAction(i18n("Un-pick %1", word), planner, &ImagingPlanner::setSelectionNotPicked);
    else
        addAction(i18n("Pick %1", word), planner, &ImagingPlanner::setSelectionPicked);


    if (ignored == nullptr)
    {
        addAction(i18n("Stop ignoring %1", word), planner, &ImagingPlanner::setSelectionNotIgnored);
        addAction(i18n("Ignore %1", word), planner, &ImagingPlanner::setSelectionIgnored);

    }
    else if (*ignored)
        addAction(i18n("Stop ignoring %1", word), planner, &ImagingPlanner::setSelectionNotIgnored);
    else
        addAction(i18n("Ignore %1", word), planner, &ImagingPlanner::setSelectionIgnored);

    addSeparator();
    addAction(i18n("Center %1 on SkyMap", names[0]), planner, &ImagingPlanner::reallyCenterOnSkymap);
    addSeparator();
    addAction(i18n("Screenshot some image of %1, plate-solve it, and temporarily place it on the SkyMap", names[0]), planner,
              &ImagingPlanner::takeScreenshot);

}

ImagingPlannerDBEntry::ImagingPlannerDBEntry(const QString &name, bool picked, bool imaged,
        bool ignored, const QString &notes) : m_Name(name), m_Notes(notes)
{
    setFlags(picked, imaged, ignored);
}

ImagingPlannerDBEntry::ImagingPlannerDBEntry(const QString &name, int flags, const QString &notes)
    : m_Name(name), m_Flags(flags), m_Notes(notes)
{
}

void ImagingPlannerDBEntry::getFlags(bool * picked, bool * imaged, bool * ignored)
{
    *picked = m_Flags & PickedBit;
    *imaged = m_Flags & ImagedBit;
    *ignored = m_Flags & IgnoredBit;
}


void ImagingPlannerDBEntry::setFlags(bool picked, bool imaged, bool ignored)
{
    m_Flags = 0;
    if (picked) m_Flags |= PickedBit;
    if (imaged) m_Flags |= ImagedBit;
    if (ignored) m_Flags |= IgnoredBit;
}

void ImagingPlanner::saveToDB(const QString &name, bool picked, bool imaged,
                              bool ignored, const QString &notes)
{
    ImagingPlannerDBEntry e(name, 0, notes);
    e.setFlags(picked, imaged, ignored);
    KStarsData::Instance()->userdb()->AddImagingPlannerEntry(e);
}

void ImagingPlanner::saveToDB(const QString &name, int flags, const QString &notes)
{
    ImagingPlannerDBEntry e(name, flags, notes);
    KStarsData::Instance()->userdb()->AddImagingPlannerEntry(e);
}

// KSUserDB::GetAllImagingPlannerEntries(QList<ImagingPlannerDBEntry> *entryList)
void ImagingPlanner::loadFromDB()
{
    // Disconnect the filter from the model, or else we'll re-filter numRows squared times.
    // Not as big a deal here because we're not touching all rows, just the rows with flags/notes.
    // Also see the reconnect below.
    m_CatalogSortModel->setSourceModel(nullptr);

    auto tz = QTimeZone(getGeo()->TZ() * 3600);
    KStarsDateTime midnight = KStarsDateTime(getDate().addDays(1), QTime(0, 1));
    KStarsDateTime ut  = getGeo()->LTtoUT(KStarsDateTime(midnight));
    KSAlmanac ksal(ut, getGeo());

    QList<ImagingPlannerDBEntry> list;
    KStarsData::Instance()->userdb()->GetAllImagingPlannerEntries(&list);
    QHash<QString, ImagingPlannerDBEntry> dbData;
    QHash<QString, int> dbNotes;
    for (const auto &entry : list)
    {
        dbData[entry.m_Name] = entry;
    }

    int rows = m_CatalogModel->rowCount();
    for (int i = 0; i < rows; ++i)
    {
        const QString &name = m_CatalogModel->item(i, NAME_COLUMN)->text();
        auto entry = dbData.find(name);
        if (entry != dbData.end())
        {
            QVariant f = entry->m_Flags;
            m_CatalogModel->item(i, FLAGS_COLUMN)->setData(f, FLAGS_ROLE);
            if (entry->m_Flags & IMAGED_BIT)
                highlightImagedObject(m_CatalogModel->index(i, NAME_COLUMN), true);
            if (entry->m_Flags & PICKED_BIT)
                highlightPickedObject(m_CatalogModel->index(i, NAME_COLUMN), true);
            QVariant n = entry->m_Notes;
            m_CatalogModel->item(i, NOTES_COLUMN)->setData(n, NOTES_ROLE);
        }
    }
    // See above. Reconnect the filter to the model.
    m_CatalogSortModel->setSourceModel(m_CatalogModel.data());
}

void ImagingPlanner::loadImagedFile()
{
    if (m_loadingCatalog)
        return;

    focusOnTable();
    QString fileName = QFileDialog::getOpenFileName(this,
                       tr("Open Already-Imaged File"), QDir::homePath(), tr("Any files (*)"));
    if (fileName.isEmpty())
        return;
    QFile inputFile(fileName);
    if (inputFile.open(QIODevice::ReadOnly))
    {
        int numSuccess = 0;
        QStringList failedNames;
        QTextStream in(&inputFile);
        while (!in.atEnd())
        {
            QString name = in.readLine().trimmed();
            if (name.isEmpty() || name.startsWith('#'))
                continue;
            name = tweakNames(name);
            if (getObject(name))
            {
                numSuccess++;
                auto startIndex = m_CatalogModel->index(0, NAME_COLUMN);
                QVariant value(name);
                auto matches = m_CatalogModel->match(startIndex, Qt::DisplayRole, value, 1, Qt::MatchFixedString);
                if (matches.size() > 0)
                {
                    setFlag(matches[0], IMAGED_BIT, m_CatalogModel);
                    highlightImagedObject(matches[0], true);

                    // Make sure we save it to the DB.
                    QString name = m_CatalogModel->data(matches[0].siblingAtColumn(NAME_COLUMN)).toString();
                    int flags = m_CatalogModel->data(matches[0].siblingAtColumn(FLAGS_COLUMN), FLAGS_ROLE).toInt();
                    QString notes = m_CatalogModel->data(matches[0].siblingAtColumn(NOTES_COLUMN), NOTES_ROLE).toString();
                    saveToDB(name, flags, notes);
                }
                else
                {
                    DPRINTF(stderr, "ooops! internal inconsitency--got an object but match didn't work");
                }
            }
            else
                failedNames.append(name);
        }
        inputFile.close();
        if (failedNames.size() == 0)
        {
            if (numSuccess > 0)
                KSNotification::info(i18n("Successfully marked %1 objects as read", numSuccess));
            else
                KSNotification::sorry(i18n("Empty file"));
        }
        else
        {
            int num = std::min((int)failedNames.size(), 10);
            QString sample = QString("\"%1\"").arg(failedNames[0]);
            for (int i = 1; i < num; ++i)
                sample.append(QString(" \"%1\"").arg(failedNames[i]));
            if (numSuccess == 0 && failedNames.size() <= 10)
                KSNotification::sorry(i18n("Failed marking all of these objects imaged: %1", sample));
            else if (numSuccess == 0)
                KSNotification::sorry(i18n("Failed marking %1 objects imaged, including: %2", failedNames.size(), sample));
            else if (numSuccess > 0 && failedNames.size() <= 10)
                KSNotification::sorry(i18n("Succeeded marking %1 objects imaged. Failed with %2: %3",
                                           numSuccess, failedNames.size() == 1 ? "this" : "these", sample));
            else
                KSNotification::sorry(i18n("Succeeded marking %1 objects imaged. Failed with %2 including these: %3",
                                           numSuccess, failedNames.size(), sample));
        }
    }
    else
    {
        KSNotification::sorry(i18n("Sorry, couldn't open file: \"%1\"", fileName));
    }
}

void ImagingPlanner::addCatalogImageInfo(const CatalogImageInfo &info)
{
    m_CatalogImageInfoMap[info.m_Name.toLower()] = info;
}

bool ImagingPlanner::findCatalogImageInfo(const QString &name, CatalogImageInfo * info)
{
    auto result = m_CatalogImageInfoMap.find(name.toLower());
    if (result == m_CatalogImageInfoMap.end())
        return false;
    if (result->m_Filename.isEmpty())
        return false;
    *info = *result;
    return true;
}

void ImagingPlanner::loadCatalogViaMenu()
{
    QString startDir = Options::imagingPlannerCatalogPath();
    if (startDir.isEmpty())
        startDir = defaultDirectory();

    QString path = QFileDialog::getOpenFileName(this, tr("Open Catalog File"), startDir, tr("Any files (*.csv)"));
    if (path.isEmpty())
        return;

    loadCatalog(path);
}

void ImagingPlanner::loadCatalog(const QString &path)
{
    removeEventFilters();

    // This tool occassionally crashed when UI interactions happen during catalog loading.
    // Don't know why, but disabling that, and re-enabling after load below.
    setEnabled(false);
    setFixedSize(this->width(), this->height());

    m_loadingCatalog = true;
    loadCatalogFromFile(path);
    catalogLoaded();

    // Re-enable UI
    setEnabled(true);
    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    m_loadingCatalog = false;
    installEventFilters();

    // Select and display the first row
    if (m_CatalogSortModel->rowCount() > 0)
    {
        auto name = m_CatalogSortModel->index(0, 0).data().toString();
        scrollToName(name);
        QItemSelection selection, deselection;
        selection.select(m_CatalogSortModel->index(0, 0), m_CatalogSortModel->index(0, 0));
        selectionChanged(selection, deselection);
    }
}

CatalogImageInfo::CatalogImageInfo(const QString &csv)
{
    QString line = csv.trimmed();
    if (line.isEmpty() || line.startsWith('#'))
        return;
    QStringList columns = line.split(",");
    if (columns.size() < 1 || columns[0].isEmpty())
        return;
    int column = 0;
    m_Name     = columns[column++];
    if (columns.size() <= column) return;
    m_Filename = columns[column++];
    if (columns.size() <= column) return;
    m_Author   = columns[column++];
    if (columns.size() <= column) return;
    m_Link     = columns[column++];
    if (columns.size() <= column) return;
    m_License  = columns[column++];
}

// This does the following:
// - Clears the internal catalog
// - Initializes the m_CatalogImageInfoMap, which goes from name to image.
// - Loads in new objects into the internal catalog.
//
// CSV File Columns:
// 1: ID:             M 1
// 2: Image Filename: M_1.jpg
// 3: Author:         Hy Murveit
// 4: Link:           https://www.astrobin.com/x3utgw/F/
// 5: License:        ACC (possibilities are ACC,ANCCC,ASACC,ANCCC,ANCSACC)
//                    last one is Attribution Non-Commercial hare-Alike Creative Commons
// Currently ID is mandatory, if there is an image filename, then Author,Link,and License
//   are also required, though could be blank.
// - Comment lines start with #
// - Can include another catalog with "LoadCatalog FILENAME"
// - Can request loading a provided KStars DSO catalog with "LoadDSOCatalog FILENAME"
// - Can request removing a KStars DSO catalog given its ID (presumably an old version).
void ImagingPlanner::loadCatalogFromFile(QString path, bool reset)
{
    QFile inputFile(path);
    if (reset)
    {
        m_numWithImage = 0;
        m_numMissingImage = 0;
    }
    int numMissingImage = 0, numWithImage = 0;
    if (!inputFile.exists())
    {
        emit popupSorry(i18n("Sorry, catalog file doesn't exist: \"%1\"", path));
        return;
    }
    QStringList objectNames;
    if (inputFile.open(QIODevice::ReadOnly))
    {
        const auto tz = QTimeZone(getGeo()->TZ() * 3600);
        const KStarsDateTime midnight = KStarsDateTime(getDate().addDays(1), QTime(0, 1));
        const KStarsDateTime ut  = getGeo()->LTtoUT(KStarsDateTime(midnight));
        const KSAlmanac ksal(ut, getGeo());

        if (reset)
        {
            Options::setImagingPlannerCatalogPath(path);
            Options::self()->save();
            if (m_CatalogModel->rowCount() > 0)
                m_CatalogModel->removeRows(0, m_CatalogModel->rowCount());
            clearObjects();
        }
        QTextStream in(&inputFile);
        while (!in.atEnd())
        {
            CatalogImageInfo info(in.readLine().trimmed());
            const QString name = info.m_Name;
            if (name.isEmpty())
                continue;
            if (name.startsWith("LoadCatalog"))
            {
                // This line isn't a normal entry, but rather points to another catalog.
                // Load that catalog and then skip this line.
                QRegularExpression re("^LoadCatalog\\s+(\\S+)", QRegularExpression::CaseInsensitiveOption);
                const auto match = re.match(name);
                if (match.hasMatch())
                {
                    const QString catFilename = match.captured(1);
                    if (catFilename.isEmpty()) continue;
                    const QFileInfo fInfo(catFilename);

                    QString catFullPath = catFilename;
                    if (!fInfo.isAbsolute())
                    {
                        const QString catDir = QFileInfo(path).absolutePath();
                        catFullPath = QString("%1%2%3").arg(catDir)
                                      .arg(QDir::separator()).arg(match.captured(1));
                    }
                    if (catFullPath != path)
                        loadCatalogFromFile(catFullPath, false);
                }
                continue;
            }
            if (name.startsWith("LoadDSOCatalog"))
            {
                // This line isn't a normal entry, but rather points to a DSO catalog
                // (that is, a standard KStars sky-object catalog)
                // which may be helpful to avoid a lot fetching of coordinates from online sources.
                QRegularExpression re("^LoadDSOCatalog\\s+(\\S+)", QRegularExpression::CaseInsensitiveOption);
                const auto match = re.match(name);
                if (match.hasMatch())
                {
                    const QString catFilename = match.captured(1);
                    if (catFilename.isEmpty()) continue;
                    const QFileInfo fInfo(catFilename);

                    QString catFullPath = catFilename;
                    if (!fInfo.isAbsolute())
                    {
                        const QString catDir = QFileInfo(path).absolutePath();
                        catFullPath = QString("%1%2%3").arg(catDir)
                                      .arg(QDir::separator()).arg(match.captured(1));
                    }
                    std::pair<bool, QString> out = m_manager.import_catalog(catFullPath, false);
                    DPRINTF(stderr, "Load of KStars catalog %s %s%s\n", catFullPath.toLatin1().data(),
                            out.first ? "succeeded." : "failed: ", out.second.toLatin1().data());
                }
                continue;
            }
            if (name.startsWith("RemoveDSOCatalog"))
            {
                // This line isn't a normal entry, but rather points to an ID of an old DSO catalog
                // which presumably a current DSO Catalog replaces.
                QRegularExpression re("^RemoveDSOCatalog\\s+(\\S+)", QRegularExpression::CaseInsensitiveOption);
                const auto match = re.match(name);
                if (match.hasMatch())
                {
                    const QString catIDstr = match.captured(1);
                    if (catIDstr.isEmpty()) continue;

                    bool ok;
                    const int catID = catIDstr.toInt(&ok);
                    if (ok && m_manager.catalog_exists(catID))
                    {
                        const std::pair<bool, QString> out = m_manager.remove_catalog(catID);
                        DPRINTF(stderr, "Removal of out-of-date catalog %d %s%s\n", catID,
                                out.first ? "succeeded." : "failed: ", out.second.toLatin1().data());
                    }
                }
                continue;
            }
            objectNames.append(name);
            if (!info.m_Filename.isEmpty())
            {
                numWithImage++;
                QFileInfo fInfo(info.m_Filename);
                if (fInfo.isRelative())
                    info.m_Filename = QString("%1%2%3").arg(QFileInfo(path).absolutePath())
                                      .arg(QDir::separator()).arg(info.m_Filename);
                addCatalogImageInfo(info);
            }
            else
            {
                numMissingImage++;
                DPRINTF(stderr, "No catalog image for %s\n", name.toLatin1().data());
            }
            QCoreApplication::processEvents();
        }
        inputFile.close();

        int num = 0, numBad = 0, iteration = 0;
        // Move to threaded thing??
        for (const auto &name : objectNames)
        {
            setStatus(i18n("%1/%2: Adding %3", ++iteration, objectNames.size(), name));
            if (addCatalogItem(ksal, name, 0)) num++;
            else
            {
                DPRINTF(stderr, "Couldn't add %s\n", name.toLatin1().data());
                numBad++;
            }
        }
        m_numWithImage += numWithImage;
        m_numMissingImage += numMissingImage;
        DPRINTF(stderr, "Catalog %s: %d of %d have catalog images\n",
                path.toLatin1().data(), numWithImage, numWithImage + numMissingImage);
    }
    else
    {
        emit popupSorry(i18n("Sorry, couldn't open file: \"%1\"", path));
    }
}

void ImagingPlanner::sorry(const QString &message)
{
    KSNotification::sorry(message);
}

void ImagingPlanner::captureRegion(const QImage &screenshot)
{
    if (m_PlateSolve.get()) disconnect(m_PlateSolve.get());

    // This code to convert the screenshot to a FITSData is, of course, convoluted.
    // TODO: improve it.
    m_ScreenShotImage = screenshot;
    QString temporaryPath = QDir(KSPaths::writableLocation(QStandardPaths::TempLocation) + QDir::separator() +
                                 qAppName()).path();
    QString tempQImage = QDir(temporaryPath).filePath("screenshot.png");
    m_ScreenShotImage.save(tempQImage);
    FITSData::ImageToFITS(tempQImage, "png", m_ScreenShotFilename);
    this->raise();
    this->activateWindow();

    if (!m_PlateSolve.get())
        m_PlateSolve.reset(new PlateSolve(this));

    m_PlateSolve->setImageDisplay(m_ScreenShotImage);
    if (currentCatalogObject())
    {
        m_PlateSolve->setPosition(*currentCatalogObject());
        m_PlateSolve->setUsePosition(true);
        m_PlateSolve->setUseScale(false);
        m_PlateSolve->setLinear(false);
        reallyCenterOnSkymap();
    }

    m_PlateSolve->setWindowTitle(QString("Plate Solve for %1").arg(currentObjectName()));
    m_PlateSolve->show();
    if (Options::imagingPlannerStartSolvingImmediately())
        extractImage();
}

void ImagingPlanner::takeScreenshot()
{
    if (!currentCatalogObject())
        return;

    const QString messageID = "ImagingPlannerScreenShotInfo";
    const QString screenshotInfo =
        QString("<p><b>Taking a screenshot of %1 for the SkyMap</b></p>"
                "<p>This allows you to screenshot/copy a good example image of %1 from another application, "
                "such as a browser viewing %1 on Astrobin. It then plate-solves that screenshot and overlays "
                "it temporarily on the SkyMap.</p>"
                "<p>You can use this to help you frame your future %1 capture. "
                "The SkyMap overlay will only be visible in the current KStars session.</p>"
                "<p>In order to do this, you should make the image you wish to copy visible "
                "on your screen now, before clicking OK. After you click OK you will see the mouse pointer change "
                "to the screenshot pointer. You then drag your mouse over the part of the %1 image "
                "you wish to copy. If you check do-not-ask-again, then you must make sure that your desired image "
                "is already visible before you run this.</p>"
                "<p>After you take your screenshot, the system will bring up a menu to help plate-solve the image. "
                "Click SOLVE on that menu to start the process, unless it is automatically started. "
                "Once successfully plate-solved, your image will be overlayed onto the SkyMap.").arg(currentObjectName());
#if KIO_VERSION >= QT_VERSION_CHECK(5, 100, 0)
    if (KMessageBox::questionTwoActions(KStars::Instance(),
                                        screenshotInfo,
                                        "ScreenShot",
                                        KGuiItem(i18nc("@action:button", "OK")),
                                        KGuiItem(i18nc("@action:button", "Cancel")),
                                        messageID
                                       )
            == KMessageBox::SecondaryAction)
#else
    const int result = KMessageBox::questionYesNo(this, screenshotInfo, "ScreenShot",
                       KGuiItem("OK"), KGuiItem("Cancel"), messageID);
    if (result != KMessageBox::Yes)
#endif
    {
        // Don't remember the cancel response.
        KMessageBox::enableMessage(messageID);
        disconnect(m_CaptureWidget.get());
        m_CaptureWidget.reset();
        this->raise();
        this->activateWindow();
        return;
    }

    if (m_CaptureWidget.get()) disconnect(m_CaptureWidget.get());
    m_CaptureWidget.reset(new ScreenCapture());
    QObject::connect(m_CaptureWidget.get(), &ScreenCapture::areaSelected,
                     this, &ImagingPlanner::captureRegion, Qt::UniqueConnection);
    disconnect(m_CaptureWidget.get(), &ScreenCapture::aborted, nullptr, nullptr);
    QObject::connect(m_CaptureWidget.get(), &ScreenCapture::aborted, this, [this]()
    {
        disconnect(m_CaptureWidget.get());
        m_CaptureWidget.reset();
        this->raise();
        this->activateWindow();
    });
    m_CaptureWidget->show();
}

void ImagingPlanner::extractImage()
{
    disconnect(m_PlateSolve.get(), &PlateSolve::solverFailed, nullptr, nullptr);
    connect(m_PlateSolve.get(), &PlateSolve::solverFailed, this, [this]()
    {
        disconnect(m_PlateSolve.get());
    });
    disconnect(m_PlateSolve.get(), &PlateSolve::solverSuccess, nullptr, nullptr);
    connect(m_PlateSolve.get(), &PlateSolve::solverSuccess, this, [this]()
    {
        disconnect(m_PlateSolve.get());
        const FITSImage::Solution &solution = m_PlateSolve->solution();
        ImageOverlay overlay;
        overlay.m_Orientation = solution.orientation;
        overlay.m_RA = solution.ra;
        overlay.m_DEC = solution.dec;
        overlay.m_ArcsecPerPixel = solution.pixscale;
        overlay.m_EastToTheRight = solution.parity;
        overlay.m_Status = ImageOverlay::AVAILABLE;

        const bool mirror = !solution.parity;
        const int scaleWidth = std::min(m_ScreenShotImage.width(), Options::imageOverlayMaxDimension());
        QImage *processedImg = new QImage;
        if (mirror)
            *processedImg = m_ScreenShotImage.mirrored(true, false).scaledToWidth(scaleWidth); // It's reflected horizontally.
        else
            *processedImg = m_ScreenShotImage.scaledToWidth(scaleWidth);
        overlay.m_Img.reset(processedImg);
        overlay.m_Width = processedImg->width();
        overlay.m_Height = processedImg->height();
        KStarsData::Instance()->skyComposite()->imageOverlay()->show();
        KStarsData::Instance()->skyComposite()->imageOverlay()->addTemporaryImageOverlay(overlay);
        centerOnSkymap();
        KStars::Instance()->activateWindow();
        KStars::Instance()->raise();
        m_PlateSolve->close();
    });
    m_PlateSolve->solveImage(m_ScreenShotFilename);
}
