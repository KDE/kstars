// ApnSerial_NET.cpp: implementation of the CApnSerial_NET class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "apogee.h"
#include "ApnSerial_NET.h"

#include "ApogeeNet.h"
#include "ApogeeNetErr.h"


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CApnSerial_NET::CApnSerial_NET()
{
	m_SerialId = -1;
}

CApnSerial_NET::~CApnSerial_NET()
{

}

bool CApnSerial_NET::InitPort( unsigned long  CamIdA, 
							   unsigned short CamIdB,
							   unsigned short SerialId )
{
	char			Hostname[25];
	BYTE			ipAddr[4];


	ipAddr[0] = (BYTE)(CamIdA & 0xFF);
	ipAddr[1] = (BYTE)((CamIdA >> 8) & 0xFF);
	ipAddr[2] = (BYTE)((CamIdA >> 16) & 0xFF);
	ipAddr[3] = (BYTE)((CamIdA >> 24) & 0xFF);

	sprintf( Hostname, "%u.%u.%u.%u", ipAddr[3], ipAddr[2], ipAddr[1], ipAddr[0] );

	if ( m_SerialId != -1 )
	{
		return false;
	}

	if ( (SerialId != 0) && (SerialId != 1) )
	{
		return false;
	}

	if ( ApnNetStartSockets() != APNET_SUCCESS )
		return false;

	if ( ApnNetSerialPortOpen( Hostname, CamIdB, SerialId ) != APNET_SUCCESS )
		return false;

	m_SerialId = SerialId;

	return true;
}

bool CApnSerial_NET::ClosePort()
{
	if ( m_SerialId == -1 )
		return false;

	// just close the port and not care whether it was successful.  if it was,
	// great.  if not, we'll still set m_SerialId to -1 so that another call
	// can at least be tried to connect to the port.
	ApnNetSerialPortClose( m_SerialId );

	ApnNetStopSockets();

	m_SerialId = -1;

	return true;
}

bool CApnSerial_NET::GetBaudRate( unsigned long *BaudRate )
{
	*BaudRate = -1;

	if ( m_SerialId == -1 )
		return false;

	/*
	unsigned long BaudRateRead;


	if ( m_SerialId == -1 )
		return false;

	if ( ApnNetSerialReadBaudRate(m_SerialId, &BaudRateRead) != APNET_SUCCESS )
		return false;

	*BaudRate = BaudRateRead;
	*/

	return true;
}

bool CApnSerial_NET::SetBaudRate( unsigned long BaudRate )
{
	if ( m_SerialId == -1 )
		return false;

	/*
	if ( ApnNetSerialWriteBaudRate(m_SerialId, BaudRate) != APNET_SUCCESS )
		return false;
	*/

	return true;
}

bool CApnSerial_NET::GetFlowControl( Apn_SerialFlowControl *FlowControl )
{
	*FlowControl = Apn_SerialFlowControl_Unknown;

	if ( m_SerialId == -1 )
		return false;

	/*
	bool FlowControlRead;

	if ( m_SerialId == -1 )
		return false;

	if ( ApnNetSerialReadFlowControl(m_SerialId, &FlowControlRead) != APNET_SUCCESS )
		return false;
	*/

	return true;
}

bool CApnSerial_NET::SetFlowControl( Apn_SerialFlowControl FlowControl )
{
	if ( m_SerialId == -1 )
		return false;

	/*
	if ( ApnNetSerialWriteFlowControl(m_SerialId, FlowControl) != APNET_SUCCESS )
		return false;
	*/

	return true;
}

bool CApnSerial_NET::GetParity( Apn_SerialParity *Parity )
{
	*Parity = Apn_SerialParity_Unknown;

	if ( m_SerialId == -1 )
		return false;

	/*
	ApnNetParity ParityRead;
		
	if ( m_SerialId == -1 )
		return false;

	if ( ApnNetSerialReadParity(m_SerialId, &ParityRead) != APNET_SUCCESS )
		return false;

	*Parity = (Apn_SerialParity)ParityRead;
	*/

	return true;
}

bool CApnSerial_NET::SetParity( Apn_SerialParity Parity )
{
	if ( m_SerialId == -1 )
		return false;

	/*
	if ( ApnNetSerialWriteParity(m_SerialId, (ApnNetParity)Parity) != APNET_SUCCESS )
		return false;
	*/

	return true;
}

bool CApnSerial_NET::Read( char	*ReadBuffer, unsigned short *ReadCount )
{
	if ( m_SerialId == -1 )
		return false;

	if ( ApnNetSerialRead(m_SerialId, ReadBuffer, ReadCount) != APNET_SUCCESS )
	{
		*ReadCount = 0;
		return false;
	}

	return true;
}

bool CApnSerial_NET::Write( char *WriteBuffer, unsigned short WriteCount )
{
	if ( m_SerialId == -1 )
		return false;

	if ( ApnNetSerialWrite(m_SerialId, WriteBuffer, WriteCount) != APNET_SUCCESS )
		return false;

	return true;
}

