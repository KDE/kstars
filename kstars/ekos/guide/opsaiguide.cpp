/*
    SPDX-FileCopyrightText: 2026 Pavan <pk6122004@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opsaiguide.h"
#include "guide.h"
#include "kstarsdata.h"
#include "kspaths.h"
#include "skycomponents/skymapcomposite.h"
#include "skycomponents/artificialhorizoncomponent.h"
#include "indi/indimount.h"
#include "Options.h"
#include "internalguide/internalguider.h"
#include "internalguide/calibration.h"
#include "guideinterface.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QJsonDocument>
#include <algorithm>
#include <QDirIterator>
#include <QFileInfo>
#include <QVector>
#include <zlib.h>

namespace
{
// Minimal in-process ZIP (DEFLATE) writer built on the already-linked zlib — avoids pulling in KArchive.
class ZipWriter
{
    public:
        bool open(const QString &path)
        {
            m_file.setFileName(path);
            return m_file.open(QIODevice::WriteOnly);
        }

        // Recursively add every file under dir, stored as prefix/<relative path>. Returns count added.
        int addTree(const QString &dir, const QString &prefix)
        {
            if (!QDir(dir).exists())
                return 0;
            int n = 0;
            const QString baseDir = QDir(dir).absolutePath();
            QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext())
            {
                const QString fp = it.next();
                if (addOne(fp, prefix + "/" + QDir(baseDir).relativeFilePath(fp)))
                    ++n;
            }
            return n;
        }

        // Add a single file under archiveName. Returns 1 if added, 0 if unreadable.
        int addOne(const QString &path, const QString &archiveName)
        {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly))
                return 0;
            const QByteArray data = f.readAll();
            f.close();

            const QByteArray name = archiveName.toUtf8();
            const quint32 crc = static_cast<quint32>(crc32(0L, reinterpret_cast<const Bytef *>(data.constData()),
                                static_cast<uInt>(data.size())));
            const QByteArray comp = rawDeflate(data);
            const QDateTime mtime = QFileInfo(path).lastModified();

            Entry e;
            e.name = name;
            e.crc = crc;
            e.compSize = static_cast<quint32>(comp.size());
            e.rawSize = static_cast<quint32>(data.size());
            e.dosTime = toDosTime(mtime);
            e.dosDate = toDosDate(mtime);
            e.offset = static_cast<quint32>(m_file.pos());
            m_entries.append(e);

            QByteArray hdr;
            put32(hdr, 0x04034b50);                       // local file header signature
            put16(hdr, 20);                               // version needed
            put16(hdr, 0);                                // general purpose flags
            put16(hdr, 8);                                // compression method = deflate
            put16(hdr, e.dosTime);
            put16(hdr, e.dosDate);
            put32(hdr, e.crc);
            put32(hdr, e.compSize);
            put32(hdr, e.rawSize);
            put16(hdr, static_cast<quint16>(name.size()));
            put16(hdr, 0);                                // extra field length
            hdr.append(name);
            m_file.write(hdr);
            m_file.write(comp);
            return 1;
        }

        void finish()
        {
            const quint32 cdStart = static_cast<quint32>(m_file.pos());
            QByteArray cd;
            for (const Entry &e : m_entries)
            {
                put32(cd, 0x02014b50);                    // central directory header signature
                put16(cd, 20);                            // version made by
                put16(cd, 20);                            // version needed
                put16(cd, 0);                             // flags
                put16(cd, 8);                             // method
                put16(cd, e.dosTime);
                put16(cd, e.dosDate);
                put32(cd, e.crc);
                put32(cd, e.compSize);
                put32(cd, e.rawSize);
                put16(cd, static_cast<quint16>(e.name.size()));
                put16(cd, 0);                             // extra len
                put16(cd, 0);                             // comment len
                put16(cd, 0);                             // disk number start
                put16(cd, 0);                             // internal attrs
                put32(cd, 0);                             // external attrs
                put32(cd, e.offset);                      // local header offset
                cd.append(e.name);
            }
            m_file.write(cd);

            QByteArray eocd;
            put32(eocd, 0x06054b50);                      // end of central directory signature
            put16(eocd, 0);                               // this disk number
            put16(eocd, 0);                               // disk with central dir
            put16(eocd, static_cast<quint16>(m_entries.size()));
            put16(eocd, static_cast<quint16>(m_entries.size()));
            put32(eocd, static_cast<quint32>(cd.size()));
            put32(eocd, cdStart);
            put16(eocd, 0);                               // comment length
            m_file.write(eocd);
            m_file.close();
        }

    private:
        struct Entry
        {
            QByteArray name;
            quint32 crc = 0, compSize = 0, rawSize = 0, offset = 0;
            quint16 dosTime = 0, dosDate = 0;
        };

        static void put16(QByteArray &b, quint16 v)
        {
            b.append(char(v & 0xFF));
            b.append(char((v >> 8) & 0xFF));
        }
        static void put32(QByteArray &b, quint32 v)
        {
            b.append(char(v & 0xFF));
            b.append(char((v >> 8) & 0xFF));
            b.append(char((v >> 16) & 0xFF));
            b.append(char((v >> 24) & 0xFF));
        }
        static quint16 toDosTime(const QDateTime &dt)
        {
            const QTime t = dt.isValid() ? dt.time() : QTime(0, 0, 0);
            return static_cast<quint16>((t.hour() << 11) | (t.minute() << 5) | (t.second() / 2));
        }
        static quint16 toDosDate(const QDateTime &dt)
        {
            const QDate d = dt.isValid() ? dt.date() : QDate(1980, 1, 1);
            int year = d.year() - 1980;
            if (year < 0)
                year = 0;
            return static_cast<quint16>((year << 9) | (d.month() << 5) | d.day());
        }
        static QByteArray rawDeflate(const QByteArray &data)
        {
            z_stream s {};
            // Negative windowBits selects a raw DEFLATE stream (no zlib header/trailer), which is what
            // the ZIP format stores — plain compress2() would wrap it and corrupt the archive.
            if (deflateInit2(&s, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
                return QByteArray();
            QByteArray out;
            out.resize(static_cast<int>(deflateBound(&s, static_cast<uLong>(data.size()))));
            s.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
            s.avail_in = static_cast<uInt>(data.size());
            s.next_out = reinterpret_cast<Bytef *>(out.data());
            s.avail_out = static_cast<uInt>(out.size());
            deflate(&s, Z_FINISH);
            out.resize(out.size() - static_cast<int>(s.avail_out));
            deflateEnd(&s);
            return out;
        }

        QFile m_file;
        QVector<Entry> m_entries;
};
} // namespace

namespace Ekos
{

OpsAIGuide::OpsAIGuide(QWidget *parent) : QWizard(parent)
{
    setupUi(this);

    connect(stopButton, &QPushButton::clicked, this, &OpsAIGuide::slotStopProtocol);
    connect(exportOfflineButton, &QPushButton::clicked, this, &OpsAIGuide::slotExportOffline);
    connect(&m_ProtocolTimer, &QTimer::timeout, this, &OpsAIGuide::slotProcessProtocol);

    connect(trainEkosLiveButton, &QPushButton::clicked, this, [this]()
    {
        if (m_SysIdData["sessions"].toArray().isEmpty())
        {
            appendLog("No data to train! Please run the protocol first.");
            return;
        }
        trainEkosLiveButton->setEnabled(false);
        trainEkosLiveButton->setText("Uploading and Training...");
        appendLog("Uploading system-ID data to EkosLive Cloud for training...");
        emit trainInEkosLiveRequested(m_SysIdData);
    });

    // Wizard-wide "Export Logs" button (visible on every page) that bundles the AI/guide/terminal logs into a zip.
    setButtonText(QWizard::CustomButton1, "Export Logs");
    setOption(QWizard::HaveCustomButton1, true);
    connect(this, &QWizard::customButtonClicked, this, [this](int which)
    {
        if (which == QWizard::CustomButton1)
            slotExportLogs();
    });

    progressBar->setValue(0);
}

void OpsAIGuide::onTrainingResult(bool success, const QJsonObject &result)
{
    if (success)
    {
        trainEkosLiveButton->setText("Training Complete - Weights Loaded");
        appendLog("EkosLive training complete. Weights saved and set as the active AI weights.");
    }
    else
    {
        trainEkosLiveButton->setEnabled(true);
        trainEkosLiveButton->setText("Train in EkosLive");
        appendLog(QString("EkosLive training failed: %1").arg(result.value("message").toString("unknown error")));
    }
}

void OpsAIGuide::slotExportLogs()
{
    const QString base = KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString suggested = QDir::homePath() + "/ekos_ai_logs_" +
                              QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".zip";
    const QString out = QFileDialog::getSaveFileName(this, "Export Logs", suggested, "Zip Archives (*.zip)");
    if (out.isEmpty())
        return;

    ZipWriter zip;
    if (!zip.open(out))
    {
        appendLog("Failed to create archive: " + out);
        return;
    }

    int n = 0;
    n += zip.addTree(base + "/ai_debug_logs",    "ai_debug_logs");    // per-frame AI telemetry CSVs
    n += zip.addTree(base + "/ai_training_logs", "ai_training_logs"); // sysid captures
    n += zip.addTree(base + "/guidelogs",        "guidelogs");        // per-session guide logs
    n += zip.addTree(base + "/logs",             "logs");             // KStars console log (if Log to File enabled)
    n += zip.addOne(base + "/guide_log.txt",     "guide_log.txt");
    zip.finish();

    if (n > 0)
        appendLog(QString("Exported %1 log file(s) to: %2").arg(n).arg(out));
    else
        appendLog("No logs found. Enable 'Log to File' in KStars settings to also capture terminal logs.");
}

void OpsAIGuide::initializePage(int id)
{
    QWizard::initializePage(id);

    // Page 2 (0-indexed) is the "System Identification Progress" page
    // where the protocol actually starts executing
    if (id == 2)
    {
        slotStartProtocol();
    }
}

void OpsAIGuide::appendLog(const QString &message)
{
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss");
    logTextEdit->append(QString("[%1] %2").arg(timeStr, message));
    emit aiGuideLog(message);
}

void OpsAIGuide::enforceSettings()
{
    if (!m_SettingsEnforced)
    {
        m_OrigRAAlgorithm = Options::rAGuidePulseAlgorithm();
        m_OrigDECAlgorithm = Options::dECGuidePulseAlgorithm();
        m_OrigRAEnabled = Options::rAGuideEnabled();
        m_OrigDECEnabled = Options::dECGuideEnabled();
        m_OrigEastEnabled = Options::eastRAGuideEnabled();
        m_OrigWestEnabled = Options::westRAGuideEnabled();
        m_OrigNorthEnabled = Options::northDECGuideEnabled();
        m_OrigSouthEnabled = Options::southDECGuideEnabled();
        m_OrigMaxDeltaRMS = Options::guideMaxDeltaRMS();
        m_SettingsEnforced = true;
    }

    Options::setRAGuidePulseAlgorithm(0);
    Options::setDECGuidePulseAlgorithm(0);
    Options::setRAGuideEnabled(true);
    Options::setDECGuideEnabled(true);
    Options::setEastRAGuideEnabled(true);
    Options::setWestRAGuideEnabled(true);
    Options::setNorthDECGuideEnabled(true);
    Options::setSouthDECGuideEnabled(true);
    Options::setGuideMaxDeltaRMS(100.0); // Allow huge drifts during Free Drift without aborting

    appendLog("Enforced Standard guiding algorithm and all directions enabled for data collection.");
}

void OpsAIGuide::restoreSettings()
{
    // Nothing was enforced (e.g. wizard opened and closed without starting) — do not
    // clobber the user's live settings with default-constructed "originals".
    if (!m_SettingsEnforced)
        return;
    m_SettingsEnforced = false;

    Options::setRAGuidePulseAlgorithm(m_OrigRAAlgorithm);
    Options::setDECGuidePulseAlgorithm(m_OrigDECAlgorithm);
    Options::setRAGuideEnabled(m_OrigRAEnabled);
    Options::setDECGuideEnabled(m_OrigDECEnabled);
    Options::setEastRAGuideEnabled(m_OrigEastEnabled);
    Options::setWestRAGuideEnabled(m_OrigWestEnabled);
    Options::setNorthDECGuideEnabled(m_OrigNorthEnabled);
    Options::setSouthDECGuideEnabled(m_OrigSouthEnabled);
    Options::setGuideMaxDeltaRMS(m_OrigMaxDeltaRMS);

    appendLog("Restored original guide algorithm and direction settings.");
}

void OpsAIGuide::slotStartProtocol()
{
    progressBar->setValue(0);
    logTextEdit->clear();
    appendLog("Starting AI Guiding Assistant Protocol...");

    // Build the Queue based on Mount Type
    m_Phases.clear();
    QString mountStr = mountTypeCombo->currentText();
    QString mountTypeEnum;
    if (mountStr == "Worm Gear") mountTypeEnum = "WORM_GEAR";
    else if (mountStr == "Harmonic Drive") mountTypeEnum = "HARMONIC_DRIVE";
    else mountTypeEnum = "DIRECT_DRIVE";

    // Initialize the root JSON object for SysId Data
    m_SysIdData = QJsonObject();
    m_SysIdData["format_version"] = "1.0";
    QJsonObject equipment;
    equipment["mount_type"] = mountTypeEnum;
    equipment["mount_name"] = "Unknown"; // Can be populated dynamically later
    equipment["camera"] = "Unknown";
    equipment["focal_length_mm"] = 0;
    equipment["pixel_size_um"] = 0.0;
    equipment["pixel_scale_arcsec_per_px"] = 0.0;
    equipment["guide_exposure_ms"] = 0; // Usually dynamic
    equipment["guide_optics_type"] = "Unknown"; // Will be populated below
    auto *guide = qobject_cast<Guide *>(parentWidget());
    if (guide)
    {
        equipment["camera"] = guide->camera();
        equipment["focal_length_mm"] = guide->focalLength();
        equipment["pixel_size_um"] = guide->pixelSizeX();
        equipment["guide_exposure_ms"] = static_cast<int>(guide->exposure() * 1000.0);
        if (guide->mount())
            equipment["mount_name"] = guide->mount()->getDeviceName();
        if (guide->focalLength() > 0)
        {
            equipment["pixel_scale_arcsec_per_px"] = (206.265 * guide->pixelSizeX()) / guide->focalLength();

            // Heuristic for OAG vs Guidescope
            if (std::abs(guide->focalLength() - Options::telescopeFocalLength()) < 1.0)
            {
                equipment["guide_optics_type"] = "OAG";
            }
            else
            {
                equipment["guide_optics_type"] = "Guidescope";
            }
        }
    }
    m_SysIdData["equipment"] = equipment;

    QJsonObject fingerprint;
    fingerprint["guide_exposure_s"] = guide ? guide->exposure() : 0.0;
    fingerprint["guide_binning"] = Options::guideBinning();
    fingerprint["ra_proportional_gain"] = Options::rAProportionalGain();
    fingerprint["dec_proportional_gain"] = Options::dECProportionalGain();
    fingerprint["ra_integral_gain"] = Options::rAIntegralGain();
    fingerprint["dec_integral_gain"] = Options::dECIntegralGain();
    fingerprint["ra_min_pulse_arcsec"] = Options::rAMinimumPulseArcSec();
    fingerprint["dec_min_pulse_arcsec"] = Options::dECMinimumPulseArcSec();
    fingerprint["ra_max_pulse_arcsec"] = static_cast<int>(Options::rAMaximumPulseArcSec());
    fingerprint["dec_max_pulse_arcsec"] = static_cast<int>(Options::dECMaximumPulseArcSec());
    fingerprint["ra_hysteresis"] = Options::rAHysteresis();
    fingerprint["dec_hysteresis"] = Options::dECHysteresis();
    fingerprint["ra_pulse_algorithm"] = 0; // Forced
    fingerprint["dec_pulse_algorithm"] = 0; // Forced
    fingerprint["all_directions_enabled"] = true; // Forced
    m_SysIdData["model_fingerprint"] = fingerprint;

    m_SysIdData["sessions"] = QJsonArray();

    enforceSettings();

    // Generate a single filename for this entire run
    QString dirPath = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("ai_training_logs");
    QDir().mkpath(dirPath);
    m_LogFilename = dirPath + "/" + QString("sysid_data_%1.json").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

    if (mountStr == "Worm Gear")
    {

        // Position 1: Pier West, high altitude
        m_Phases.append({65.0, -45.0, 480, false, false, "", "", 0, 0, 0}); // 8min Standard Guiding
        m_Phases.append({65.0, -45.0, 600, true, false, "", "", 0, 0, 0});  // 10min Free Drift (captures 3 full 198s worm cycles)

        // Position 2: lower altitude
        m_Phases.append({40.0, -45.0, 480, false, false, "", "", 0, 0, 0}); // 8min Standard Guiding
        m_Phases.append({40.0, -45.0, 400, true, false, "", "", 0, 0, 0});  // ~6.7min Free Drift

        // Position 3: Pier East, high altitude
        m_Phases.append({65.0,  45.0, 480, false, false, "", "", 0, 0, 0}); // 8min Standard Guiding
        m_Phases.append({65.0,  45.0, 400, true, false, "", "", 0, 0, 0});  // ~6.7min Free Drift
    }
    else if (mountStr == "Harmonic Drive")
    {
        // Standard guiding at Position 1
        m_Phases.append({65.0, -45.0, 480, false, false, {}, {}, 0, 15, 30});  // 8min standard guiding
        // Position 1: Alt ~65°, 3 hours East of meridian to prevent flip
        m_Phases.append({65.0, -45.0, 120, true, false, {}, {}, 0, 15, 30});  // 2min Free Drift (PE detection)

        // Pulse response tests at Position 1 — RA axis
        m_Phases.append({65.0, -45.0, 0, false, true, "RA", "EAST",  50, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "RA", "EAST", 100, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "RA", "EAST", 200, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "RA", "WEST",  50, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "RA", "WEST", 100, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "RA", "WEST", 200, 15, 30});

        // Pulse response tests — DEC axis
        m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "NORTH",  50, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "NORTH", 100, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "NORTH", 200, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "SOUTH",  50, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "SOUTH", 100, 15, 30});
        m_Phases.append({65.0, -45.0, 0, false, true, "DEC", "SOUTH", 200, 15, 30});


        // Position 2: Alt ~45°
        m_Phases.append({45.0, -45.0, 120, true, false, {}, {}, 0, 15, 30});   // 2min free drift
        m_Phases.append({45.0, -45.0, 300, false, false, {}, {}, 0, 15, 30});  // 5min standard guiding
    }
    else
    {


        // Position 1: Alt 70, Az ~180 (meridian south) — parallactic angle near zero
        m_Phases.append({70.0,   0.0, 120, false, false, "", "", 0, 0, 0}); // 2min standard (calibrate)
        m_Phases.append({70.0,   0.0, 180, true, false, "", "", 0, 0, 0});  // 3min free drift

        // Position 2: Alt 50, Az ~120 (SE, ~2h east) — parallactic angle negative
        m_Phases.append({50.0, -60.0, 120, false, false, "", "", 0, 0, 0}); // 2min standard
        m_Phases.append({50.0, -60.0, 180, true, false, "", "", 0, 0, 0});  // 3min free drift

        // Position 3: Alt 35, Az ~240 (SW, ~2h west) — parallactic angle positive (opposite sign to pos 2)
        m_Phases.append({35.0,  60.0, 120, false, false, "", "", 0, 0, 0}); // 2min standard
        m_Phases.append({35.0,  60.0, 180, true, false, "", "", 0, 0, 0});  // 3min free drift
    }

    appendLog(QString("Loaded %1 phases for %2").arg(m_Phases.size()).arg(mountStr));

    m_TotalPhases = m_Phases.size();
    progressBar->setValue(0);
    exportOfflineButton->setEnabled(false);

    m_State = STATE_PRECHECK;
    m_ProtocolTimer.start(1000); // 1Hz tick
}

void OpsAIGuide::slotExportOffline()
{
    if (m_LogFilename.isEmpty() || !QFile::exists(m_LogFilename))
    {
        appendLog("No data to export! Please run the protocol first.");
        return;
    }

    QString savePath = QFileDialog::getSaveFileName(this, "Export Offline Training Data", QDir::homePath() + "/sysid_data.json",
                       "JSON Files (*.json)");
    if (!savePath.isEmpty())
    {
        if (QFile::copy(m_LogFilename, savePath))
        {
            appendLog(QString("Successfully exported data to: %1").arg(savePath));
        }
        else
        {
            appendLog("Failed to export data!");
        }
    }
}

void OpsAIGuide::slotStopProtocol()
{
    m_ProtocolTimer.stop();
    auto *guide = qobject_cast<Guide*>(parentWidget());
    if (guide)
    {
        guide->setAIFreeDrift(false);
        guide->abort();
        disconnect(guide, &Guide::guideStats, this, &OpsAIGuide::slotOnGuideStats);
    }
    restoreSettings();
    m_State = STATE_DONE;
    statusLabel->setText("Protocol Aborted by User");
    appendLog("User requested protocol stop. Idle.");
}

void OpsAIGuide::done(int result)
{
    // Called by QWizard/QDialog for every dismissal path — Finish (accept), Cancel/Esc/
    // window-close (reject). Without this the protocol timer keeps ticking on a hidden
    // dialog (still driving the mount) and the guider is left in Free-Drift, silently
    // suppressing every subsequent guide pulse.
    const bool wasRunning = (m_State != STATE_IDLE && m_State != STATE_DONE && m_State != STATE_ERROR);

    m_ProtocolTimer.stop();

    auto *guide = qobject_cast<Guide *>(parentWidget());
    if (guide)
    {
        guide->setAIFreeDrift(false);
        disconnect(guide, &Guide::guideStats, this, &OpsAIGuide::slotOnGuideStats);
        if (wasRunning)
            guide->abort();
    }

    // No-op if settings were never enforced.
    restoreSettings();

    QWizard::done(result);
}

void OpsAIGuide::slotProcessProtocol()
{
    switch (m_State)
    {
        case STATE_IDLE:
        case STATE_DONE:
        case STATE_ERROR:
            m_ProtocolTimer.stop();
            break;

        case STATE_PRECHECK:
        {
            auto *guide = qobject_cast<Guide*>(parentWidget());
            if (!guide || !guide->mount() || !guide->mount()->isConnected())
            {
                appendLog("ERROR: Mount is not connected!");
                m_State = STATE_ERROR;
                break;
            }
            if (guide->mount()->status() == ISD::Mount::MOUNT_PARKED)
            {
                appendLog("Mount is parked. Unparking before scan...");
                guide->mount()->unpark();
                break; // Wait in PRECHECK until unpark is complete
            }
            if (guide->mount()->status() != ISD::Mount::MOUNT_TRACKING && guide->mount()->status() != ISD::Mount::MOUNT_IDLE)
            {
                // Wait until mount is idle or tracking
                break;
            }
            if (m_Phases.isEmpty())
            {
                appendLog("All phases complete!");
                restoreSettings();
                m_State = STATE_DONE;
                progressBar->setValue(100);
                exportOfflineButton->setEnabled(true);
                emit aiGuideComplete();
                this->next();
                break;
            }

            // Check if the next phase is a pulse response at the same position
            if (m_Phases.first().pulseResponse)
            {
                m_State = STATE_PULSE_RESPONSE_INIT;
                break;
            }

            m_State = STATE_HORIZON_SCAN;
            break;
        }

        case STATE_HORIZON_SCAN:
        {
            double targetAlt = m_Phases.first().targetAlt;
            appendLog(QString("Phase started: Scanning Artificial Horizon for safe slew to Alt %1°...").arg(targetAlt));
            statusLabel->setText("Calculating safe slew target...");

            // 1. Define target Altitude and starting Azimuth based on Phase offset from local meridian
            double meridianAz = KStarsData::Instance()->geo()->lat()->Degrees() > 0 ? 180.0 : 0.0;
            double targetAzOffset = m_Phases.first().azOffset;
            double targetAz = fmod(meridianAz + targetAzOffset + 360.0, 360.0);

            // 2. Scan Azimuth East/West first, and if necessary, bump Altitude up slightly (up to +10°)
            auto* horizon = &KStarsData::Instance()->skyComposite()->artificialHorizon()->getHorizon();
            double clearAz = 0;
            double clearAlt = targetAlt;
            bool foundClear = false;

            for (double altOffset = 0; altOffset <= 10.0; altOffset += 5.0)
            {
                for (double azOffset = 0; azOffset <= 30.0; azOffset += 5.0)
                {
                    QString reason;

                    // Try West
                    if (horizon->isAltitudeOK(targetAz + azOffset, targetAlt + altOffset, &reason))
                    {
                        clearAz = targetAz + azOffset;
                        clearAlt = targetAlt + altOffset;
                        foundClear = true;
                        break;
                    }
                    // Try East
                    if (horizon->isAltitudeOK(targetAz - azOffset, targetAlt + altOffset, &reason))
                    {
                        clearAz = targetAz - azOffset;
                        clearAlt = targetAlt + altOffset;
                        foundClear = true;
                        break;
                    }
                }
                if (foundClear) break; // Found a clear patch!
            }

            if (!foundClear)
            {
                appendLog("ERROR: Entire meridian is blocked by Artificial Horizon! Cannot proceed.");
                statusLabel->setText("Error: View Blocked");
                m_State = STATE_ERROR;
            }
            else
            {
                if (std::abs(m_TargetAz - clearAz) < 0.1 && std::abs(m_TargetAlt - clearAlt) < 0.1)
                {
                    appendLog("Continuing data collection at current position...");
                    m_State = STATE_SETTLING;
                    m_SettlingTimer = 2; // Minimal settling since we're already here
                }
                else
                {
                    m_TargetAz = clearAz;
                    m_TargetAlt = clearAlt;
                    appendLog(QString("Found safe sky patch: Az %1°, Alt %2°").arg(m_TargetAz).arg(m_TargetAlt));

                    // Issue Slew Command
                    auto *guide = qobject_cast<Guide*>(parentWidget());
                    if (!guide || !guide->mount())
                    {
                        appendLog("ERROR: Mount unavailable, cannot slew.");
                        m_State = STATE_ERROR;
                        break;
                    }
                    SkyPoint targetPoint;
                    targetPoint.setAz(m_TargetAz);
                    targetPoint.setAlt(m_TargetAlt);
                    // Convert Az/Alt to Equatorial (RA/Dec) so Mount can slew
                    targetPoint.HorizontalToEquatorialNow();

                    appendLog("Issuing Slew command to mount...");
                    guide->mount()->Slew(&targetPoint);
                    m_State = STATE_SLEWING;
                }
            }
            break;
        }

        case STATE_SLEWING:
        {
            auto *guide = qobject_cast<Guide*>(parentWidget());
            if (!guide || !guide->mount())
            {
                appendLog("ERROR: Mount became unavailable during slew!");
                m_State = STATE_ERROR;
                break;
            }
            if (guide->mount()->status() == ISD::Mount::MOUNT_TRACKING)
            {
                appendLog("Slew complete! Mount is tracking.");
                m_State = STATE_SETTLING;
                m_SettlingTimer = 10; // Settle for 10 seconds
            }
            else if (guide->mount()->status() == ISD::Mount::MOUNT_ERROR)
            {
                appendLog("ERROR: Mount failed to slew!");
                m_State = STATE_ERROR;
            }
            break;
        }

        case STATE_SETTLING:
            if (m_SettlingTimer > 0)
            {
                statusLabel->setText(QString("Settling... %1s").arg(m_SettlingTimer));
                m_SettlingTimer--;
            }
            else
            {
                appendLog("Settling complete. Starting phase data collection...");
                ProtocolPhase phase = m_Phases.first();
                m_CaptureTimer = phase.durationSeconds;
                m_AbortRetries = 0;
                m_FreeDriftOverflow = false;

                auto *guide = qobject_cast<Guide*>(parentWidget());
                guide->setAIFreeDrift(phase.freeDrift);

                // Clear the phase array for this session
                m_PhaseData = QJsonArray();

                // Connect to the guideStats signal
                connect(guide, &Guide::guideStats, this, &OpsAIGuide::slotOnGuideStats, Qt::UniqueConnection);
                m_FrameTimer.start();

                // Start KStars guiding to trigger camera loop
                if (guide->status() == GUIDE_IDLE || guide->status() == GUIDE_ABORTED)
                {
                    guide->guide();
                }

                m_State = STATE_CAPTURING_DATA;
            }
            break;

        case STATE_CAPTURING_DATA:
        {
            auto *guide = qobject_cast<Guide*>(parentWidget());

            // --- Recovery from unexpected abort (star drift / seeing spike) ---
            if (!guide || guide->status() == GUIDE_ABORTED)
            {
                constexpr int MAX_RETRIES = 3;
                if (m_AbortRetries < MAX_RETRIES)
                {
                    m_AbortRetries++;
                    appendLog(QString("Guider aborted (retry %1/%2). Attempting recovery...")
                              .arg(m_AbortRetries).arg(MAX_RETRIES));
                    if (guide)
                    {
                        guide->setAIFreeDrift(m_Phases.first().freeDrift);
                        guide->guide();
                        m_FrameTimer.start(); // restart so next dt is not huge
                    }
                    break; // wait for next 1Hz tick to see if guider recovered
                }
                // Retries exhausted — save whatever frames we have and move on
                appendLog(QString("Phase ended early (guide star lost after %1 retries). Saving %2 frames.")
                          .arg(MAX_RETRIES).arg(m_PhaseData.size()));
                if (guide)
                {
                    guide->setAIFreeDrift(false);
                    guide->abort();
                    disconnect(guide, &Guide::guideStats, this, &OpsAIGuide::slotOnGuideStats);
                }
                // Fall through to phase-finalisation below by setting timer to 0
                m_CaptureTimer = 0;
            }

            if (guide && guide->status() != GUIDE_GUIDING && guide->status() != GUIDE_ABORTED)
            {
                statusLabel->setText("Waiting for Guider to calibrate/start...");
                // Restart frame timer so the first 'dt' isn't huge
                m_FrameTimer.start();
                break; // Pause timer countdown
            }

            // --- Free-drift safety: end early if star has drifted too far ---
            const bool phaseTimedOut = (m_CaptureTimer <= 0);
            const bool freeDriftOverflowed = m_FreeDriftOverflow;

            if (phaseTimedOut || freeDriftOverflowed)
            {
                if (freeDriftOverflowed)
                    appendLog(QString("Free drift ended early to protect guide star (%1 frames saved).")
                              .arg(m_PhaseData.size()));
                else
                    appendLog("Phase complete. Stopping capture...");

                if (guide)
                {
                    guide->abort();
                    guide->setAIFreeDrift(false);
                    disconnect(guide, &Guide::guideStats, this, &OpsAIGuide::slotOnGuideStats);
                }

                // Package the phase data into the main JSON
                ProtocolPhase phase = m_Phases.first();
                QJsonObject phaseRecord;
                phaseRecord["session_id"] = QString("phase_alt%1_%2").arg(phase.targetAlt).arg(
                                                QDateTime::currentDateTime().toString("HHmmss"));
                phaseRecord["type"] = phase.freeDrift ? "free_drift" : "standard_guiding";
                // Record the MEAN achieved altitude over the phase, not the nominal target: the
                // horizon scan can raise the actual slew target by up to +10° and the mount tracks
                // through the multi-minute session. The trainers fit refraction (k_ref/k_ref_dec)
                // against this single session altitude, so the nominal targetAlt biases the fit.
                double meanAlt = m_TargetAlt;
                if (!m_PhaseData.isEmpty())
                {
                    double sumAlt = 0.0;
                    for (int i = 0; i < m_PhaseData.size(); ++i)
                        sumAlt += m_PhaseData.at(i).toObject()["altitude_deg"].toDouble();
                    meanAlt = sumAlt / m_PhaseData.size();
                }
                phaseRecord["altitude_deg"] = meanAlt;
                phaseRecord["azimuth_deg"] = m_TargetAz;
                if (guide && guide->mount())
                {
                    auto pierSide = guide->mount()->pierSide();
                    phaseRecord["pier_side"] = (pierSide == ISD::Mount::PIER_EAST) ? "EAST" : "WEST";
                }
                phaseRecord["duration_s"] = phase.durationSeconds;
                phaseRecord["aggressiveness_ra"] = phase.freeDrift ? 0.0 : Options::rAProportionalGain();
                phaseRecord["aggressiveness_dec"] = phase.freeDrift ? 0.0 : Options::dECProportionalGain();
                phaseRecord["min_pulse_ra_arcsec"] = phase.freeDrift ? 0.0 : Options::rAMinimumPulseArcSec();
                phaseRecord["min_pulse_dec_arcsec"] = phase.freeDrift ? 0.0 : Options::dECMinimumPulseArcSec();
                phaseRecord["max_pulse_ra_arcsec"] = phase.freeDrift ? 0.0 : Options::rAMaximumPulseArcSec();
                phaseRecord["max_pulse_dec_arcsec"] = phase.freeDrift ? 0.0 : Options::dECMaximumPulseArcSec();

                if (guide)
                {
                    auto *internalGuider = qobject_cast<Ekos::InternalGuider*>(guide->getGuiderInstance());
                    if (internalGuider)
                    {
                        const auto &cal = internalGuider->getCalibration();
                        phaseRecord["ra_ms_per_arcsec"] = cal.raPulseMillisecondsPerArcsecond();
                        phaseRecord["dec_ms_per_arcsec"] = cal.decPulseMillisecondsPerArcsecond();
                    }
                }

                phaseRecord["frames"] = m_PhaseData;

                QJsonArray sessions = m_SysIdData["sessions"].toArray();
                sessions.append(phaseRecord);
                m_SysIdData["sessions"] = sessions;

                // Write out the sysid_data.json
                m_LogFile.setFileName(m_LogFilename);
                if (m_LogFile.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    QJsonDocument doc(m_SysIdData);
                    m_LogFile.write(doc.toJson());
                    m_LogFile.close();
                }

                m_Phases.removeFirst();
                m_State = STATE_PRECHECK; // Loop back for next phase
            }
            else
            {
                statusLabel->setText(QString("Capturing Data... %1s remaining").arg(m_CaptureTimer));
                m_CaptureTimer--;
            }
            break;
        }

        // ── Pulse Response States ─────────────────────────────────────────────
        case STATE_PULSE_RESPONSE_INIT:
        {
            auto *guide = qobject_cast<Guide*>(parentWidget());
            if (!guide)
            {
                m_State = STATE_ERROR;
                break;
            }

            ProtocolPhase phase = m_Phases.first();
            appendLog(QString("Pulse Response: %1 %2 %3ms — preparing...")
                      .arg(phase.pulseAxis, phase.pulseDirection)
                      .arg(phase.pulseMagnitudeMs));

            // Disable all corrections (enter free drift)
            guide->setAIFreeDrift(true);

            m_PulseFrameCount = 0;
            m_PulseResponseData = QJsonArray();

            // Connect to stats to record response frames
            connect(guide, &Guide::guideStats, this, &OpsAIGuide::slotOnGuideStats, Qt::UniqueConnection);

            // Make sure guiding is running (camera loop)
            if (guide->status() == GUIDE_IDLE || guide->status() == GUIDE_ABORTED)
                guide->guide();

            m_FrameTimer.start();
            m_PulseSettleTimer = 5; // 5s initial settle
            m_State = STATE_PULSE_SETTLING;
            break;
        }

        case STATE_PULSE_SENDING:
        {
            auto *guide = qobject_cast<Guide*>(parentWidget());
            if (!guide)
            {
                m_State = STATE_ERROR;
                break;
            }

            ProtocolPhase phase = m_Phases.first();

            // Send the pulse via the guide module's sendSinglePulse API
            guide->setAIFreeDrift(false);
            if (phase.pulseAxis == "RA")
            {
                if (phase.pulseDirection == "EAST")
                    guide->sendSinglePulse(RA_INC_DIR, phase.pulseMagnitudeMs, StartCaptureAfterPulses);
                else
                    guide->sendSinglePulse(RA_DEC_DIR, phase.pulseMagnitudeMs, StartCaptureAfterPulses);
            }
            else // DEC
            {
                if (phase.pulseDirection == "NORTH")
                    guide->sendSinglePulse(DEC_INC_DIR, phase.pulseMagnitudeMs, StartCaptureAfterPulses);
                else
                    guide->sendSinglePulse(DEC_DEC_DIR, phase.pulseMagnitudeMs, StartCaptureAfterPulses);
            }
            guide->setAIFreeDrift(true);

            appendLog(QString("Sent %1ms %2 %3 pulse. Recording %4 response frames...")
                      .arg(phase.pulseMagnitudeMs).arg(phase.pulseAxis).arg(phase.pulseDirection)
                      .arg(phase.responseFrames));

            m_PulseFrameCount = 0;
            m_PulseResponseData = QJsonArray();
            m_FrameTimer.start();
            // Backstop (1Hz ticks): the pulse deliberately shoves the star, so it may leave the
            // tracking box and abort the guider mid-recording. Bound the wait so we never hang.
            m_PulseWatchdog = phase.responseFrames * 6 + 30;
            m_State = STATE_PULSE_RECORDING;
            break;
        }

        case STATE_PULSE_RECORDING:
        {
            ProtocolPhase phase = m_Phases.first();
            auto *guide = qobject_cast<Guide*>(parentWidget());

            // The pulse deliberately shoves the star; if it leaves the tracking box the internal
            // guider ABORTS, guideStats stops firing and m_PulseFrameCount freezes. Without this
            // guard the wizard would hang here forever with the mount stuck in Free-Drift. Detect
            // an aborted/gone guider or a stalled recording (watchdog) and finalize gracefully.
            const bool guiderAborted = (!guide || guide->status() == GUIDE_ABORTED);
            if (m_PulseWatchdog > 0)
                m_PulseWatchdog--;
            const bool completed   = (m_PulseFrameCount >= phase.responseFrames);
            const bool interrupted = (!completed && (guiderAborted || m_PulseWatchdog <= 0));

            statusLabel->setText(QString("Recording pulse response: %1/%2 frames")
                                 .arg(m_PulseFrameCount).arg(phase.responseFrames));

            if (completed || interrupted)
            {
                if (interrupted)
                    appendLog(QString("Pulse response interrupted (%1) after %2/%3 frames.")
                              .arg(guiderAborted ? "guider aborted — star likely pushed out of frame" : "timed out")
                              .arg(m_PulseFrameCount).arg(phase.responseFrames));
                else
                    appendLog(QString("Pulse response recorded: %1 frames").arg(m_PulseFrameCount));

                // Package the pulse response session into sysid JSON (skip if nothing usable was captured).
                if (m_PulseFrameCount > 0)
                {
                    ProtocolPhase p = m_Phases.first();
                    QJsonObject pulseSession;
                    pulseSession["session_id"] = QString("pulse_response_%1_%2_%3ms_%4")
                                                 .arg(p.pulseAxis.toLower(), p.pulseDirection.toLower())
                                                 .arg(p.pulseMagnitudeMs)
                                                 .arg(QDateTime::currentDateTime().toString("HHmmss"));
                    pulseSession["type"] = "pulse_response";
                    pulseSession["pulse_axis"] = p.pulseAxis;
                    pulseSession["pulse_direction"] = p.pulseDirection;
                    pulseSession["pulse_magnitude_ms"] = p.pulseMagnitudeMs;
                    // Record the commanded slew altitude (post horizon-scan), consistent with m_TargetAz.
                    pulseSession["altitude_deg"] = m_TargetAlt;
                    pulseSession["azimuth_deg"] = m_TargetAz;

                    if (guide && guide->mount())
                    {
                        auto pierSide = guide->mount()->pierSide();
                        pulseSession["pier_side"] = (pierSide == ISD::Mount::PIER_EAST) ? "EAST" : "WEST";
                    }

                    pulseSession["response_frames"] = m_PulseResponseData;

                    QJsonArray sessions = m_SysIdData["sessions"].toArray();
                    sessions.append(pulseSession);
                    m_SysIdData["sessions"] = sessions;

                    // Write out the sysid_data.json incrementally
                    m_LogFile.setFileName(m_LogFilename);
                    if (m_LogFile.open(QIODevice::WriteOnly | QIODevice::Text))
                    {
                        QJsonDocument doc(m_SysIdData);
                        m_LogFile.write(doc.toJson());
                        m_LogFile.close();
                    }
                }

                if (interrupted)
                {
                    // Guider is aborted/stalled: skip the post-pulse settle (it would just wait out
                    // the timer with no star) and advance directly to the next phase.
                    if (guide)
                    {
                        guide->setAIFreeDrift(false);
                        disconnect(guide, &Guide::guideStats, this, &OpsAIGuide::slotOnGuideStats);
                    }
                    m_Phases.removeFirst();
                    m_State = STATE_PRECHECK;
                }
                else
                {
                    // Post-pulse settle before next pulse
                    m_PulseSettleTimer = phase.settleSeconds;
                    m_State = STATE_PULSE_SETTLING;
                }
            }
            break;
        }

        case STATE_PULSE_SETTLING:
        {
            if (m_PulseSettleTimer > 0)
            {
                statusLabel->setText(QString("Pulse settling... %1s").arg(m_PulseSettleTimer));
                m_PulseSettleTimer--;
            }
            else
            {
                // Check if this was the initial settle (before sending pulse)
                // or the post-recording settle (after recording)
                if (m_PulseFrameCount == 0 && m_PulseResponseData.isEmpty())
                {
                    // Initial settle complete → send the pulse
                    m_State = STATE_PULSE_SENDING;
                }
                else
                {
                    // Post-recording settle complete → advance to next phase
                    auto *guide = qobject_cast<Guide*>(parentWidget());
                    if (guide)
                    {
                        guide->setAIFreeDrift(false);
                        disconnect(guide, &Guide::guideStats, this, &OpsAIGuide::slotOnGuideStats);
                    }
                    m_Phases.removeFirst();
                    m_State = STATE_PRECHECK;
                }
            }
            break;
        }

        default:
            break;
    } // end switch

    // Update Progress Bar
    if (m_TotalPhases > 0 && m_State != STATE_DONE && m_State != STATE_ERROR && m_State != STATE_IDLE)
    {
        int completed = m_TotalPhases - m_Phases.size();
        double currentProgress = 0.0;

        if (!m_Phases.isEmpty() && m_Phases.first().durationSeconds > 0)
        {
            if (m_State == STATE_CAPTURING_DATA)
            {
                currentProgress = 1.0 - (static_cast<double>(m_CaptureTimer) / m_Phases.first().durationSeconds);
            }
            else if (m_State == STATE_HORIZON_SCAN || m_State == STATE_SLEWING || m_State == STATE_SETTLING)
            {
                currentProgress = 0.1; // Baseline for transition states
            }
        }

        int progressValue = static_cast<int>(((completed + currentProgress) / m_TotalPhases) * 100);
        progressBar->setValue(std::max(0, std::min(99, progressValue)));
        emit aiGuideProgress(completed, m_TotalPhases, statusLabel->text());
    }
}

void OpsAIGuide::slotOnGuideStats(double raErr, double decErr, int raPulse, int decPulse, double snr, double skyBg,
                                  int numStars)
{
    Q_UNUSED(skyBg)
    Q_UNUSED(numStars)

    if (m_State == STATE_CAPTURING_DATA)
    {
        double dt = m_FrameTimer.isValid() ? (m_FrameTimer.restart() / 1000.0) : 0.0;

        // --- Free-drift overflow guard ---
        // If the guide star has drifted more than 25 arcsec from centre during free drift,
        // flag the overflow so STATE_CAPTURING_DATA ends the phase before the star is lost.
        if (m_Phases.first().freeDrift && !m_FreeDriftOverflow)
        {
            constexpr double FREE_DRIFT_LIMIT_ARCSEC = 25.0;
            if (std::abs(raErr) > FREE_DRIFT_LIMIT_ARCSEC || std::abs(decErr) > FREE_DRIFT_LIMIT_ARCSEC)
            {
                appendLog(QString("Free drift limit reached (RA=%1\" DEC=%2\"). Ending phase early to protect star.")
                          .arg(raErr, 0, 'f', 1).arg(decErr, 0, 'f', 1));
                m_FreeDriftOverflow = true;
                return; // don't record this out-of-bounds frame
            }
        }

        double dx = raErr;
        double dy = decErr;
        auto *guide = qobject_cast<Guide *>(parentWidget());
        if (guide && guide->pixelSizeX() > 0 && guide->focalLength() > 0)
        {
            // Note: Options::guideBinning() is "1x1", "2x2", etc. We extract the first char to get bin factor.
            double binning = std::max(1, Options::guideBinning().left(1).toInt());
            double scale = (206.265 * guide->pixelSizeX() * binning) / guide->focalLength();
            dx = -raErr / scale; // raErr is negated delta in gmath
            dy = decErr / scale;
        }

        // Fetch current mount altitude, azimuth and DEC
        double alt_deg = 45.0, az_deg = 180.0, dec_deg = 0.0;
        if (guide && guide->mount())
        {
            SkyPoint currentPos = guide->mount()->currentCoordinates();
            az_deg = currentPos.az().Degrees();
            alt_deg = currentPos.alt().Degrees();
            dec_deg = currentPos.dec().Degrees();
        }

        // Fetch Latitude
        double lat_deg = 45.0;
        if (KStarsData::Instance() && KStarsData::Instance()->geo() && KStarsData::Instance()->geo()->lat())
        {
            lat_deg = KStarsData::Instance()->geo()->lat()->Degrees();
        }

        // Calculate Parallactic Angle (q)
        double parallactic_angle_deg = 0.0;
        double sin_az = std::sin(az_deg * M_PI / 180.0);
        double cos_lat = std::cos(lat_deg * M_PI / 180.0);
        double cos_dec = std::cos(dec_deg * M_PI / 180.0);

        if (std::abs(cos_dec) > 1e-6)
        {
            double sin_q = (sin_az * cos_lat) / cos_dec;
            sin_q = std::clamp(sin_q, -1.0, 1.0);
            parallactic_angle_deg = std::asin(sin_q) * 180.0 / M_PI;
        }

        QJsonObject frame;
        frame["timestamp"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
        frame["altitude_deg"] = alt_deg;
        frame["azimuth_deg"] = az_deg;
        frame["parallactic_angle_deg"] = parallactic_angle_deg;
        frame["ra_raw_px"] = dx;
        frame["dec_raw_px"] = dy;
        frame["snr"] = snr;
        frame["dt"] = dt;
        frame["error_code"] = (snr == 0.0) ? FRAME_STAR_LOST : FRAME_OK;
        frame["ra_pulse_ms"] = raPulse;
        frame["dec_pulse_ms"] = decPulse;
        m_PhaseData.append(frame);
    }

    // ── Pulse response frame recording ───────────────────────────────────
    if (m_State == STATE_PULSE_RECORDING)
    {
        double dt = m_FrameTimer.isValid() ? (m_FrameTimer.restart() / 1000.0) : 0.0;

        double dx = raErr;
        double dy = decErr;
        auto *guide = qobject_cast<Guide *>(parentWidget());
        if (guide && guide->pixelSizeX() > 0 && guide->focalLength() > 0)
        {
            double binning = std::max(1, Options::guideBinning().left(1).toInt());
            double scale = (206.265 * guide->pixelSizeX() * binning) / guide->focalLength();
            dx = -raErr / scale;
            dy = decErr / scale;
        }

        QJsonObject frame;
        frame["t"] = m_PulseFrameCount * dt;
        frame["ra_raw_px"] = dx;
        frame["dec_raw_px"] = dy;
        frame["snr"] = snr;
        frame["dt"] = dt;
        m_PulseResponseData.append(frame);
        m_PulseFrameCount++;
    }
}

}
