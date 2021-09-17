/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "oal/observer.h"

void OAL::Observer::setObserver(QString _id, QString _name, QString _surname, QString _contact)
{
    m_Id      = _id;
    m_Name    = _name;
    m_Surname = _surname;
    m_Contact = _contact;
}
