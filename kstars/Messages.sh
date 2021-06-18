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
   sed 's/\([A-Z].*\)/xi18nc("Constellation name (optional)", "\1");/' | sed 's/\ "/"/g' >> "kstars_i18n.cpp"

# extract sky cultures
grep ^C data/cnames.dat | gawk '{ print "xi18nc( \"Sky Culture\", \"" $2 "\" );" }' >> "kstars_i18n.cpp"

# City data (name, province, country)
python3 data/scripts/extract_geo_data.py >> "kstars_i18n.cpp"

# extract image/info menu items
gawk 'BEGIN {FS=":"}; (NF==4 && $3~"http") {gsub(/"/, "\\\""); print "xi18nc(\"Image/info menu item (should be translated)\",\"" $2 "\");"; }' < data/image_url.dat | sed 's/xi18nc(.*,"");//' >> "image_url.tmp"
sort --unique image_url.tmp >> kstars_i18n.cpp
gawk 'BEGIN {FS=":"}; (NF==4 && $3~"http") {gsub(/"/, "\\\""); print "xi18nc(\"Image/info menu item (should be translated)\",\"" $2 "\");"; }' < data/info_url.dat | sed 's/xi18nc(.*,"");//' >> "info_url.tmp"
sort --unique info_url.tmp >> kstars_i18n.cpp

# star names : some might be different in other languages, or they might have to be adapted to non-Latin alphabets
# TODO: Move this thing to starnames.dat
cat data/stars.dat | gawk 'BEGIN { FS=", "; } ($1!~/#/ && NF==3) { printf( "xi18nc(\"star name\", \"%s\");\n", $3); }' >> kstars_i18n.cpp;

# extract satellite group names
cat data/satellites.dat | gawk 'BEGIN {FS=";"} ($1!~/#/) { printf( "xi18nc(\"Satellite group name\", \"%s\");\n", $1); }' >> kstars_i18n.cpp;

# Extract asteroids
cat data/asteroids.dat | gawk '{if (NR!=1) {{FPAT = "([^,]*)|(\"[^\"]+\")"}; {gsub("\"",""); name = substr($1,8); printf("xi18nc(\"Asteroid name (optional)\",\"%s\")\n",name)}}}' >> kstars_i18n.cpp;

# Extract comets
cat data/comets.dat | gawk '{if (NR!=1) {{FPAT = "([^,]*)|(\"[^\"]+\")"}; {gsub("\"",""); sub(/^[ \t]+/, ""); printf("xi18nc(\"Comet name (optional)\",\"%s\")\n",$1)}}}' >> kstars_i18n.cpp;

# extract strings from file containing advanced URLs:
cat data/advinterface.dat | gawk '( match( $0, "KSLABEL" ) ) { \
name=substr($0,10); \
printf( "xi18nc(\"Advanced URLs: description or category\", \"%s\")\n", name ); }' >> kstars_i18n.cpp

# finish file
echo "#endif" >> kstars_i18n.cpp
# cleanup temporary files
rm -f image_url.tmp
rm -f info_url.tmp
rm -f tips.cpp

$EXTRACTRC `find . -name '*.ui' -o -name '*.rc' -o -name '*.kcfg' | sort` >> rc.cpp || exit 11
(cd data && $PREPARETIPS > ../tips.cpp)
$XGETTEXT -C --from-code=UTF-8 --keyword=xi18n --keyword=xi18nc:1c,2 `find . -name '*.cpp' -o -name '*.h' -o -name '*.qml' | sort` -o $podir/kstars.pot
rm -f tips.cpp
rm -f kstars_i18n.cpp
rm -f rc.cpp

