// ApnSerial_USB.cpp: implementation of the CApnSerial_USB class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "apogee.h"
#include "ApnSerial_USB.h"

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
