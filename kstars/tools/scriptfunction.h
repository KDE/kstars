/*
    SPDX-FileCopyrightText: 2003 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>

/**
 *
 * Jason Harris
 **/
class ScriptFunction
{
  public:
    ScriptFunction(const QString &name, const QString &desc, bool clockfcn = false, const QString &at1 = QString(),
                   const QString &an1 = QString(), const QString &at2 = QString(), const QString &an2 = QString(),
                   const QString &at3 = QString(), const QString &an3 = QString(), const QString &at4 = QString(),
                   const QString &an4 = QString(), const QString &at5 = QString(), const QString &an5 = QString(),
                   const QString &at6 = QString(), const QString &an6 = QString());
    explicit ScriptFunction(ScriptFunction *sf);
    ~ScriptFunction() = default;

    QString name() const { return Name; }
    QString prototype() const;
    QString description() const { return Description; }
    QString argType(unsigned int n) const { return ArgType[n]; }
    QString argName(unsigned int n) const { return ArgName[n]; }
    QString argVal(unsigned int n) const { return ArgVal[n]; }
    QString argDBusType(unsigned int n) const { return ArgDBusType[n]; }

    void setValid(bool b) { Valid = b; }
    bool valid() const { return Valid; }

    void setClockFunction(bool b = true) { ClockFunction = b; }
    bool isClockFunction() const { return ClockFunction; }

    void setArg(unsigned int n, QString newVal) { ArgVal[n] = newVal; }
    bool checkArgs();
    int numArgs() const { return NumArgs; }

    QString scriptLine() const;

    void setINDIProperty(QString prop) { INDIProp = prop; }
    QString INDIProperty() const { return INDIProp; }
    QString DBusType(const QString &type);

  private:
    QString Name, Description;
    QString ArgType[6];
    QString ArgDBusType[6];
    QString ArgName[6];
    QString ArgVal[6];
    QString INDIProp;
    bool Valid, ClockFunction;
    int NumArgs;
};
