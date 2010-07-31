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

#ifndef SCRIPTFUNCTION_H
#define SCRIPTFUNCTION_H

#include <tqstring.h>

/**
 *
 * Jason Harris
 **/
class ScriptFunction
{
public:
	ScriptFunction( TQString name, TQString desc, bool clockfcn=false,
			TQString at1="", TQString an1="",
			TQString at2="", TQString an2="",
			TQString at3="", TQString an3="",
			TQString at4="", TQString an4="",
			TQString at5="", TQString an5="",
			TQString at6="", TQString an6=""
	);
	ScriptFunction( ScriptFunction *sf );
	~ScriptFunction();

	TQString name() const { return Name; }
	TQString prototype() const;
	TQString description() const { return Description; }
	TQString argType( unsigned int n ) const { return ArgType[n]; }
	TQString argName( unsigned int n ) const { return ArgName[n]; }
	TQString argVal( unsigned int n ) const { return ArgVal[n]; }

	void setValid( bool b ) { Valid = b; }
	bool valid() const { return Valid; }

	void setClockFunction( bool b=true ) { ClockFunction = b; }
	bool isClockFunction() const { return ClockFunction; }

	void setArg( unsigned int n, TQString newVal ) { ArgVal[n] = newVal; }
	bool checkArgs();
	unsigned int numArgs() const { return NumArgs; }

	TQString scriptLine() const;
	
	void    setINDIProperty(TQString prop) { INDIProp = prop; }
	TQString INDIProperty() const { return INDIProp; }

private:
	TQString Name, Description;
	TQString ArgType[6];
	TQString ArgName[6];
	TQString ArgVal[6];
	TQString INDIProp;
	bool Valid, ClockFunction;
	unsigned int NumArgs;
};

#endif
