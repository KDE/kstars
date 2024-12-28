/*
    SPDX-FileCopyrightText: 2024 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QFrame>

class KConfigDialog;

namespace Ekos
{

class OpsFocusBase : public QFrame
{
    Q_OBJECT
public:
    OpsFocusBase(const QString name);

    const QString &dialogName() const
    {
        return m_dialogName;
    }


signals:
    void settingsUpdated();

protected:
    KConfigDialog *m_ConfigDialog { nullptr };

private:
    QString m_dialogName;

};
} // namespace Ekos
