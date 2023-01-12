/*
    SPDX-FileCopyrightText: 2023 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_yaxistool.h"
#include "qcustomplot.h"

#include <QDialog>
#include <QFrame>

#include <memory>

/**
 * @class YAxisInfo
 * @short Used to keep track of the various Y-axes and connect them to the QLineEdits.
 *
 * @version 1.0
 * @author Hy Murveit
 */
struct YAxisInfo
{
    static constexpr double LOWER_RESCALE = -102, UPPER_RESCALE = -101;
    QCPAxis *axis;
    QCPRange initialRange;
    bool rescale;
    int graphIndex;
    QCustomPlot *plot;
    QCheckBox *checkBox;
    QString name;
    QString shortName;
    QColor initialColor;
    QColor color;
    YAxisInfo() {}
    YAxisInfo(QCPAxis *axis_, QCPRange initialRange_, bool rescale_, int graphIndex_,
              QCustomPlot *plot_, QCheckBox *checkBox_, const QString &name_,
              const QString &shortName_, const QColor &color_)
        : axis(axis_), initialRange(initialRange_), rescale(rescale_), graphIndex(graphIndex_),
          plot(plot_), checkBox(checkBox_), name(name_), shortName(shortName_),
          initialColor(color_), color(color_) {}
    static bool isRescale(const QCPRange &range)
    {
        return (range.lower == YAxisInfo::LOWER_RESCALE &&
                range.upper == YAxisInfo::UPPER_RESCALE);
    }
};

class YAxisToolUI : public QFrame, public Ui::YAxisTool
{
        Q_OBJECT

    public:
        explicit YAxisToolUI(QWidget *parent);
        void reset(const YAxisInfo &info);
    private:
};

/**
 * @class YAxisTool
 * @short Manages adjusting the Y-axis of Analyze stats graphs.
 *
 * @version 1.0
 * @author Hy Murveit
 */
class YAxisTool : public QDialog
{
        Q_OBJECT
    public:
        explicit YAxisTool(QWidget *ks);
        virtual ~YAxisTool() override = default;
        void reset(QObject *key, const YAxisInfo &info, bool isLeftAxis);
        void replot(bool isLeftAxis);
        const QCPAxis *getAxis()
        {
            return m_Info.axis;
        }

    protected:
        void closeEvent(QCloseEvent *event) override;
        void showEvent(QShowEvent *event) override;

    signals:
        void axisChanged(QObject *key, const YAxisInfo &info);
        void axisColorChanged(QObject *key, const YAxisInfo &info, const QColor &color);
        void leftAxisChanged(QCPAxis *axis);

    public slots:
        void updateValues(const double value);
        void slotClosed();

    private slots:
        void slotSaveChanges();

    private:
        void updateAutoValues();
        void updateLeftAxis();
        void updateToDefaults();
        void updateSpins();
        void updateColor();
        void computeAutoLimits();

        YAxisToolUI *ui { nullptr };
        QObject *m_Key { nullptr };
        YAxisInfo m_Info;
};
