/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2024 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "capturetypes.h"
#include <QObject>

namespace Ekos
{
class CaptureModuleState : public QObject
{
    Q_OBJECT
public:
    explicit CaptureModuleState(QObject *parent = nullptr): QObject{parent}{}


    // ////////////////////////////////////////////////////////////////////
    // shared attributes
    // ////////////////////////////////////////////////////////////////////
    bool forceInSeqAF(const QString opticaltrain) const
     {
        if (m_ForceInSeqAF.contains(opticaltrain))
            return m_ForceInSeqAF[opticaltrain];
        else
            // default value is off
            return false;
    }
    void setForceInSeqAF(bool value, const QString opticaltrain)
    {
        m_ForceInSeqAF[opticaltrain] = value;
    }

private:

    // ////////////////////////////////////////////////////////////////////
    // shared attributes
    // ////////////////////////////////////////////////////////////////////
    // User has requested an autofocus run as soon as possible
    QMap<QString, bool> m_ForceInSeqAF;

};
} // namespace Ekos

