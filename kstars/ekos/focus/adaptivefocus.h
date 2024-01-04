/*
    SPDX-FileCopyrightText: 2023 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "focus.h"

namespace Ekos
{

// AdaptiveFocus is setup as a friend class of Focus because it is tightly coupled to Focus relying  on many
// members of Focus, both variables and functions.
//
// adaptiveFocus is signalled (by capture). Check each adaptive dimension (temp, alt) to determine whether a focus move is required.
// Total the movements from each dimension and, if required, adjust focus.
//
// Use the adaptive focus parameters associated with the current filter unless that has a lock filter, in which case use the lock filter.
// Use the last good autofocus run as the reference that provides the focus position, temperature
// and altitude. Adaptive focus then calculates the proposed position based on the current temperature and altitude
// and calculates how to move from the current position to the proposed position - note there may be an overscan involved.
//
// To keep track of the movements the data are passed to Analyze which allows the user to review everything. Analyze displays the
// deltas from the previous adaptive focus run for temperature and altitude to the next adaptive focus run.
//
// Calculations are performed as doubles and rounded to int for the final movement.
//
// A complication is that not all focusers move exactly to the requested position - 'bad boy' focusers. E.g. a request to
// goto position X results in the focuser moving to X, X +/- 1 or X +/- 2. These position errors need to be managed. There
// are 2 aspects to this; before an Adaptive Focus iteration, the focuser may be "off" where it should be. This could be
// because the focuser is a "bad boy" so it moved to the correct position +/- 1 or 2 ticks; this is the Last Pos Error.
// Adaptive Focus will attempt to correct this error even if there are no temperature or altitude changes (providing the
// movement is above the user defined minimum movement.
// When the Adaptive Focus iteration runs, the focuser may again experience a Pos Error.
// These Position Errors are passed to Analyze to breakdown each focuser movement.
//
// Proposed Position = Ref Position (from last Autofocus) + adjustment for temperature + adjustment for altitude
//
// Accounting since last Adaptive Focus
// Last Pos Error Reversal = Last Iteration Proposed Move - Focuser position at start of Adaptive Focus iteration
// This Pos Error = Focuser position at end of Adaptive Focus iteration - Proposed Move
// End Position = Start Position + temperature adjustment + altitude adjustment + Last Pos Error Reversal + This Pos Error
//
// Example: Last Autofocus Run, Pos=100 0C 70Alt. Parameters: 10ticks/Degree C, 1tick/Degree Alt
// 1st Adaptive Focus: 0.5C 71Alt
//                     Proposed Position = 100 + 5 + 1                = 106
//                     Actual Position                                = 105 (focuser movement error)
//                     Accounting: Start Position                     = 100
//                                 Temperature Adjustment             = 5
//                                 Altitude Adjustment                = 1
//                                 Last Pos Error Reversal            = 0
//                                 This Pos Error                     = -1
//                                 End Position = 100 + 5 + 1 + 0 - 1 = 105
//
// 2nd Adaptive Focus: 0.75C 72Alt
//                     Proposed Position = 100 + 7.5 + 2              = 110
//                     Actual Position                                = 112 (focuser movement error)
//                     Accounting: Start Position                     = 105
//                                 Temperature Adjustment             = 2.5
//                                 Altitude Adjustment                = 1
//                                 Last Pos Error Reversal            = 1
//                                 This Pos Error                     = 2
//                                 End Position = 100 + 5 + 1 + 0 - 1 = 112
//
// 3rd Adaptive Focus: 0.25C 72Alt
//                     Proposed Position = 100 + 2.5 + 2              = 105
//                     Actual Position                                = 105 (no focuser movement error)
//                     Accounting: Start Position                     = 112
//                                 Temperature Adjustment             = -5
//                                 Altitude Adjustment                = 0
//                                 Last Pos Error Reversal            = -2
//                                 This Pos Error                     =
//                                 End Position = 100 + 5 + 1 + 0 - 1 = 105
//
// Adaptive Focus moves the focuser between Autofocus runs.
// Adapt Start Pos adjusts the start position of an autofocus run based on Adaptive Focus settings
// The start position uses the last successful AF run for the active filter and adapts that position
// based on the temperature and altitude delta between now and when the last successful AF run happened
// Only enabled for LINEAR 1 PASS

class AdaptiveFocus
{
    public:

        AdaptiveFocus(Focus* _focus);
        ~AdaptiveFocus();

        /**
         * @brief runAdaptiveFocus runs the next iteration of Adaptive Focus
         * @param current focuser position
         * @param active filter
         */
        void runAdaptiveFocus(const int currentPosition, const QString &filter);

        /**
         * @brief Perform admin functions on Adaptive Focus to inform other modules of
         * @param current focuser position
         * @param success (or not)
         * @param focuserMoved (or not)
         */
        void adaptiveFocusAdmin(const int currentPosition, const bool success, const bool focuserMoved);

        /**
         * @brief Reset the variables used by Adaptive Focus
         */
        void resetAdaptiveFocusCounters();

        /**
         * @brief Return whether Adaptive Focus is running
         * @return inAdaptiveFocus
         */
        bool inAdaptiveFocus()
        {
            return m_inAdaptiveFocus;
        }

        /**
         * @brief Set the value of inAdaptiveFocus
         * @param value
         */
        void setInAdaptiveFocus(bool value);

        /**
         * @brief adapt the start position based on temperature and altitude
         * @param position is the unadapted focuser position
         * @param AFfilter is the filter to run autofocus on
         * @return adapted start position
         */
        int adaptStartPosition(int position, QString &AFfilter);

    private:

        /**
         * @brief Get the Adaptive Filter to use
         * @param active filter
         * @return filter
         */
        QString getAdaptiveFilter(const QString filter);

        /**
         * @brief Get filter offset between active and adaptive filters
         * @param Active filter
         * @param Adaptive filter
         * @return offset
         */
        int getAdaptiveFilterOffset(const QString &activeFilter, const QString &adaptiveFilter);

        Focus *m_focus { nullptr };

        // Focuser is processing an adaptive focus request
        bool m_inAdaptiveFocus { false };

        // m_ThisAdaptiveFocus* variables hold values for the current Adaptive Focus run
        int     m_ThisAdaptiveFocusStartPos         { INVALID_VALUE };
        double  m_ThisAdaptiveFocusTemperature      { INVALID_VALUE };
        double  m_ThisAdaptiveFocusAlt              { INVALID_VALUE };
        double  m_ThisAdaptiveFocusTempTicks        { INVALID_VALUE };
        double  m_ThisAdaptiveFocusAltTicks         { INVALID_VALUE };
        int     m_ThisAdaptiveFocusRoundingError    { 0 };
        // m_LastAdaptiveFocus* variables hold values for the previous Adaptive Focus run
        QString m_LastAdaptiveFilter                { NULL_FILTER };
        double  m_LastAdaptiveFocusTemperature      { INVALID_VALUE };
        double  m_LastAdaptiveFocusAlt              { INVALID_VALUE };
        int     m_LastAdaptiveFocusPosErrorReversal { INVALID_VALUE };

        int     m_AdaptiveFocusPositionReq          { INVALID_VALUE };
        int     m_AdaptiveTotalMove                 { 0 };
};

}
