kde4_header()




########### install files ###############


kde4_footer()



#original Makefile.am contents follow:

#noinst_LIBRARIES = libkstarstools.a
#
#libkstarstools_a_SOURCES = altvstime.ui altvstime.cpp wutdialog.ui \
#			wutdialog.cpp astrocalc.cpp modcalcapcoord.ui modcalcazel.ui \
#			modcalcdaylength.ui modcalcgalcoord.ui modcalcgeod.ui modcalcjd.ui \
#			modcalcprec.ui modcalcsidtime.ui modcalcapcoord.cpp modcalcazel.cpp \
#			modcalcdaylength.cpp modcalcgalcoord.cpp modcalcgeodcoord.cpp modcalcjd.cpp modcalcprec.cpp \
#			modcalcsidtime.cpp lcgenerator.cpp scriptbuilder.ui scriptnamedialog.ui \
#			optionstreeview.ui argchangeviewoption.ui arglooktoward.ui argsetaltaz.ui \
#			argsetgeolocation.ui argsetlocaltime.ui argsetradec.ui argsettrack.ui argtimescale.ui \
#			argwaitforkey.ui argwaitfor.ui argzoom.ui argexportimage.ui argprintimage.ui \
#			argsetcolor.ui argloadcolorscheme.ui scriptbuilder.cpp scriptfunction.cpp scriptargwidgets.cpp \
#			planetviewer.ui planetviewer.cpp pvplotwidget.cpp jmoontool.cpp \
#			modcalcequinox.ui modcalcequinox.cpp modcalcplanets.ui modcalcplanets.cpp \
#			modcalceclipticcoords.ui modcalceclipticcoords.cpp modcalcangdist.ui modcalcangdist.cpp \
#			observinglist.cpp observinglist.ui argstartindi.ui argshutdownindi.ui argswitchindi.ui \
#			argsetportindi.ui argsettargetcoordindi.ui argsettargetnameindi.ui argsetactionindi.ui \
#			argsetfocusspeedindi.ui argstartfocusindi.ui argsetfocustimeoutindi.ui argsetgeolocationindi.ui \
#			argstartexposureindi.ui argsetutcindi.ui argsetdeviceindi.ui argsetframetypeindi.ui \
#			argsetscopeactionindi.ui argsetccdtempindi.ui modcalcvlsr.ui modcalcvlsr.cpp \
#			argsetfilternumindi.ui obslistwizard.cpp obslistwizard.h obslistwizard.ui lcgenerator.ui \
#			../widgets/kstarsplotwidget.cpp ../widgets/timebox.cpp
#
#libkstarstools_a_LIBADD = ../widgets/libkstarswidgets.a 
#
#INCLUDES= -I.. -I$(srcdir)/.. -I../widgets -I$(top_srcdir) $(all_includes)
#
#METASOURCES = AUTO
