/*
    SPDX-FileCopyrightText: 2009 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
NOTE: This file was written from scratch using several headers written
by others as reference. Of particular mention is LICQ's
licq_bytorder.h and Oskar Liljeblad's byteswap.h licensed under the
GPL.
*/

#ifndef BYTEORDER_H_
#define BYTEORDER_H_

// Check if we have standard byteswap headers

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>

#elif defined HAVE_MACHINE_ENDIAN_H
#include <machine/endian.h>
#define bswap_16(x) swap16(x)
#define bswap_32(x) swap32(x)

#elif defined HAVE_SYS_BYTEORDER_H
#include <sys/byteorder.h>
#define bswap_16(x) BSWAP_16(x)
#define bswap_32(x) BSWAP_32(x)
#endif

// If no standard headers are found, we define our own byteswap macros

#ifndef bswap_16
#define bswap_16(x) ((((x)&0x00FF) << 8) | (((x)&0xFF00) >> 8))
#endif

#ifndef bswap_32

#define bswap_32(x) \
    ((((x)&0x000000FF) << 24) | (((x)&0x0000FF00) << 8) | (((x)&0x00FF0000) >> 8) | (((x)&0xFF000000) >> 24))
#endif

#endif
