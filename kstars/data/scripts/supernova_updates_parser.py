##************************************************************************##
#            supernova_updates_parser.py  -  K Desktop Planetarium         #
#                             -------------------                          #
#    begin                : 2011/18/06                                     #
#    copyright            : (C) 2011 by Samikshan Bairagya                 #
#    email                : samikshan@gmail.com                            #
##************************************************************************##

##************************************************************************##
#                                                                          #
#   This program is free software; you can redistribute it and/or modify   #
#   it under the terms of the GNU General Public License as published by   #
#   the Free Software Foundation; either version 2 of the License, or      #
#   (at your option) any later version.                                    #
#                                                                          #
##************************************************************************##

### Supernova Updates Parser. This program reads data from http://www.cbat.eps.harvard.edu/lists/RecentSupernovae.html 
### This page gives details on supernovae that have occurred since the start of 2010.

#!/usr/bin/env python
import sys
import re
from urllib import urlopen
import os
import difflib

def parse( line ) :
    parsed = toCSV(re.sub('<.*?>','',line))
    return parsed

#FIXME: The extracted data is converted to CSV by inserting commas
       #after definite number of characters. There might be a better
       #way to do this.
def toCSV(line):
    commaInterval= [7,24,36,44,51,63,68,86,110,129,133,143]
    for i in range(0,len(line)-1):
        if i in commaInterval:
            edited = line [ :i ] + "," +line [ i+1: ]
            line=edited
    return line

sock = urlopen("http://www.cbat.eps.harvard.edu/lists/RecentSupernovae.html")
pageLines = sock.readlines()
sock.close()
found = False
firstLine = True

output_file=sys.argv[1]
#print "Output file is " + output_file

output = open(output_file,'w')

for i in pageLines:
    if(found):
        p = re.compile("</pre>")
        m = p.search(i)
        if m:
            found = False
            break
        if (firstLine):
            parsedLine = "#" + parse(i)
            firstLine = False
        else:
            parsedLine = parse(i)
        output.write(parsedLine)
        continue;
    p = re.compile("<pre>")
    m = p.search(i)
    if m:
        #print "found!!"+i
        firstLine=True
        found = True

output.close()
print "Supernovae List Update Finished"
