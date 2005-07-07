/***************************************************************************
                          scriptfunction.cpp  -  description
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

#include <kdebug.h>

#include "scriptfunction.h"


ScriptFunction::ScriptFunction( QString name, QString desc, bool clockfcn,
		QString at1, QString an1, QString at2, QString an2, QString at3, QString an3,
		QString at4, QString an4, QString at5, QString an5, QString at6, QString an6 ) : INDIProp("") {
	Name = name;
	ClockFunction = clockfcn;

	ArgType[0] = at1;  ArgName[0] = an1;
	ArgType[1] = at2;  ArgName[1] = an2;
	ArgType[2] = at3;  ArgName[2] = an3;
	ArgType[3] = at4;  ArgName[3] = an4;
	ArgType[4] = at5;  ArgName[4] = an5;
	ArgType[5] = at6;  ArgName[5] = an6;

	//Construct a richtext description of the function
	QString nameStyle  = "<span style=\"font-family:monospace;font-weight:600\">%1</span>";  //bold
	QString typeStyle  = "<span style=\"font-family:monospace;color:#009d00\">%1</span>";    //green
	QString paramStyle = "<span style=\"font-family:monospace;color:#00007f\">%1</span>";    //blue

	Description =  "<html><head><meta name=\"qrichtext\" content=\"1\" /></head>";
	Description += "<body style=\"font-size:11pt;font-family:sans\">";
	Description += "<p>" + nameStyle.arg( Name + "(" );

	NumArgs = 0;
	if ( ! at1.isEmpty() && ! an1.isEmpty() ) {
		Description += " " + typeStyle.arg( at1 );
		Description += " " + paramStyle.arg( an1 );
		NumArgs++;
	}

	if ( ! at2.isEmpty() && ! an2.isEmpty() ) {
		Description += ", " + typeStyle.arg( at2 );
		Description += " " + paramStyle.arg( an2 );
		NumArgs++;
	}

	if ( ! at3.isEmpty() && ! an3.isEmpty() ) {
		Description += ", " + typeStyle.arg( at3 );
		Description += " " + paramStyle.arg( an3 );
		NumArgs++;
	}

	if ( ! at4.isEmpty() && ! an4.isEmpty() ) {
		Description += ", " + typeStyle.arg( at4 );
		Description += " " + paramStyle.arg( an4 );
		NumArgs++;
	}

	if ( ! at5.isEmpty() && ! an5.isEmpty() ) {
		Description += ", " + typeStyle.arg( at5 );
		Description += " " + paramStyle.arg( an5 );
		NumArgs++;
	}

	if ( ! at6.isEmpty() && ! an6.isEmpty() ) {
		Description += ", " + typeStyle.arg( at6 );
		Description += " " + paramStyle.arg( an6 );
		NumArgs++;
	}

	//Set Valid=false if there are arguments (indicates that this fcn's args must be filled in)
	Valid = true;
	if ( NumArgs ) Valid = false;

	//Finish writing function prototype
	if ( NumArgs ) Description += " ";
	Description += nameStyle.arg( ")" ) + "</p><p>";

	//before adding description, replace any '%n' instances with the corresponding
	//argument name in color.  For now, assume that the %n's occur in order, with no skips.
	//Also assume that '%' is *only* used to indicate argument instances
	int narg = desc.contains( '%' );
	switch (narg ) {
		case 1:
			Description += desc.arg( paramStyle.arg( an1 ) );
			break;
		case 2:
			Description +=
					desc.arg( paramStyle.arg( an1 ) )
							.arg( paramStyle.arg( an2 ) );
			break;
		case 3:
			Description +=
					desc.arg( paramStyle.arg( an1 ) )
							.arg( paramStyle.arg( an2 ) )
							.arg( paramStyle.arg( an3 ) );
			break;
		case 4:
			Description +=
					desc.arg( paramStyle.arg( an1 ) )
							.arg( paramStyle.arg( an2 ) )
							.arg( paramStyle.arg( an3 ) )
							.arg( paramStyle.arg( an4 ) );
			break;
		case 5:
			Description +=
					desc.arg( paramStyle.arg( an1 ) )
							.arg( paramStyle.arg( an2 ) )
							.arg( paramStyle.arg( an3 ) )
							.arg( paramStyle.arg( an4 ) )
							.arg( paramStyle.arg( an5 ) );
			break;
		case 6:
			Description +=
					desc.arg( paramStyle.arg( an1 ) )
							.arg( paramStyle.arg( an2 ) )
							.arg( paramStyle.arg( an3 ) )
							.arg( paramStyle.arg( an4 ) )
							.arg( paramStyle.arg( an5 ) )
							.arg( paramStyle.arg( an6 ) );
			break;
		default:
			Description += desc;
			break;
	}

	//Finish up
	Description += "</p></body></html>";
}

//Copy constructor
ScriptFunction::ScriptFunction( ScriptFunction *sf )
{
	Name = sf->name();
	Description = sf->description();
	ClockFunction = sf->isClockFunction();
	NumArgs = sf->numArgs();
	INDIProp = sf->INDIProperty();
	Valid = sf->valid();

	for ( unsigned int i=0; i<6; i++ ) {
		ArgType[i] = sf->argType(i);
		ArgName[i] = sf->argName(i);
		ArgVal[i]  = "";
	}
}

ScriptFunction::~ScriptFunction()
{
}

QString ScriptFunction::prototype() const {
	QString p = Name + "(";

	bool args( false );
	if ( ! ArgType[0].isEmpty() && ! ArgName[0].isEmpty() ) {
		p += " " + ArgType[0];
		p += " " + ArgName[0];
		args = true; //assume that if any args are present, 1st arg is present
	}

	if ( ! ArgType[1].isEmpty() && ! ArgName[1].isEmpty() ) {
		p += ", " + ArgType[1];
		p += " " + ArgName[1];
	}

	if ( ! ArgType[2].isEmpty() && ! ArgName[2].isEmpty() ) {
		p += ", " + ArgType[2];
		p += " " + ArgName[2];
	}

	if ( ! ArgType[3].isEmpty() && ! ArgName[3].isEmpty() ) {
		p += ", " + ArgType[3];
		p += " " + ArgName[3];
	}

	if ( ! ArgType[4].isEmpty() && ! ArgName[4].isEmpty() ) {
		p += ", " + ArgType[4];
		p += " " + ArgName[4];
	}

	if ( ! ArgType[5].isEmpty() && ! ArgName[5].isEmpty() ) {
		p += ", " + ArgType[5];
		p += " " + ArgName[5];
	}

	if ( args ) p += " ";
	p += ")";

	return p;
}

QString ScriptFunction::scriptLine() const {
	QString out( Name );
	unsigned int i=0;
	while ( ! ArgName[i].isEmpty() && i < 6 ) {
		// Wrap arg in quotes if it contains a space
		if ( ArgVal[i].contains(" ") ) {
			out += " \"" + ArgVal[i] + "\"";
		} else {
			out += " " + ArgVal[i];
		}
		++i;
	}

	return out;
}
