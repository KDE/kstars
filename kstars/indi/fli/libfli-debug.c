/*

  Copyright (c) 2002 Finger Lakes Instrumentation (FLI), L.L.C.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

        Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer. 

        Redistributions in binary form must reproduce the above
        copyright notice, this list of conditions and the following
        disclaimer in the documentation and/or other materials
        provided with the distribution.

        Neither the name of Finger Lakes Instrumentation (FLI), LLC
        nor the names of its contributors may be used to endorse or
        promote products derived from this software without specific
        prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

  ======================================================================

  Finger Lakes Instrumentation, L.L.C. (FLI)
  web: http://www.fli-cam.com
  email: support@fli-cam.com

*/

#include <syslog.h>
#include <stdarg.h>

#include "libfli-libfli.h"

int sysloglevel(int level)
{
  switch (level)
  {
  case FLIDEBUG_INFO:
    return LOG_INFO;
    break;

  case FLIDEBUG_WARN:
    return LOG_WARNING;
    break;

  case FLIDEBUG_FAIL:
    return LOG_ERR;
    break;

  case FLIDEBUG_ALL:
    return LOG_EMERG | LOG_ALERT | LOG_CRIT | LOG_ERR | LOG_WARNING |
      LOG_NOTICE | LOG_INFO | LOG_DEBUG;
    break;
  }

  return 0;
}

int debugopen(char *host)
{
  openlog("libfli", LOG_PID , LOG_USER);

  return 0;
}

int debugclose(void)
{
  closelog();

  return 0;
}

void debug(int level, const char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  vsyslog(sysloglevel(level), format, ap);
  va_end(ap);

  return;
}


void setdebuglevel(char *host, int level)
{
  static int open = 0;

  if (level == 0)
  {
    debugclose();
    open = 0;
    return;
  }

  if (open == 0)
  {
    debugopen(host);
    open = 1;
  }

  setlogmask(LOG_UPTO(sysloglevel(level)));

  return;
}
