// ApnCamera_NET.cpp: implementation of the CApnCamera_NET class.
//
// Copyright (c) 2003, 2004 Apogee Instruments, Inc.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ApnCamera_NET.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ApogeeNet.h"
#include "ApogeeNetErr.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////



bool CApnCamera::InitDriver( unsigned long	CamIdA, 
								 unsigned short CamIdB, 
								 unsigned long	Option )
{
	char	Hostname[25];
	BYTE	ipAddr[4];
	int	init;

	ipAddr[0] = (BYTE)(CamIdA & 0xFF);
	ipAddr[1] = (BYTE)((CamIdA >> 8) & 0xFF);
	ipAddr[2] = (BYTE)((CamIdA >> 16) & 0xFF);
	ipAddr[3] = (BYTE)((CamIdA >> 24) & 0xFF);

	sprintf( Hostname, "%u.%u.%u.%u:%u", ipAddr[3], ipAddr[2], ipAddr[1], ipAddr[0], CamIdB );

	if ( ApnNetConnect( Hostname ) != APNET_SUCCESS )
	{
		return false;
	}

	m_CameraInterface = Apn_Interface_NET;
	
	// Before trying to initialize, perform a simple loopback test
	unsigned short	RegData;
	unsigned short	NewRegData;

	RegData = 0x5AA5;
	if ( Write( FPGA_REG_SCRATCH, RegData )		!= APNET_SUCCESS ) return false;
	if ( Read( FPGA_REG_SCRATCH, NewRegData )	!= APNET_SUCCESS ) return false;		
	if ( RegData != NewRegData ) return false;

	RegData = 0xA55A;
	if ( Write( FPGA_REG_SCRATCH, RegData )		!= APNET_SUCCESS ) return false;
	if ( Read( FPGA_REG_SCRATCH, NewRegData )	!= APNET_SUCCESS ) return false;		
	if ( RegData != NewRegData ) return false;
	printf("Loopback test successful - ALTA-E detected at %u.%u.%u.%u:%u\n", ipAddr[3], ipAddr[2], ipAddr[1], ipAddr[0]);

	// The loopback test was successful.  Proceed with initialization.
	init = InitDefaults();
	if ( init != 0 ) {
		printf("Loopback test successful - InitDefaults FAILED\n");
		return false;
	}

	printf("Loopback test successful - InitDefaults completed\n");
	return true;
}


bool CApnCamera::CloseDriver()
{
	ApnNetClose();

	return true;
}


bool CApnCamera::GetImageData( unsigned short *pImageBuffer, 
								   unsigned short &Width,
								   unsigned short &Height,
								   unsigned long  &Count )
{
	unsigned short	Offset;
	unsigned short	*pTempBuffer;
	long			i, j;

	
	// Make sure it is okay to get the image data
	// The app *should* have done this on its own, but we have to make sure
	while ( !ImageReady() )
	{
		Sleep( 50 );
		read_ImagingStatus();
	}

	Width	= m_pvtWidth;
	Height	= m_pvtHeight;

	if ( m_pvtBitsPerPixel == 16 )
		Offset = 1;

	if ( m_pvtBitsPerPixel == 12 )
		Offset = 10;

	Width -= Offset;	// Calculate the true image width

	pTempBuffer = new unsigned short[(Width+Offset) * Height];
	
	ApnNetGetImageTcp( pTempBuffer );

	for ( i=0; i<Height; i++ )
	{
		for ( j=0; j<Width; j++ )
		{
			pImageBuffer[(i*Width)+j] = pTempBuffer[(i*(Width+Offset))+j+Offset];
		}
	}

	delete [] pTempBuffer;

	Count = read_ImageCount();

	SignalImagingDone();

	return true;
}


bool CApnCamera::GetLineData( unsigned short *pLineBuffer,
								  unsigned short &Size )
{
	Size = 0;

	return false;
}


long CApnCamera::PreStartExpose( unsigned short BitsPerPixel )
{
	m_pvtWidth	= GetExposurePixelsH();
	m_pvtHeight	= GetExposurePixelsV();

	if ( (BitsPerPixel != 16) && (BitsPerPixel != 12) )
	{
		// Invalid bit depth request
		return 1;
	}

	m_pvtBitsPerPixel = BitsPerPixel;

	if ( BitsPerPixel == 16 )
		m_pvtWidth += 1;

	if ( BitsPerPixel == 12 )
		m_pvtWidth += 10;

	if ( ApnNetStartExp( m_pvtWidth, m_pvtHeight ) != APNET_SUCCESS )
	{
		return 1;
	}

	return 0;
}


long CApnCamera::PostStopExposure( bool DigitizeData )
{
	if ( ApnNetStopExp( DigitizeData ) != APNET_SUCCESS )
	{
		return 1;
	}

	if ( !DigitizeData )
	{
		SignalImagingDone();
	}

	return 0;
}


void CApnCamera::SetNetworkTransferMode( Apn_NetworkMode TransferMode )
{
	switch ( TransferMode )
	{
	case Apn_NetworkMode_Tcp:
		ApnNetSetSpeed( false );
		break;
	case Apn_NetworkMode_Udp:
		ApnNetSetSpeed( true );
		break;
	}
}


long CApnCamera::Read( unsigned short reg, unsigned short& val )
{
	if ( ApnNetReadReg( reg, &val ) != APNET_SUCCESS )
	{
		return 1;		// Failure
	}
	return 0;
}


long CApnCamera::Write( unsigned short reg, unsigned short val )
{
	if ( ApnNetWriteReg( reg, val ) != APNET_SUCCESS )
	{
		return 1;		// Failure
	}

	return 0;
}


long CApnCamera::WriteMultiSRMD( unsigned short reg, unsigned short val[], unsigned short count )
{
	if ( ApnNetWriteRegMulti( reg, val, count ) != APNET_SUCCESS )
	{
		return 1;
	}

	return 0;
}


long CApnCamera::WriteMultiMRMD( unsigned short reg[], unsigned short val[], unsigned short count )
{
	if ( ApnNetWriteRegMultiMRMD( reg, val, count ) != APNET_SUCCESS )
	{
		return 1;
	}

	return 0;
}


long CApnCamera::QueryStatusRegs( unsigned short& StatusReg,
									  unsigned short& HeatsinkTempReg,
									  unsigned short& CcdTempReg,
									  unsigned short& CoolerDriveReg,
									  unsigned short& VoltageReg,
									  unsigned short& TdiCounter,
									  unsigned short& SequenceCounter )
{
	unsigned short RegNumber[7];
	unsigned short RegData[7];

	RegNumber[0] = 91;
	RegNumber[1] = 93;
	RegNumber[2] = 94;
	RegNumber[3] = 95;
	RegNumber[4] = 96;
	RegNumber[5] = 104;
	RegNumber[6] = 105;

	if ( ApnNetReadRegMulti( RegNumber, RegData, 7 ) != APNET_SUCCESS )
	{
		return 1;
	}

	StatusReg		= RegData[0];
	HeatsinkTempReg	= RegData[1];
	CcdTempReg		= RegData[2];
	CoolerDriveReg	= RegData[3];
	VoltageReg		= RegData[4];
	TdiCounter		= RegData[5];
	SequenceCounter	= RegData[6];

	return 0;
}

