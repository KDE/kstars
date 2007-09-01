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
// CameraIO_PCI.h: interface for the CCameraIO_PCI class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CAMERAIO_PCI_H__0F583058_8596_11D4_915F_0060676644C1__INCLUDED_)
#define AFX_CAMERAIO_PCI_H__0F583058_8596_11D4_915F_0060676644C1__INCLUDED_

#include "CameraIO.h"

class CCameraIO_PCI : public CCameraIO  
{
public:

	CCameraIO_PCI();
	virtual ~CCameraIO_PCI();

	bool InitDriver();
	long ReadLine( long SkipPixels, long Pixels, unsigned short* pLineBuffer );
	long Write( unsigned short reg, unsigned short val );
	long Read( unsigned short reg, unsigned short& val );

private:

	BOOLEAN	m_IsWDM;
	HANDLE	m_hDriver;

	void	CloseDriver();
};

#endif // !defined(AFX_CAMERAIO_PCI_H__0F583058_8596_11D4_915F_0060676644C1__INCLUDED_)
