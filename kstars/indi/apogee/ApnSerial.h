/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
// ApnSerial.h: interface for the CApnSerial class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_APNSERIAL_H__A27F1749_FA8F_40E8_A03F_4A28C8378DD1__INCLUDED_)
#define AFX_APNSERIAL_H__A27F1749_FA8F_40E8_A03F_4A28C8378DD1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Apogee.h"


class CApnSerial  
{
public:

	CApnSerial();
	virtual ~CApnSerial();

	virtual bool InitPort( unsigned long	CamIdA,
						   unsigned short	CamIdB,
						   unsigned short	SerialId ) = 0;

	virtual bool ClosePort() = 0;

	virtual bool GetBaudRate( unsigned long *BaudRate ) = 0;

	virtual bool SetBaudRate( unsigned long BaudRate ) = 0;

	virtual bool GetFlowControl( Apn_SerialFlowControl *FlowControl ) = 0;

	virtual bool SetFlowControl( Apn_SerialFlowControl FlowControl ) = 0;

	virtual bool GetParity( Apn_SerialParity *Parity ) = 0;

	virtual bool SetParity( Apn_SerialParity Parity ) = 0;

	virtual bool Read( char			  *ReadBuffer,
					   unsigned short *ReadCount ) = 0;

	virtual bool Write( char		   *WriteBuffer,
						unsigned short WriteCount ) = 0;

	// Variables
	Apn_Interface	m_CameraInterface;
	short			m_SerialId;

};

#endif // !defined(AFX_APNSERIAL_H__A27F1749_FA8F_40E8_A03F_4A28C8378DD1__INCLUDED_)
