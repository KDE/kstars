/*  Ekos state machine for refocusing
    SPDX-FileCopyrightText: 2022 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "refocusstate.h"

namespace Ekos
{

int RefocusState::getRefocusEveryNTimerElapsedSec()
{
    /* If timer isn't valid, consider there is no focus to be done, that is, that focus was just done */
    return m_refocusEveryNTimer.isValid() ? static_cast<int>(m_refocusEveryNTimer.elapsed() / 1000) : 0;
}

void RefocusState::decreaseInSequenceFocusCounter()
{
    if (inSequenceFocusCounter > 0)
        --inSequenceFocusCounter;
}






} // namespace
