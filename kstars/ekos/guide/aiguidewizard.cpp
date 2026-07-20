/*
    SPDX-FileCopyrightText: 2026 Pavan <pk6122004@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "aiguidewizard.h"
#include "aiguideprotocol.h"
#include "guide.h"
#include "kspaths.h"
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QDirIterator>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <zlib.h>

namespace
{
// Minimal in-process ZIP (DEFLATE) writer — avoids pulling in KArchive.
class ZipWriter
{
    public:
        bool open(const QString &path)
        {
            m_file.setFileName(path);
            return m_file.open(QIODevice::WriteOnly);
        }

        int addTree(const QString &dir, const QString &prefix, const QDateTime &since = QDateTime())
        {
            if (!QDir(dir).exists())
                return 0;
            int n = 0;
            const QString baseDir = QDir(dir).absolutePath();
            QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext())
            {
                const QString fp = it.next();
                if (since.isValid() && QFileInfo(fp).lastModified() < since)
                    continue;
                if (addOne(fp, prefix + "/" + QDir(baseDir).relativeFilePath(fp)))
                    ++n;
            }
            return n;
        }

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
            put32(hdr, 0x04034b50);
            put16(hdr, 20);
            put16(hdr, 0);
            put16(hdr, 8);
            put16(hdr, e.dosTime);
            put16(hdr, e.dosDate);
            put32(hdr, e.crc);
            put32(hdr, e.compSize);
            put32(hdr, e.rawSize);
            put16(hdr, static_cast<quint16>(name.size()));
            put16(hdr, 0);
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
                put32(cd, 0x02014b50);
                put16(cd, 20);
                put16(cd, 20);
                put16(cd, 0);
                put16(cd, 8);
                put16(cd, e.dosTime);
                put16(cd, e.dosDate);
                put32(cd, e.crc);
                put32(cd, e.compSize);
                put32(cd, e.rawSize);
                put16(cd, static_cast<quint16>(e.name.size()));
                put16(cd, 0);
                put16(cd, 0);
                put16(cd, 0);
                put16(cd, 0);
                put32(cd, 0);
                put32(cd, e.offset);
                cd.append(e.name);
            }
            m_file.write(cd);

            QByteArray eocd;
            put32(eocd, 0x06054b50);
            put16(eocd, 0);
            put16(eocd, 0);
            put16(eocd, static_cast<quint16>(m_entries.size()));
            put16(eocd, static_cast<quint16>(m_entries.size()));
            put32(eocd, static_cast<quint32>(cd.size()));
            put32(eocd, cdStart);
            put16(eocd, 0);
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
            if (year < 0) year = 0;
            return static_cast<quint16>((year << 9) | (d.month() << 5) | d.day());
        }
        static QByteArray rawDeflate(const QByteArray &data)
        {
            z_stream s {};
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

AIGuideWizard::AIGuideWizard(AIGuideProtocol *protocol, QWidget *parent) : QWizard(parent), m_Protocol(protocol)
{
    setupUi(this);

    // Wire protocol signals → wizard UI only (EkosLive forwarding is handled directly in Guide ctor)
    connect(m_Protocol, &AIGuideProtocol::protocolLog, this, &AIGuideWizard::appendLog);
    connect(m_Protocol, &AIGuideProtocol::protocolProgress, this, [this](int current, int total, const QString & status)
    {
        if (total > 0)
        {
            int progressValue = static_cast<int>(((current + 0.1) / total) * 100);
            progressBar->setValue(std::max(0, std::min(99, progressValue)));
        }
        statusLabel->setText(status);
    });
    connect(m_Protocol, &AIGuideProtocol::protocolComplete, this, [this]()
    {
        progressBar->setValue(100);
        exportOfflineButton->setEnabled(true);
        this->next();
    });

    connect(stopButton, &QPushButton::clicked, this, &AIGuideWizard::slotStopProtocol);
    connect(exportOfflineButton, &QPushButton::clicked, this, &AIGuideWizard::slotExportOffline);

    connect(m_Protocol, &AIGuideProtocol::protocolStopped, this, [this]()
    {
        stopButton->setText(i18n("Start"));
        stopButton->setEnabled(true);
        disconnect(stopButton, &QPushButton::clicked, this, &AIGuideWizard::slotStopProtocol);
        connect(stopButton, &QPushButton::clicked, this, &AIGuideWizard::slotStartProtocol);
    });

    connect(m_Protocol, &AIGuideProtocol::trainingRequested, this, [this](const QJsonObject & data)
    {
        if (data["sessions"].toArray().isEmpty())
        {
            appendLog("No data to train! Please run the protocol first.");
            return;
        }
        trainEkosLiveButton->setEnabled(false);
        trainEkosLiveButton->setText("Uploading and Training...");
        appendLog("Uploading system-ID data to EkosLive Cloud for training...");
        emit trainInEkosLiveRequested(data);
    });

    connect(trainEkosLiveButton, &QPushButton::clicked, this, [this]()
    {
        m_Protocol->requestTraining();
    });

    connect(offlineDocsButton, &QPushButton::clicked, this, []()
    {
        QDesktopServices::openUrl(QUrl("https://kstars-docs.kde.org/en/user_manual/ekos-guide.html#training-the-model-offline"));
    });

    setButtonText(QWizard::CustomButton1, "Export Logs");
    setOption(QWizard::HaveCustomButton1, true);
    connect(this, &QWizard::customButtonClicked, this, [this](int which)
    {
        if (which == QWizard::CustomButton1)
            slotExportLogs();
    });

    progressBar->setValue(0);
}

void AIGuideWizard::showEvent(QShowEvent *event)
{
    QWizard::showEvent(event);

    m_AutoNavigating = true;

    // If protocol is already running (e.g. started via EkosLive), jump to page 2
    if (m_Protocol->state() != AIGuideProtocol::STATE_IDLE &&
            m_Protocol->state() != AIGuideProtocol::STATE_DONE &&
            m_Protocol->state() != AIGuideProtocol::STATE_ERROR)
    {
        for (int i = 0; i < 4 && currentId() != 2; i++)
            next();

        stopButton->setText(i18n("Stop"));
        stopButton->setEnabled(true);
        disconnect(stopButton, &QPushButton::clicked, this, &AIGuideWizard::slotStartProtocol);
        connect(stopButton, &QPushButton::clicked, this, &AIGuideWizard::slotStopProtocol, Qt::UniqueConnection);
    }
    // If protocol already completed, jump to training page (page 3)
    else if (m_Protocol->state() == AIGuideProtocol::STATE_DONE)
    {
        for (int i = 0; i < 4 && currentId() != 3; i++)
            next();
        exportOfflineButton->setEnabled(true);
        progressBar->setValue(100);
    }
    else if (m_Protocol->state() == AIGuideProtocol::STATE_IDLE && currentId() != 0)
        restart();

    m_AutoNavigating = false;
}

int AIGuideWizard::state() const
{
    return static_cast<int>(m_Protocol->state());
}

int AIGuideWizard::totalPhases() const
{
    return m_Protocol->totalPhases();
}

int AIGuideWizard::phasesRemaining() const
{
    return m_Protocol->phasesRemaining();
}

const QJsonObject &AIGuideWizard::sysIdData() const
{
    return m_Protocol->sysIdData();
}

void AIGuideWizard::onTrainingResult(bool success, const QJsonObject &result)
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

void AIGuideWizard::slotExportLogs()
{
    const QString base = KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    QDateTime since = QDateTime::currentDateTime().addDays(-1);
    const auto sessions = QDir(base + "/ai_training_logs").entryList(QStringList("sysid_data_*.json"),
                          QDir::Files, QDir::Name);
    if (!sessions.isEmpty())
    {
        const QString stamp = QFileInfo(sessions.last()).completeBaseName().mid(QString("sysid_data_").size());
        const QDateTime newest = QDateTime::fromString(stamp, "yyyyMMdd_HHmmss");
        if (newest.isValid())
            since = newest;
    }
    const QString suggested = QDir::homePath() + "/ekos_ai_logs_" +
                              QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".zip";
    const QString out = QFileDialog::getSaveFileName(this, "Export Logs", suggested, "Zip Archives (*.zip)");
    if (out.isEmpty()) return;

    if (auto *exportButton = button(QWizard::CustomButton1))
        exportButton->setEnabled(false);
    appendLog(QString("Exporting logs to %1...").arg(out));

    auto *watcher = new QFutureWatcher<int>(this);
    connect(watcher, &QFutureWatcher<int>::finished, this, [this, watcher, out]()
    {
        watcher->deleteLater();
        const int n = watcher->result();
        if (auto *exportButton = button(QWizard::CustomButton1))
            exportButton->setEnabled(true);
        if (n < 0)
            appendLog("Failed to create archive: " + out);
        else if (n > 0)
            appendLog(QString("Exported %1 log file(s) to: %2").arg(n).arg(out));
        else
            appendLog("No logs found for the latest session. Enable 'Log to File' in KStars settings to also capture terminal logs.");
    });

    watcher->setFuture(QtConcurrent::run([base, out, since]()
    {
        int n = -1;
        ZipWriter zip;
        if (zip.open(out))
        {
            n = 0;
            n += zip.addTree(base + "/ai_debug_logs",    "ai_debug_logs",    since);
            n += zip.addTree(base + "/ai_training_logs", "ai_training_logs", since);
            n += zip.addTree(base + "/guidelogs",        "guidelogs",        since);
            n += zip.addTree(base + "/logs",             "logs",             since);
            if (!since.isValid() || QFileInfo(base + "/guide_log.txt").lastModified() >= since)
                n += zip.addOne(base + "/guide_log.txt", "guide_log.txt");
            zip.finish();
        }
        return n;
    }));
}

void AIGuideWizard::initializePage(int id)
{
    QWizard::initializePage(id);

    // Page 2 (0-indexed) is the "System Identification Progress" page
    if (id == 2 && !m_AutoNavigating)
    {
        progressBar->setValue(0);
        logTextEdit->clear();
        exportOfflineButton->setEnabled(false);

        stopButton->setText(i18n("Stop"));
        stopButton->setEnabled(true);
        disconnect(stopButton, &QPushButton::clicked, this, &AIGuideWizard::slotStartProtocol);
        connect(stopButton, &QPushButton::clicked, this, &AIGuideWizard::slotStopProtocol, Qt::UniqueConnection);

        m_Protocol->start(mountTypeCombo->currentText());
    }
}

void AIGuideWizard::appendLog(const QString &message)
{
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss");
    logTextEdit->append(QString("[%1] %2").arg(timeStr, message));
}

void AIGuideWizard::slotStartProtocol()
{
    // Navigate to the progress page (page 2) so the user sees logs/progress,
    // even when called programmatically via DBus/EkosLive.
    // initializePage(2) handles UI setup and protocol start.
    restart();
    for (int i = 0; i < 2; ++i)
        next();
}

void AIGuideWizard::slotExportOffline()
{
    const auto &filename = m_Protocol->logFilename();
    if (filename.isEmpty() || !QFile::exists(filename))
    {
        appendLog("No data to export! Please run the protocol first.");
        return;
    }

    QString savePath = QFileDialog::getSaveFileName(this, "Export Offline Training Data",
                       QDir::homePath() + "/sysid_data.json", "JSON Files (*.json)");
    if (!savePath.isEmpty())
    {
        if (QFile::copy(filename, savePath))
            appendLog(QString("Successfully exported data to: %1").arg(savePath));
        else
            appendLog("Failed to export data!");
    }
}

void AIGuideWizard::slotStopProtocol()
{
    m_Protocol->stop();
    statusLabel->setText("Protocol Aborted by User");
}

void AIGuideWizard::done(int result)
{
    const auto state = m_Protocol->state();
    if (state > AIGuideProtocol::STATE_IDLE && state < AIGuideProtocol::STATE_DONE)
        m_Protocol->stop();
    QWizard::done(result);
}

}