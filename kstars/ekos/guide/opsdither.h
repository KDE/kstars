/*  Ekos Options
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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
