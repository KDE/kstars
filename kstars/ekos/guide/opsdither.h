/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_opsdither.h"
#include "guide.h"
#include "ksutils.h"
#include "kspaths.h"

class KConfigDialog;

namespace Ekos
{
/**
 * @class OpsDither
 *
 * Enables the user to set guide options
 *
 * @author Jasem Mutlaq
 */
class OpsDither : public QFrame, public Ui::OpsDither
{
        Q_OBJECT

    public:
        explicit OpsDither();
        virtual ~OpsDither() override = default;

    signals:
        void settingsUpdated();

    private:
        KConfigDialog *m_ConfigDialog { nullptr };

};
}
