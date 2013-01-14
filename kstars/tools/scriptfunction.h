/***************************************************************************
                          scriptfunction.h  -  description
                             -------------------
    begin                : Thu Apr 17 2003
    copyright            : (C) 2003 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SCRIPTFUNCTION_H_
#define SCRIPTFUNCTION_H_

#include <qstring.h>

/**
 *
 * Jason Harris
 **/
class ScriptFunction
{
public:
    ScriptFunction( const QString &name, const QString &desc,
                    bool clockfcn=false,
                    const QString &at1=QString(), const QString &an1=QString(),
                    const QString &at2=QString(), const QString &an2=QString(),
                    const QString &at3=QString(), const QString &an3=QString(),
                    const QString &at4=QString(), const QString &an4=QString(),
                    const QString &at5=QString(), const QString &an5=QString(),
                    const QString &at6=QString(), const QString &an6=QString() );
    explicit ScriptFunction( ScriptFunction *sf );
    ~ScriptFunction();

    QString name() const { return Name; }
    QString prototype() const;
    QString description() const { return Description; }
    QString argType( unsigned int n ) const { return ArgType[n]; }
    QString argName( unsigned int n ) const { return ArgName[n]; }
    QString argVal( unsigned int n ) const { return ArgVal[n]; }
    QString argDBusType (unsigned int n) const { return ArgDBusType[n];}

    void setValid( bool b ) { Valid = b; }
    bool valid() const { return Valid; }

    void setClockFunction( bool b=true ) { ClockFunction = b; }
    bool isClockFunction() const { return ClockFunction; }

    void setArg( unsigned int n, QString newVal ) { ArgVal[n] = newVal; }
    bool checkArgs();
    int numArgs() const { return NumArgs; }

    QString scriptLine() const;

    void    setINDIProperty(QString prop) { INDIProp = prop; }
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

#endif
