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
#if !defined(STDAFX__INCLUDED_)
#define STDAFX__INCLUDED_

#ifdef __linux__
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#define ULONG unsigned long
#define USHORT unsigned short
#define PUSHORT unsigned short *
#define BYTE unsigned char
#define DWORD long
#define BOOLEAN unsigned long
#define INTERNET_OPEN_TYPE_DIRECT 1
#define INTERNET_FLAG_NO_CACHE_WRITE 1
#define INTERNET_FLAG_KEEP_CONNECTION 1
#define Sleep(x) usleep(1000*x)
#endif


#endif





