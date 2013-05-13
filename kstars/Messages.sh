#! /bin/sh
#
# (LW 18/04/2002) Stripped trailing slashes from comments, to keep make happy
# (JH 16/08/2002) Patch submitted by Stefan Asserhall to deal with diacritic characters properly
# (JH 16/08/2002) modified to sort strings alphabetically and filter through uniq.
# (HE 31/08/2002) treat cities, regions, countries separately

rm -f kstars_i18n.cpp
rm -f cities.tmp
rm -f regions.tmp
rm -f countries.tmp
echo "#if 0" >> kstars_i18n.cpp

# extract constellations
sed -e "s/\([0-9].*[a-z]\)//" < data/cnames.dat | sed 's/^[A-B] //' | \
   sed 's/\([A-Z].*\)/i18nc("Constellation name (optional)", "\1");/' | sed 's/\ "/"/g' >> "kstars_i18n.cpp"

# extract sky cultures
grep ^C data/cnames.dat | awk '{ print "i18nc( \"Sky Culture\", \"" $2 "\" );" }' >> "kstars_i18n.cpp"

# extract cities
awk 'BEGIN {FS=":"}; {print "\"" $2 ":" $3 ":" $1 "\""; }' < data/Cities.dat | \
   sed 's/ *:/:/g' | \
   sed 's/ *\"$/\");/g' | sed 's/^\" */i18nc(\"City in /' | sed 's/ *: */ /' | sed 's/ *: */\",\"/' | sed 's/i18nc(.*,"");//' >> "cities.tmp"
sort --unique cities.tmp >> kstars_i18n.cpp

# extract regions
awk 'BEGIN {FS=":"}; {print "\"" $3 ":" $2 "\""; }' < data/Cities.dat | \
   sed 's/ *\"$/\");/g' | sed 's/^\" */i18nc(\"Region\/state in /' | sed 's/ *: */\",\"/g' | sed 's/i18nc(.*,"");//' >> "regions.tmp";
sort --unique regions.tmp >> kstars_i18n.cpp

# extract countries
awk 'BEGIN {FS=":"}; {print "\"" $3 "\""; }' < data/Cities.dat | \
   sed 's/ *\"$/\");/g' | sed 's/^\" */i18nc(\"Country name\",\"/g' | sed 's/i18nc(.*,"");//' >> "countries.tmp"
sort --unique countries.tmp >> kstars_i18n.cpp

# extract image/info menu items
awk 'BEGIN {FS=":"}; (NF==4 && $3~"http") {gsub(/\"/, "\\\""); print "i18nc(\"Image/info menu item (should be translated)\",\"" $2 "\");"; }' < data/image_url.dat | sed 's/i18nc(.*,"");//' >> "image_url.tmp"
sort --unique image_url.tmp >> kstars_i18n.cpp
awk 'BEGIN {FS=":"}; (NF==4 && $3~"http") {gsub(/\"/, "\\\""); print "i18nc(\"Image/info menu item (should be translated)\",\"" $2 "\");"; }' < data/info_url.dat | sed 's/i18nc(.*,"");//' >> "info_url.tmp"
sort --unique info_url.tmp >> kstars_i18n.cpp

# star names : some might be different in other languages, or they might have to be adapted to non-Latin alphabets
# TODO: Move this thing to starnames.dat
cat data/stars.dat | gawk 'BEGIN { FS=", "; } ($1!~/\#/ && NF==3) { printf( "i18nc(\"star name\", \"%s\");\n", $3); }' >> kstars_i18n.cpp;

# extract satellite group names
cat data/satellites.dat | gawk 'BEGIN {FS=";"} ($1!~/\#/) { printf( "i18nc(\"Satellite group name\", \"%s\");\n", $1); }' >> kstars_i18n.cpp;

# extract deep-sky object names (sorry, I don't know perl-fu ;( ...using AWK )
cat data/ngcic.dat | grep -v '^#' | gawk '{ split(substr( $0, 77 ), name, " "); \
if ( name[1]!="" ) { \
printf( "%s", name[1] ); i=2; \
while( name[i]!="" ) { printf( " %s", name[i] ); i++; } \
printf( "\n" ); } }' | sort --unique | gawk '{ \
printf( "i18nc(\"object name (optional)\", \"%s\");\n", $0 ); }' >> kstars_i18n.cpp

# extract strings from file containing advanced URLs:
cat data/advinterface.dat | gawk '( match( $0, "KSLABEL" ) ) { \
name=substr($0,10); \
printf( "i18nc(\"Advanced URLs: description or category\", \"%s\")\n", name ); }' >> kstars_i18n.cpp

# finish file
echo "#endif" >> kstars_i18n.cpp
# cleanup temporary files
rm -f cities.tmp
rm -f regions.tmp
rm -f countries.tmp
rm -f image_url.tmp
rm -f info_url.tmp
rm -f tips.cpp

$EXTRACTRC `find . -name '*.ui' -o -name '*.rc' -o -name '*.kcfg' | sort` >> rc.cpp || exit 11
(cd data && $PREPARETIPS > ../tips.cpp)
$XGETTEXT `find . -name '*.cpp' -o -name '*.h' -name '*.qml' | sort` -o $podir/kstars.pot
rm -f tips.cpp
rm -f kstars_i18n.cpp
rm -f rc.cpp
