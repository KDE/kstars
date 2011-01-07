/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
// ApnCamera_NET.h: interface for the CApnCamera_NET class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_APNCAMERA_NET_H__D6F0E3AB_536C_4937_9E2B_DCF682D0DD31__INCLUDED_)
#define AFX_APNCAMERA_NET_H__D6F0E3AB_536C_4937_9E2B_DCF682D0DD31__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ApnCamera.h"

class CApnCamera_NET : public CApnCamera  
{
private:
	unsigned short m_pvtBitsPerPixel;

	unsigned short m_pvtWidth;
	unsigned short m_pvtHeight;

public:
	CApnCamera_NET();
	virtual ~CApnCamera_NET();

	bool InitDriver( unsigned long	CamIdA, 
					 unsigned short CamIdB, 
					 unsigned long	Option );

	bool CloseDriver();

	long PreStartExpose( unsigned short BitsPerPixel );

	long PostStopExposure( bool DigitizeData );


	bool GetImageData( unsigned short *pImageData, 
					   unsigned short &Width,
					   unsigned short &Height,
					   unsigned long  &Count );

	bool GetLineData( unsigned short *pLineBuffer,
					  unsigned short &Size );		

	long Read( unsigned short reg, unsigned short& val );
	long Write( unsigned short reg, unsigned short val );

	long WriteMultiSRMD( unsigned short reg, 
						 unsigned short val[], 
						 unsigned short count );

	long WriteMultiMRMD( unsigned short reg[], 
						 unsigned short val[], 
						 unsigned short count );

	long QueryStatusRegs( unsigned short& StatusReg,
						  unsigned short& HeatsinkTempReg,
						  unsigned short& CcdTempReg,
						  unsigned short& CoolerDriveReg,
						  unsigned short& VoltageReg,
						  unsigned short& TdiCounter,
						  unsigned short& SequenceCounter );

};

#endif // !defined(AFX_APNCAMERA_NET_H__D6F0E3AB_536C_4937_9E2B_DCF682D0DD31__INCLUDED_)
