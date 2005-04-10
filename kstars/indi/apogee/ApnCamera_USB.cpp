// ApnCamera_USB.cpp: implementation of the CApnCamera_USB class.
//
// Copyright (c) 2003, 2004 Apogee Instruments, Inc.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ApnCamera_USB.h"

#include "ApogeeUsb.h"
#include "ApogeeUsbErr.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////



bool CApnCamera::InitDriver( unsigned long	CamIdA, 
								 unsigned short /*CamIdB*/, 
								 unsigned long	/*Option*/ )
{
	if ( ApnUsbOpen( (unsigned short)CamIdA ) != APN_USB_SUCCESS )
	{
		return false;
	}

	m_CameraInterface = Apn_Interface_USB;

	// Before trying to initialize, perform a simple loopback test
	unsigned short	RegData;
	unsigned short	NewRegData;

	RegData = 0x5AA5;
	if ( Write( FPGA_REG_SCRATCH, RegData )		!= APN_USB_SUCCESS ) return false;
	if ( Read( FPGA_REG_SCRATCH, NewRegData )	!= APN_USB_SUCCESS ) return false;		
	if ( RegData != NewRegData ) return false;

	RegData = 0xA55A;
	if ( Write( FPGA_REG_SCRATCH, RegData )		!= APN_USB_SUCCESS ) return false;
	if ( Read( FPGA_REG_SCRATCH, NewRegData )	!= APN_USB_SUCCESS ) return false;		
	if ( RegData != NewRegData ) return false;

	// The loopback test was successful.  Proceed with initialization.
	if ( InitDefaults() != 0 )
		return false;

	return true;
}


bool CApnCamera::CloseDriver()
{
	ApnUsbClose();

	return true;
}

void CApnCamera::SetNetworkTransferMode( Apn_NetworkMode /*TransferMode*/ )
{
    	return;
} 

bool CApnCamera::GetImageData( unsigned short *pImageBuffer, 
								   unsigned short &Width,
								   unsigned short &Height,
								   unsigned long  &Count )
{
        unsigned short	Offset(0);
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
	
	ApnUsbGetImage( pTempBuffer );

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


bool CApnCamera::GetLineData( unsigned short */*pLineBuffer*/,
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

	if ( ApnUsbStartExp( m_pvtWidth, m_pvtHeight ) != APN_USB_SUCCESS )
	{
		return 1;
	}

	return 0;
}


long CApnCamera::PostStopExposure( bool DigitizeData )
{
	PUSHORT			pRequestData;


	if ( !DigitizeData )
	{
		while ( !ImageReady() )
		{
			Sleep( 50 );
			read_ImagingStatus();
		}

		pRequestData = new USHORT[m_pvtWidth*m_pvtHeight];

		ApnUsbGetImage( pRequestData );

		delete [] pRequestData;

		SignalImagingDone();
	}

	// The following code will eventually be the implementation of the STOP
	// command for USB.  Currently, this does not work correctly.
	// if ( ApnUsbStopExp( DigitizeData ) != APN_USB_SUCCESS )
	// {
	//	return 1;
	// }

	return 0;
}


long CApnCamera::Read( unsigned short reg, unsigned short& val )
{
	if ( ApnUsbReadReg( reg, &val ) != APN_USB_SUCCESS )
	{
		return 1;		// Failure
	}

	return 0;
}


long CApnCamera::Write( unsigned short reg, unsigned short val )
{
	if ( ApnUsbWriteReg( reg, val ) != APN_USB_SUCCESS )
	{
		return 1;		// Failure
	}

	return 0;
}


long CApnCamera::WriteMultiSRMD( unsigned short reg, unsigned short val[], unsigned short count )
{
	if ( ApnUsbWriteRegMulti( reg, val, count ) != APN_USB_SUCCESS )
	{
		return 1;
	}

	return 0;
}


long CApnCamera::WriteMultiMRMD( unsigned short reg[], unsigned short val[], unsigned short count )
{
	if ( ApnUsbWriteRegMultiMRMD( reg, val, count ) != APN_USB_SUCCESS )
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
       /*unsigned short stat,heat,ccdt,cool,volt,tdic,sequ;*/

	if ( ApnUsbReadStatusRegs( &StatusReg,
							   &HeatsinkTempReg,
							   &CcdTempReg,
							   &CoolerDriveReg,
							   &VoltageReg,
							   &TdiCounter,
							   &SequenceCounter ) != APN_USB_SUCCESS )
	{
		return 1;
	}
	return 0;
}




