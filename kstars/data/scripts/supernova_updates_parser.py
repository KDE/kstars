
### Supernova Updates Parser. This program reads data from http://www.cbat.eps.harvard.edu/lists/RecentSupernovae.html 
### This page gives details on supernovae that have occurred since the start of 2010 .

#!/usr/bin/env python
import re
import urllib
import os
from PyKDE4.kdecore import KStandardDirs

def parse( line ) :
    parsed = toCSV(re.sub('<.*?>','',line))
    return parsed

def toCSV(line):
    commaInterval= [7,24,36,44,51,63,68,86,110,129,133,143]
    for i in range(0,len(line)-1):
	if i in commaInterval:
	    edited = line [ :i ] + "," +line [ i+1: ]
	    line=edited
    return line

sock = urllib.urlopen("http://www.cbat.eps.harvard.edu/lists/RecentSupernovae.html")
pageLines=sock.readlines()
sock.close()
print len(pageLines)
found = False
firstLine=True

output = open(KStandardDirs().locateLocal('appdata','supernova.dat'),'w')
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
	#print count
	output.write(parsedLine)
	continue;
    p = re.compile("<pre>")
    m = p.search(i)
    if m:
	print "found!!"+i
	firstLine=True
	found = True
output.close()
#if(i==len(pageLines)) :
    #print "Not Found" + i

