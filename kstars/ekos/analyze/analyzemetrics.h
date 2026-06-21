/*
    SPDX-FileCopyrightText: 2026 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QByteArray>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <cstddef>
#include <deque>
#include <optional>

namespace Ekos
{

/** Constant-size live telemetry collected by the Analyze module. */
class AnalyzeMetrics
{
    public:
        explicit AnalyzeMetrics(const QString &version = QString());

        void reset(qint64 startTimeMs);
        void clear();

        void captureStarting(qint64 timeMs, double exposureSeconds, const QString &filter);
        void captureComplete(qint64 timeMs, double exposureSeconds, const QString &filter,
                             double hfr, int stars, double median, double eccentricity);
        void captureAborted(qint64 timeMs, const QString &filter);

        void autofocusStarting(qint64 timeMs, const QString &filter);
        void autofocusComplete(qint64 timeMs, const QString &filter, double position);
        void autofocusAborted(qint64 timeMs, const QString &filter);
        void adaptiveFocusComplete(qint64 timeMs, const QString &filter, int adjustment,
                                   int position, bool moved);
        void temperature(double value);

        void guideState(const QString &state);
        void guideStats(qint64 timeMs, double raError, double decError, int raPulse,
                        int decPulse, double snr, double skyBackground, int stars);

        void alignState(const QString &state);
        void mountState(const QString &state);
        void mountCoordinates(double ra, double dec, double azimuth, double altitude,
                              int pierSide, double hourAngle);
        void meridianFlipState(const QString &state);

        void schedulerJobStarted(qint64 timeMs, const QString &jobName);
        void schedulerJobEnded(qint64 timeMs);
        void targetDistance(double arcseconds);

        QByteArray render(qint64 nowMs) const;

    private:
        class RollingRms
        {
            public:
                double add(double x, double y);
                void reset();

            private:
                static constexpr std::size_t WINDOW_SIZE = 40;
                std::deque<double> m_x;
                std::deque<double> m_y;
                double m_xSum { 0 };
                double m_ySum { 0 };
                double m_xSquares { 0 };
                double m_ySquares { 0 };
        };

        struct CaptureTotals
        {
            quint64 completed { 0 };
            quint64 aborted { 0 };
            double exposureSeconds { 0 };
        };

        struct FocusTotals
        {
            quint64 completed { 0 };
            quint64 aborted { 0 };
        };

        static constexpr int MAX_FILTERS = 32;

        QString boundedFilter(const QString &filter);
        void ensureSession(qint64 timeMs);
        static QString normalizedState(const QString &state);

        QString m_version;
        std::optional<qint64> m_sessionStartMs;

        bool m_captureActive { false };
        std::optional<qint64> m_captureStartMs;
        QString m_captureFilter;
        double m_captureExposureSeconds { 0 };
        std::optional<double> m_captureDurationSeconds;
        std::optional<double> m_captureHfr;
        std::optional<double> m_captureEccentricity;
        std::optional<double> m_captureStars;
        std::optional<double> m_captureMedian;
        std::optional<double> m_targetDistance;
        QMap<QString, CaptureTotals> m_captureTotals;

        bool m_autofocusActive { false };
        std::optional<qint64> m_autofocusStartMs;
        std::optional<double> m_autofocusDurationSeconds;
        std::optional<double> m_focusPosition;
        std::optional<double> m_adaptiveAdjustment;
        std::optional<double> m_temperature;
        QMap<QString, FocusTotals> m_focusTotals;
        QMap<QString, QPair<quint64, quint64>> m_adaptiveFocusTotals;

        QString m_guideState { QStringLiteral("idle") };
        QMap<QString, quint64> m_guideTransitions;
        std::optional<qint64> m_lastGuideStatsMs;
        std::optional<double> m_guideRaError;
        std::optional<double> m_guideDecError;
        std::optional<double> m_guideDrift;
        std::optional<double> m_guideRms;
        std::optional<double> m_captureRms;
        std::optional<double> m_guideRaPulse;
        std::optional<double> m_guideDecPulse;
        std::optional<double> m_guideSnr;
        std::optional<double> m_guideSkyBackground;
        std::optional<double> m_guideStars;
        RollingRms m_guideRmsFilter;
        RollingRms m_captureRmsFilter;

        QString m_alignState { QStringLiteral("idle") };
        quint64 m_alignCompleted { 0 };
        quint64 m_alignFailed { 0 };
        quint64 m_alignAborted { 0 };

        QString m_mountState { QStringLiteral("idle") };
        std::optional<double> m_mountRa;
        std::optional<double> m_mountDec;
        std::optional<double> m_mountAzimuth;
        std::optional<double> m_mountAltitude;
        std::optional<double> m_mountHourAngle;
        QString m_pierSide { QStringLiteral("unknown") };

        QString m_flipState { QStringLiteral("none") };
        quint64 m_flipCompleted { 0 };
        quint64 m_flipFailed { 0 };

        bool m_schedulerActive { false };
        QString m_schedulerJob;
        std::optional<qint64> m_schedulerStartMs;
        std::optional<double> m_schedulerDurationSeconds;
        quint64 m_schedulerStarted { 0 };
        quint64 m_schedulerEnded { 0 };
};

} // namespace Ekos
