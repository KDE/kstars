/*  Ekos guide tool
    Copyright (C) 2012 Alexander Stepanenko

    Modified by Jasem Mutlaq <mutlaqja@ikarustech.com> for KStars.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <sys/types.h>

#include "common.h"

// define some colors
const u_char DEF_BKGD_COLOR[3] 		= {0, 0, 0};
const u_char DEF_RA_COLOR[3]		= {0, 255, 0};
const u_char DEF_DEC_COLOR[3]		= {0, 165, 255};
const u_char DEF_GRID_COLOR[3]		= {128, 128, 128};
const u_char DEF_WHITE_COLOR[3]		= {255, 255, 255};
const u_char DEF_GRID_FONT_COLOR[3]	= {0, 255, 128};

const u_char DEF_SQR_OVL_COLOR[3]	= {0, 255, 0};

void u_msg( const char *fmt, ...)
{
/*
        va_list args;
        char buf[1024];

        va_start(args, fmt);
        int ret = vsnprintf( buf, sizeof(buf)-1, fmt, args );
        va_end(args);
*/
        va_list     argptr;
        QString     text;

        va_start (argptr,fmt);
        text.vsprintf(fmt, argptr);
        va_end (argptr);

        QMessageBox::information( NULL, "Info...", text, QMessageBox::Ok, QMessageBox::Ok );

        //QMessageBox::information( this, "Info...", QString().sprintf("test = %d", 13), QMessageBox::Ok, QMessageBox::Ok );
}

