/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

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
#define TRUE 1
#define FALSE 0
#define INTERNET_OPEN_TYPE_DIRECT 1
#define INTERNET_FLAG_NO_CACHE_WRITE 1
#define INTERNET_FLAG_KEEP_CONNECTION 1
#define Sleep(x) usleep(1000*x)
#endif


#endif





