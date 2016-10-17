/* 
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 

Copyright 2001 Liam Girdwood 
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libnova/utility.h>

static void usage(void)
{
	fprintf(stdout, "libnova-config --version\n");
	exit (-1);
}

static void get_version(void)
{
	fprintf(stdout, "libnova %s\n", ln_get_version());
}

int main(int argc, const char *argv[])
{
	int arg = 0;
	int version = 0;
	
	if (argc < 2)
		usage();
		
	argc--;
	arg = 1;
			
	while (argc) {
		if (strcmp (argv[arg], "--version") == 0)
			version = 1;
		arg++;
		argc--;
	}
	
	if (version)
		get_version();
		
	exit (0);
}
