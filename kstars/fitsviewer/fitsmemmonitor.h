/*
    SPDX-FileCopyrightText: 2025 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>
#include <QTimer>
#include <QProgressBar>
#include <QLabel>
#include <QHBoxLayout>
#include <QtGlobal>
#include <QEvent>
#include <QMouseEvent>
#include <KLocalizedString>

/**
 * @brief FITSMemoryMonitor provides functionality to display memory usage and update the display periodically.
 *
 * @author John Evans
 */

struct MemoryInfo
{
    quint64 totalSystemRAM = 0;
    quint64 processMemoryUsage = 0;
};

class FITSMemoryMonitor : public QWidget
{
    Q_OBJECT

  public:
    explicit FITSMemoryMonitor(QWidget *parent = nullptr);

    /**
     * @brief Get the update interval, default is 1000 = 1s
     * @return interval
     */
    int updateInterval() const;

    /**
     * @brief Set the update interval
     * @param interval
     */
    void setUpdateInterval(const int interval);

    /**
     * @brief Get the show label setting
     * @return show setting
     */
    bool showLabel() const;

    /**
     * @brief Set the show label setting
     * @param show setting
     */
    void setShowLabel(const bool show);

    /**
     * @brief Get the label format
     * @return label format
     */
    QString labelFormat() const;

    /**
     * @brief Set the label format
     * @param label format
     */
    void setLabelFormat(const QString &format);

  protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

  private slots:
    void updateMemoryDisplay();
    void onLabelDoubleClicked();

  private:
    void setupUI();
    MemoryInfo getMemoryInfo();
    QString formatBytes(quint64 bytes);

    QTimer *updateTimer { nullptr };
    QProgressBar *memoryBar { nullptr };
    QLabel *memoryLabel { nullptr };
    int m_updateInterval { 1000 };
    bool m_showLabel { true };
    QString m_labelFormat { "RAM: %1 / %2" };
};
