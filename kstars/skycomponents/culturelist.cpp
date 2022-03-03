/*
    SPDX-FileCopyrightText: 2008 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "culturelist.h"
#include "ksfilereader.h"
#include "Options.h"

CultureList::CultureList()
{
    KSFileReader fileReader;
    if (!fileReader.open("cnames.dat"))
        return;

    while (fileReader.hasMoreLines())
    {
        QString line = fileReader.readLine();
        if (line.size() < 1)
            continue;

        QChar mode = line.at(0);
        if (mode == 'C')
            m_CultureList << line.mid(2).trimmed();
    }

    m_CurrentCulture = m_CultureList.at(0);
    m_CultureList.sort();
    for (int i = 0; i < m_CultureList.size(); ++i)
    {
        if (m_CultureList.at(i) == Options::skyCulture())
        {
            m_CurrentCulture = m_CultureList.at(i);
        }
    }
}

void CultureList::setCurrent(QString newName)
{
    m_CurrentCulture = newName;
}

QString CultureList::getName(int index) const
{
    return m_CultureList.value(index);
}
