/*  Apogee Control Library

Copyright (C) 2001-2006 Dave Mills  (rfactory@theriver.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
// ApnSerial_USB.cpp: implementation of the CApnSerial_USB class.
//
//////////////////////////////////////////////////////////////////////

#include "ApnSerial_USB.h"
#include "stdafx.h"
#include "apogee.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CApnSerial_USB::CApnSerial_USB()
{

}

CApnSerial_USB::~CApnSerial_USB()
{

}

bool CApnSerial_USB::InitPort( unsigned long  CamIdA, 
							   unsigned short CamIdB,
							   unsigned short SerialId )
{
	return true;
}

bool CApnSerial_USB::ClosePort()
{
	return true;
}

bool CApnSerial_USB::GetBaudRate( unsigned long *BaudRate )
{
	return true;
}

bool CApnSerial_USB::SetBaudRate( unsigned long BaudRate )
{
	return true;
}

bool CApnSerial_USB::GetFlowControl( Apn_SerialFlowControl *FlowControl )
{
	return true;
}

bool CApnSerial_USB::SetFlowControl( Apn_SerialFlowControl FlowControl )
{
	return true;
}

bool CApnSerial_USB::GetParity( Apn_SerialParity *Parity )
{
	return true;
}

bool CApnSerial_USB::SetParity( Apn_SerialParity Parity )
{
	return true;
}

bool CApnSerial_USB::Read( char	*ReadBuffer, unsigned short *ReadCount )
{
	return true;
}

bool CApnSerial_USB::Write( char *WriteBuffer, unsigned short WriteCount )
{
	return true;
}
