/*
    SPDX-FileCopyrightText: 2025 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fitsmemmonitor.h"
#include <QTimer>

#ifdef Q_OS_LINUX
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#elif defined(Q_OS_WIN)
#include <windows.h>
#include <psapi.h>
#elif defined(Q_OS_MACOS)
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <sys/sysctl.h>
#include <mach/task.h>
#endif

FITSMemoryMonitor::FITSMemoryMonitor(QWidget *parent) : QWidget(parent)
{
    setupUI();
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &FITSMemoryMonitor::updateMemoryDisplay);
    updateTimer->start(m_updateInterval);
    updateMemoryDisplay();
}

int FITSMemoryMonitor::updateInterval() const
{
    return m_updateInterval;
}

void FITSMemoryMonitor::setUpdateInterval(const int interval)
{
    m_updateInterval = std::max(100, interval);
    if (updateTimer)
        updateTimer->setInterval(m_updateInterval);
}

bool FITSMemoryMonitor::showLabel() const
{
    return m_showLabel;
}

void FITSMemoryMonitor::setShowLabel(const bool show)
{
    m_showLabel = show;
    if (memoryLabel)
        memoryLabel->setVisible(show);
}

QString FITSMemoryMonitor::labelFormat() const
{
    return m_labelFormat;
}

void FITSMemoryMonitor::setLabelFormat(const QString &format)
{
    m_labelFormat = format;
}

void FITSMemoryMonitor::setupUI()
{
    auto *layout = new QHBoxLayout(this);

    memoryLabel = new QLabel(i18n("RAM: -- / --"));
    memoryLabel->setVisible(m_showLabel);
    memoryLabel->installEventFilter(this);

    memoryBar = new QProgressBar();
    memoryBar->setRange(0, 100);
    memoryBar->setTextVisible(true);
    memoryBar->setFormat("%p%");
    memoryBar->setToolTip(i18n("Double-click to toggle text display"));
    memoryBar->setMinimumWidth(100);
    memoryBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    memoryBar->installEventFilter(this);

    layout->addWidget(memoryLabel);
    layout->addStretch();
    layout->addWidget(memoryBar);
    layout->setContentsMargins(0, 0, 0, 0);

    setMaximumHeight(30);
}

MemoryInfo FITSMemoryMonitor::getMemoryInfo()
{
    MemoryInfo info = {};

#ifdef Q_OS_LINUX
    // System memory from /proc/meminfo
    QFile meminfo("/proc/meminfo");
    if (meminfo.open(QIODevice::ReadOnly))
    {
        QTextStream stream(&meminfo);
        QString line;
        while (stream.readLineInto(&line))
        {
            QStringList parts = line.split(QRegularExpression("\\s+"));
            if (parts.size() >= 2 && line.startsWith("MemTotal:"))
            {
                info.totalSystemRAM = parts[1].toULongLong() * 1024;
                break;
            }
        }
    }

    // Process memory from /proc/self/status
    QFile status("/proc/self/status");
    if (status.open(QIODevice::ReadOnly))
    {
        QTextStream stream(&status);
        QString line;
        while (stream.readLineInto(&line))
        {
            QStringList parts = line.split(QRegularExpression("\\s+"));
            if (parts.size() >= 2 && line.startsWith("VmRSS:"))
            {
                info.processMemoryUsage = parts[1].toULongLong() * 1024;
                break;
            }
        }
    }

#elif defined(Q_OS_WIN)
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus))
        info.totalSystemRAM = memStatus.ullTotalPhys;

    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
        info.processMemoryUsage = pmc.WorkingSetSize;

#elif defined(Q_OS_MACOS)
    // Total physical memory
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    uint64_t physical_memory;
    size_t length = sizeof(uint64_t);
    if (sysctl(mib, 2, &physical_memory, &length, NULL, 0) == 0)
        info.totalSystemRAM = physical_memory;

    // Process memory usage
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_BASIC_INFO,
                  (task_info_t)&t_info, &t_info_count) == KERN_SUCCESS)
        info.processMemoryUsage = t_info.resident_size;
#endif

    return info;
}

QString FITSMemoryMonitor::formatBytes(quint64 bytes)
{
    const quint64 GB = 1024ULL * 1024ULL * 1024ULL;
    const quint64 MB = 1024ULL * 1024ULL;

    if (bytes >= GB)
        return QString::number(bytes / (double)GB, 'f', 1) + " GB";
    else if (bytes >= MB)
        return QString::number(bytes / (double)MB, 'f', 0) + " MB";
    else
        return QString::number(bytes / 1024.0, 'f', 0) + " KB";
}

void FITSMemoryMonitor::updateMemoryDisplay()
{
    MemoryInfo info = getMemoryInfo();

    if (info.totalSystemRAM > 0 && info.processMemoryUsage > 0)
    {
        QString processStr = formatBytes(info.processMemoryUsage);
        QString totalStr = formatBytes(info.totalSystemRAM);

        memoryLabel->setText(m_labelFormat.arg(processStr, totalStr));

        int percentage = (int)((double)info.processMemoryUsage / info.totalSystemRAM * 100);
        memoryBar->setValue(percentage);

        // Colour the progress bar
        QPalette p = memoryBar->palette();
        if (percentage > 70)
            p.setColor(QPalette::Highlight, Qt::red);
        else if (percentage >50)
            // No Qt defined Orange or Amber
            p.setColor(QPalette::Highlight, QColor(255, 191, 0));
        else
            p.setColor(QPalette::Highlight, Qt::green);
        memoryBar->setPalette(p);

    }
    else
    {
        memoryLabel->setText(i18n("RAM: -- / --"));
        memoryBar->setValue(0);
        QPalette p = memoryBar->palette();
        p.setColor(QPalette::Highlight, Qt::green);
        memoryBar->setPalette(p);
    }
}

bool FITSMemoryMonitor::eventFilter(QObject *obj, QEvent *event)
{
    if ((obj == memoryLabel || obj == memoryBar) && event->type() == QEvent::MouseButtonDblClick)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton)
        {
            onLabelDoubleClicked();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// Toggle the label visibility
void FITSMemoryMonitor::onLabelDoubleClicked()
{
    m_showLabel = !m_showLabel;
    memoryLabel->setVisible(m_showLabel);
}
