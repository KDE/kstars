/*
    SPDX-FileCopyrightText: 2009 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "oal/oal.h"

#include <QString>

/// FIXME: why not just use a QHash?

/**
 * @class OAL::Observer
 *
 * Information on user who created or contributed to the observation.
 */
class OAL::Observer
{
    public:
        QString id() const
        {
            return m_Id;
        }
        QString name() const
        {
            return m_Name;
        }
        QString surname() const
        {
            return m_Surname;
        }
        QString contact() const
        {
            return m_Contact;
        }
        Observer(QString _id, QString _name = "", QString _surname = "", QString _contact = "")
        {
            setObserver(_id, _name, _surname, _contact);
        }
        void setObserver(QString _id, QString _name = "", QString _surname = "", QString _contact = "");

    private:
        QString m_Name, m_Surname, m_Contact, m_Id;
};
