/*

  Copyright (c) 2000, 2002 Finger Lakes Instrumentation (FLI), L.L.C.
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

#include <stdlib.h>
#include <string.h>

#include "libfli-libfli.h"
#include "libfli-mem.h"

#define DEFAULT_NUM_POINTERS (1024)

static struct {
  void **pointers;
  int total;
  int used;
} allocated = {NULL, 0, 0};

void *xmalloc(size_t size)
{
  int i;
  void *ptr;

  if (allocated.used + 1 > allocated.total)
  {
    void **tmp;
    int num;

    num = (allocated.total == 0 ? DEFAULT_NUM_POINTERS : 2 * allocated.total);

    if ((tmp = (void **)realloc(allocated.pointers,
				num * sizeof(void **))) == NULL)
      return NULL;

    allocated.pointers = tmp;
    memset(&allocated.pointers[allocated.total], 0, num * sizeof(void **));
    allocated.total += num;
  }

  if ((ptr = malloc(size)) == NULL)
    return NULL;

  for (i = 0; i < allocated.total; i++)
  {
    if (allocated.pointers[i] == NULL)
      break;
  }

  if (i == allocated.total)
  {
    /* This shouldn't happen */
    debug(FLIDEBUG_WARN, "Internal memory allocation error");
    free(ptr);

    return NULL;
  }

  allocated.pointers[i] = ptr;
  allocated.used++;

  return ptr;
}


void *xcalloc(size_t nmemb, size_t size)
{
  void *ptr;

  if ((ptr = xmalloc(nmemb * size)) == NULL)
    return NULL;

  memset(ptr, 0, nmemb * size);

  return ptr;
}

void xfree(void *ptr)
{
  int i;

  for (i = 0; i < allocated.total; i++)
  {
    if (allocated.pointers[i] == ptr)
    {
      free(ptr);
      allocated.pointers[i] = NULL;
      allocated.used--;

      return;
    }
  }

  debug(FLIDEBUG_WARN, "Attempting to free an invalid pointer");

  return;
}

void *xrealloc(void *ptr, size_t size)
{
  int i;

  for (i = 0; i < allocated.total; i++)
  {
    if (allocated.pointers[i] == ptr)
    {
      void *tmp;

      if ((tmp = realloc(ptr, size)) == NULL)
	return NULL;

      allocated.pointers[i] = tmp;

      return tmp;
    }
  }

  debug(FLIDEBUG_WARN, "Attempting to realloc an invalid pointer");

  return NULL;
}

int xfree_all(void)
{
  int i;
  int freed = 0;

  for (i = 0; i < allocated.total; i++)
  {
    if (allocated.pointers[i] != NULL)
    {
      free(allocated.pointers[i]);
      allocated.pointers[i] = NULL;
      allocated.used--;
      freed++;
    }
  }

  if (allocated.used != 0)
    debug(FLIDEBUG_WARN, "Internal memory handling error");

  if (allocated.pointers != NULL)
    free(allocated.pointers);

  allocated.pointers = NULL;
  allocated.used = 0;
  allocated.total = 0;

  return freed;
}

char *xstrdup(const char *s)
{
  void *tmp;
  size_t len;

  len = strlen(s) + 1;

  if ((tmp = xmalloc(len)) == NULL)
    return NULL;

  memcpy(tmp, s, len);

  return tmp;
}
