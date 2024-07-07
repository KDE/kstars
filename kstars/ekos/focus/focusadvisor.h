/*
    SPDX-FileCopyrightText: 2024 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "focus.h"
#include "ui_focusadvisor.h"
#include "ui_focusadvisorhelp.h"

namespace Ekos
{

// FocusAdvisor is setup as a friend class of Focus because it is tightly coupled to Focus relying on many
// members of Focus, both variables and functions.
//
// FocusAdvisor is designed to assist the user with focus. Its main purpose is to setup Focus for new users
// who may not be familiar with the many options that the Focus module offers. It will attempt to find reasonable focus
// rather than the "best" setup which will require some trial and error by the user.
// The main features are:
// - Parameter setup -   FocusAdvisor recommends a setup using well tried combinations including Linear 1 Pass
//                       Hperbolic curve fitting and HFR measure. A help dialog displays the current and proposed parameter
//                       settings.
// - Find stars -        Searches the range of travel of the focuser to find some stars. If the user knows roughtly where they
//                       are then set the focuser to this position and Find Stars will search around this area first.
// - Coarse adjustment - This tool will attempt to adjust parameters (step size and backlash) in order get a reasonable V-curve
//                       without curve fitting. This is a preparatory step before running Autofocus to make sure parameters are
//                       good enough to allow Autofocus to complete.
// - Fine adjustment -   This tool runs Autofocus and adjusts step size and backlash by analysing the results of Autofocus. If
//                       Autofocus is not within its tolerance paraneters it will keep adjusting and retrying.
//

class FocusAdvisor : public QDialog, public Ui::focusAdvisorDialog
{
        Q_OBJECT

    public:

        FocusAdvisor(QWidget *parent = nullptr);
        ~FocusAdvisor();

        typedef enum
        {
            Idle,
            UpdatingParameters,
            FindingStars,
            CoarseAdjustments,
            FineAdjustments
        } Stage;

        /**
         * @brief Initialise the Focus Advisor Control
         */
        void initControl();

        /**
         * @brief Focus Advisor Control function
         */
        void control();

        /**
         * @brief Focus Advisor analysis of Autofocus run
         */
        bool analyseAF();

        /**
         * @brief Update parameters based on Focus Advisor recommendations
         */
        void updateParams();

        /**
         * @brief Setup the Focus Advisor recommendations
         * @param Optical Train name
         */
        void setupParams(const QString &OTName);

        /**
         * @brief Return whether Focus Advisor is running
         * @return inFocusAdvisor
         */
        bool inFocusAdvisor()
        {
            return m_inFocusAdvisor;
        }

        /**
         * @brief Set the value of m_inFocusAdvisor
         * @param value
         */
        void setInFocusAdvisor(bool value);

        /**
         * @brief Return prefix to use on saved off focus frame
         * @return prefix
         */
        QString getFocusFramePrefix();

        /**
         * @brief set the dialog buttons
         * @param Focus Advisor running (or not)
         */
        void setButtons(const bool running);

        /**
         * @brief Reset Focus Advisor
         */
        Q_INVOKABLE void reset();

        /**
         * @brief Launch the focus advisor algorithm
         */
        Q_INVOKABLE bool start();

        /**
         * @brief Stop the focus advisor algorithm
         */
        Q_INVOKABLE void stop();


    private:

        /**
         * @brief setup the dialog UI
         */
        void processUI();

        /**
         * @brief setup the help dialog table
         */
        void setupHelpTable();

        /**
         * @brief Focus Advisor help window
         */
        void help();

        /**
         * @brief Returns whether Focus Advisor can run with the currently set parameters
         */
        bool canFocusAdvisorRun();

        /**
         * @brief addSectionToHelpTable
         * @param last used row in table
         * @param section name
         */
        void addSectionToHelpTable(int &row, const QString &section);

        /**
         * @brief addParamToHelpTable
         * @param last used row in table
         * @param parameter name
         * @param currentValue of parameter
         * @param newValue of parameter
         */
        void addParamToHelpTable(int &row, const QString &parameter, const QString &currentValue, const QString &newValue);

        /**
         * @brief resizeHelpDialog based on contents
         */
        void resizeHelpDialog();

        /**
         * @brief return text for flag for display in help dialog
         * @param flag
         */
        QString boolText(const bool flag);

        /**
         * @brief return whether stars exist in the latest focus frame
         * @return stars found
         */
        bool starsFound();

        /**
         * @brief Process the passed in focus parameter
         * @param defaultValue of the parameter
         * @param Parameter map
         * @param widget associated with the parameter
         * @param text to display in help dialog for this parameter
         */
        template <typename T>
        void processParam(T defaultValue, int &row, QVariantMap &map, const QWidget *widget, const QString text)
        {
            const QSpinBox *sb = dynamic_cast<const QSpinBox*>(widget);
            const QDoubleSpinBox *dsb = dynamic_cast<const QDoubleSpinBox*>(widget);
            const QCheckBox *chb = dynamic_cast<const QCheckBox*>(widget);
            const QComboBox *cb = dynamic_cast<const QComboBox*>(widget);
            const QRadioButton *rb = dynamic_cast<const QRadioButton*>(widget);
            const QGroupBox *gb = dynamic_cast<const QGroupBox*>(widget);
            int dp = 0;
            QString currentText, suffix;

            if (sb)
            {
                suffix = sb->suffix();
                currentText = QString("%1%2").arg(sb->value()).arg(suffix);
            }
            else if (dsb)
            {
                dp = dsb->decimals();
                suffix = dsb->suffix();
                currentText = QString("%1%2").arg(dsb->value(), 0, 'f', dp).arg(suffix);
            }
            else if (chb)
                currentText = QString("%1").arg(boolText(chb->isChecked()));
            else if (cb)
                currentText = cb->currentText();
            else if (rb)
                currentText = QString("%1").arg(boolText(rb->isChecked()));
            else if (gb)
                currentText = QString("%1").arg(boolText(gb->isChecked()));
            else
            {
                qCDebug(KSTARS_EKOS_FOCUS) << "Bad parameter widget" << widget->objectName() << "passed to" << __FUNCTION__;
                return;
            }

            QString newText;

            map.insert(widget->objectName(), defaultValue);

            if constexpr(std::is_same_v<T, int>)
                newText = QString("%1%2").arg(defaultValue).arg(suffix);
            else if constexpr(std::is_same_v<T, double>)
                newText = QString("%1%2").arg(defaultValue, 0, 'f', dp).arg(suffix);
            else if constexpr(std::is_same_v<T, bool>)
                newText = QString("%1").arg(boolText(defaultValue));
            else if constexpr(std::is_same_v<T, QString>)
                newText = defaultValue;
            else
            {
                qCDebug(KSTARS_EKOS_FOCUS) << "Bad template parameter" << defaultValue << "passed to" << __FUNCTION__;
                return;
            }
            addParamToHelpTable(row, text, currentText, newText);
        }

        /**
         * @brief Look at similar Optical Trains to get parameters
         * @param Optical Train name
         * @return Parameter map
         */
        QVariantMap getOTDefaults(const QString &OTName);

        /**
         * @brief returns whether the optical train telescope has a central obstruction
         * @param scopeType is the type of telescope
         * @return whether scope has an obstruction
         */
        bool scopeHasObstruction(const QString &scopeType);

        /**
         * @brief Initialise the find stars algorithm
         * @param startPos is the starting position to use
         */
        void initFindStars(const int startPos);

        /**
         * @brief Find stars algorithm to scan Focuser movement range to find stars
         */
        void findStars();

        /**
         * @brief Initialise the pre-Autofocus adjustment algorithm
         * @param startPos is the starting position to use
         */
        void initPreAFAdj(const int startPos);

        /**
         * @brief Algorithm for coarse parameter adjustment prior to running Autofocus
         */
        void preAFAdj();

        /**
         * @brief Check the Max / Min star measure ratio against the limit
         * @param limit to check against
         * @param maxMinRatio
         * @return Check passed or not
         */
        bool maxMinRatioOK(const double limit, const double maxMinRatio);

        /**
         * @brief Return min position where stars exist
         */
        int minStarsLimit();

        /**
         * @brief Return max position where stars exist
         */
        int maxStarsLimit();

        /**
         * @brief Initialise the Autofocus adjustment algorithm
         * @param startPos is the starting position to use
         * @param retryOverscan calculation
         */
        void initAFAdj(const int startPos, const bool retryOverscan);

        /**
         * @brief Start Autofocus
         * @param startPos is the starting position to use
         */
        void startAF(const int startPos);

        /**
         * @brief Abort Focus Advisor
         * @param failCode
         * @param message msg
         */
        void abort(const QString &msg);

        /**
         * @brief Abort Focus Advisor
         * @param whether Autofocus was run
         * @param message msg
         */
        void complete(const bool autofocus, const QString &msg);

        /**
         * @brief Reset settings temporarily adjusted by Focus Advisor
         * @param success of Focus Advisor
         */
        void resetSavedSettings(const bool success);

        Focus *m_focus { nullptr };
        QVariantMap m_map;

        // Help popup
        std::unique_ptr<Ui::focusAdvisorHelpDialog> m_helpUI;
        QPointer<QDialog> m_helpDialog;

        // Buttons
        QPushButton *m_runButton;
        QPushButton *m_stopButton;
        QPushButton *m_helpButton;

        // Control bit for Focus Advisor
        bool m_inFocusAdvisor = false;

        // Initial parameter settings restore in case Focus Advisor fails
        int m_initialStepSize = -1;
        int m_initialBacklash = -1;
        int m_initialAFOverscan = -1;
        bool m_initialUseWeights = false;
        bool m_initialScanStartPos = false;

        // Vectors to use to analyse position/measure results
        QVector<int> m_position;
        QVector<double> m_measure;

        // Control bits for Focus Advisor algorithm stages
        bool m_inFindStars = false;
        bool m_inPreAFAdj = false;
        bool m_inAFAdj = false;

        // Find Stars Algorithm
        bool m_findStarsIn = false;
        bool m_findStarsMaxBound = false;
        bool m_findStarsMinBound = false;
        bool m_findStarsRange = false;
        bool m_findStarsRangeIn = false;
        bool m_findStarsFindInEdge = false;
        int m_findStarsSector = 0;
        int m_findStarsJumpsInSector = 0;
        int m_findStarsRunNum = 0;
        int m_findStarsInRange = -1;
        int m_findStarsOutRange = -1;

        // Pre-Autofocus Algorithm
        int m_preAFInner = 0;
        bool m_preAFNoStarsOut = false;
        bool m_preAFNoStarsIn = false;
        int m_preAFRunNum = 0;
        bool m_preAFMaxRange = false;

        // Find Stars & Pre-Autofocus Algorithms
        int m_jumpsToGo = 0;
        int m_jumpSize = 0;

        // Autofocus algorithm analysis
        bool m_minOverscan = false;
        bool m_runAgainR2 = false;
        bool m_nearFocus = false;

        // Help table Cols
        typedef enum
        {
            HELP_PARAMETER = 0,
            HELP_CURRENT_VALUE,
            HELP_NEW_VALUE,
            HELP_MAX_COLS
        } HelpColID;

      signals:
        void newMessage(QString);
        void newStage(Stage);
};

}
