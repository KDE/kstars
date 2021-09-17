/*
    SPDX-FileCopyrightText: 2003 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scriptfunction.h"

#include <QDebug>
#include <QStringList>

ScriptFunction::ScriptFunction(const QString &name, const QString &desc, bool clockfcn, const QString &at1,
                               const QString &an1, const QString &at2, const QString &an2, const QString &at3,
                               const QString &an3, const QString &at4, const QString &an4, const QString &at5,
                               const QString &an5, const QString &at6, const QString &an6)
    : INDIProp(QString())
{
    Name          = name;
    ClockFunction = clockfcn;

    ArgType[0]     = at1;
    ArgDBusType[0] = DBusType(at1);
    ArgName[0]     = an1;
    ArgType[1]     = at2;
    ArgDBusType[1] = DBusType(at2);
    ArgName[1]     = an2;
    ArgType[2]     = at3;
    ArgDBusType[2] = DBusType(at3);
    ArgName[2]     = an3;
    ArgType[3]     = at4;
    ArgDBusType[3] = DBusType(at4);
    ArgName[3]     = an4;
    ArgType[4]     = at5;
    ArgDBusType[4] = DBusType(at5);
    ArgName[4]     = an5;
    ArgType[5]     = at6;
    ArgDBusType[5] = DBusType(at6);
    ArgName[5]     = an6;

    //Construct a richtext description of the function
    QString nameStyle  = "<span style=\"font-family:monospace;font-weight:600\">%1</span>"; //bold
    QString typeStyle  = "<span style=\"font-family:monospace;color:#009d00\">%1</span>";   //green
    QString paramStyle = "<span style=\"font-family:monospace;color:#00007f\">%1</span>";   //blue

    Description = "<html><head><meta name=\"qrichtext\" content=\"1\" /></head>";
    Description += "<body style=\"font-size:11pt;font-family:sans\">";
    Description += "<p>" + nameStyle.arg(Name + '(');

    NumArgs = 0;
    if (!at1.isEmpty() && !an1.isEmpty())
    {
        Description += ' ' + typeStyle.arg(at1);
        Description += ' ' + paramStyle.arg(an1);
        NumArgs++;
    }

    if (!at2.isEmpty() && !an2.isEmpty())
    {
        Description += ", " + typeStyle.arg(at2);
        Description += ' ' + paramStyle.arg(an2);
        NumArgs++;
    }

    if (!at3.isEmpty() && !an3.isEmpty())
    {
        Description += ", " + typeStyle.arg(at3);
        Description += ' ' + paramStyle.arg(an3);
        NumArgs++;
    }

    if (!at4.isEmpty() && !an4.isEmpty())
    {
        Description += ", " + typeStyle.arg(at4);
        Description += ' ' + paramStyle.arg(an4);
        NumArgs++;
    }

    if (!at5.isEmpty() && !an5.isEmpty())
    {
        Description += ", " + typeStyle.arg(at5);
        Description += ' ' + paramStyle.arg(an5);
        NumArgs++;
    }

    if (!at6.isEmpty() && !an6.isEmpty())
    {
        Description += ", " + typeStyle.arg(at6);
        Description += ' ' + paramStyle.arg(an6);
        NumArgs++;
    }

    //Set Valid=false if there are arguments (indicates that this fcn's args must be filled in)
    Valid = true;
    if (NumArgs)
        Valid = false;

    //Finish writing function prototype
    if (NumArgs)
        Description += ' ';
    Description += nameStyle.arg(")") + "</p><p>";

    //Add description
    Description += desc;

    //Finish up
    Description += "</p></body></html>";
}

//Copy constructor
ScriptFunction::ScriptFunction(ScriptFunction *sf)
{
    Name          = sf->name();
    Description   = sf->description();
    ClockFunction = sf->isClockFunction();
    NumArgs       = sf->numArgs();
    INDIProp      = sf->INDIProperty();
    Valid         = sf->valid();

    for (unsigned int i = 0; i < 6; i++)
    {
        ArgType[i]     = sf->argType(i);
        ArgName[i]     = sf->argName(i);
        ArgDBusType[i] = sf->argDBusType(i);
        //ArgVal[i] .clear();
        // JM: Some default argument values might be passed from another object as well
        ArgVal[i] = sf->argVal(i);
    }
}

QString ScriptFunction::DBusType(const QString &type)
{
    if (type == QString("int"))
        return QString("int32");
    else if (type == QString("uint"))
        return QString("uint32");
    else if (type == QString("double"))
        return type;
    else if (type == QString("QString"))
        return QString("string");
    else if (type == QString("bool"))
        return QString("boolean");

    return nullptr;
}

QString ScriptFunction::prototype() const
{
    QString p = Name + '(';

    bool args(false);
    if (!ArgType[0].isEmpty() && !ArgName[0].isEmpty())
    {
        p += ' ' + ArgType[0];
        p += ' ' + ArgName[0];
        args = true; //assume that if any args are present, 1st arg is present
    }

    if (!ArgType[1].isEmpty() && !ArgName[1].isEmpty())
    {
        p += ", " + ArgType[1];
        p += ' ' + ArgName[1];
    }

    if (!ArgType[2].isEmpty() && !ArgName[2].isEmpty())
    {
        p += ", " + ArgType[2];
        p += ' ' + ArgName[2];
    }

    if (!ArgType[3].isEmpty() && !ArgName[3].isEmpty())
    {
        p += ", " + ArgType[3];
        p += ' ' + ArgName[3];
    }

    if (!ArgType[4].isEmpty() && !ArgName[4].isEmpty())
    {
        p += ", " + ArgType[4];
        p += ' ' + ArgName[4];
    }

    if (!ArgType[5].isEmpty() && !ArgName[5].isEmpty())
    {
        p += ", " + ArgType[5];
        p += ' ' + ArgName[5];
    }

    if (args)
        p += ' ';
    p += ')';

    return p;
}

QString ScriptFunction::scriptLine() const
{
    QString out(Name);
    unsigned int i = 0;

    while (i < 6 && !ArgName[i].isEmpty())
    {
        //Make sure strings are quoted
        QString value = ArgVal[i];
        if (ArgDBusType[i] == "string")
        {
            if (value.isEmpty())
            {
                value = "\"\"";
            }
            else
            {
                if (value.at(0) != '\"' && value.at(0) != '\'')
                {
                    value = '\"' + value;
                }
                if (value.right(1) != "\"" && value.right(1) != "\'")
                {
                    value = value + '\"';
                }
            }
        }

        // Write DBus style prototype compatible with dbus-send format
        out += ' ' + ArgDBusType[i] + ':' + value;
        ++i;
    }

    return out;
}
