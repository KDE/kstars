$filename = $ARGV[0]; # "somecities.dat"

&initTZList();

open ( CITIES, $filename ) || die "Datei $filename nicht gefunden";
while ( $city = <CITIES> ) {
	chop $city;
	$_ = $city;
	$fieldcount = s/://g;
	@fields = split(/:/, $city );

  for ( $i=0; $i<$fieldcount; $i++) {
  	$fields[$i] =~ s/^\s+//;
	  $fields[$i] =~ s/\s+$//;
	}
#  printf("%d x ", $fieldcount );
	$TZ = &calcTZ($fields[2]);
	if ( $fieldcount == 11 ) {
		printf ( "%-32s :", $fields[0] ); # City
    printf ( " %-21s :", $fields[1] ); # Province
    printf ( " %-21s :", $fields[2] ); # Country
    printf ( " %2s :", $fields[3] ); # Lat deg
    printf ( " %2s :", $fields[4] ); # Lat min
    printf ( " %2s :", $fields[5] ); # lat sec
    printf ( " %1s :", $fields[6] ); # lat sign
    printf ( " %3s :", $fields[7] ); # lon deg
    printf ( " %2s :", $fields[8] ); # long min
    printf ( " %2s :", $fields[9] ); # lon sec
    printf ( " %1s :", $fields[10] ); # lon sign
		printf ( " %s", $TZ );
	}
	else {
		# old data: no privince field, replace with blank
		printf ( "%-32s :", $fields[0] ); # City
    printf ( " %-21s :", "" ); # Province
    printf ( " %-21s :", $fields[1] ); # Country
    printf ( " %2s :", $fields[2] ); # Lat deg
    printf ( " %2s :", $fields[3] ); # Lat min
    printf ( " %2s :", $fields[4] ); # lat sec
    printf ( " %1s :", $fields[5] ); # lat sign
    printf ( " %3s :", $fields[6] ); # lon deg
    printf ( " %2s :", $fields[7] ); # long min
    printf ( " %2s :", $fields[8] ); # lon sec
    printf ( " %1s :", $fields[9] ); # lon sign
		printf ( " %s", $TZ );
	}
  printf"\n";
}

sub calcTZ {
	local($country);
	local($TZ);
	$country=$_[0];
	$TZ = $timezone{$country};
	if ( $TZ eq "" ) {
		$TZ = "x";
	}
	$TZ;
}

sub initTZList{
	$timezone{"Afghanistan"} = "4.5";
	$timezone{"Algeria"} = "1.0";
	$timezone{"Antarcti"} = "x"; # covers several TZs
	$timezone{"Antigua"} = "x";
	$timezone{"Argentina"} = "-3.0";	
	$timezone{"Ascension Island"} = "0.0";
	$timezone{"Australia"} = "x";	# covers several TZs
	$timezone{"Austria"} = "1.0";
	$timezone{"Azores"} = "-1.0";
	$timezone{"Bahamas"} = "-5.0";
	$timezone{"Bahrain"} = "x";  # illegible in map
	$timezone{"Bangladesh"} = "6.0";
	$timezone{"Barbados"} = "x";
	$timezone{"Belgium"} = "1.0";
	$timezone{"Belize"} = "-6.0";
	$timezone{"Bermuda"} = "x";
	$timezone{"Bolivia"} = "-4.0";
	$timezone{"Bosnia"} = "1.0";
	$timezone{"Brazil"} = "x";  # covers several TZs
	$timezone{"Brunei"} = "8.0";
	$timezone{"Bulgaria"} = "2.0";
	$timezone{"Cabo Verde"} = "1.0";
	$timezone{"Caicos Islands"} = "x";
	$timezone{"Cameroon"} = "1.0";
	$timezone{"Canada"} = "x"; # covers several TZs
	$timezone{"Canary Island"} = "0.0";
	$timezone{"Cayman Islands"} = "x";
	$timezone{"Chad"} = "1.0";
	$timezone{"Chile"} = "-4.0";
	$timezone{"China"} = "8.0";
	$timezone{"Colombia"} = "-5.0";
	$timezone{"Costa Rica"} = "-6.0";
	$timezone{"Croatia"} = "1.0";
	$timezone{"Cuba"} = "-5.0";
	$timezone{"Cyprus"} = "2.0";
	$timezone{"Czech Republic"} = "1.0";
	$timezone{"Denmark"} = "1.0";
	$timezone{"Djibouti"} = "3.0";
	$timezone{"Dominican Repub"} = "-4.0";
	$timezone{"Ecuador"} = "-5.0";
	$timezone{"Egypt"} = "2.0";
	$timezone{"El Salvador"} = "-6.0";
	$timezone{"Ethiopia"} = "3.0";
	$timezone{"Falkland Island"} = "-4.0";
	$timezone{"Fiji"} = "12.0";
	$timezone{"Finland"} = "2.0";
	$timezone{"France"} = "1.0";
	$timezone{"French Guiana"} = "-3.0";
	$timezone{"French Polynesia"} = "-10.0";  # mostly
	$timezone{"Gabon"} = "1.0";
	$timezone{"Gambia"} = "0.0";
	$timezone{"Germany"} = "1.0";
	$timezone{"Ghana"} = "0.0";
	$timezone{"Greece"} = "2.0";
	$timezone{"Greenland"} = "-3.0";
	$timezone{"Guadalcanal"} = "x";
	$timezone{"Guam"} = "x";
	$timezone{"Guatemala"} = "-6.0";
	$timezone{"Guyana"} = "-4.0";
	$timezone{"Haiti"} = "-5.0";
	$timezone{"Honduras"} = "-6.0";
	$timezone{"Hong Kong"} = "8.0";
	$timezone{"Hungary"} = "1.0";
	$timezone{"Iceland"} = "0.0";
	$timezone{"India"} = "5.5";
	$timezone{"Indonesia"} = "x"; # covers several TZs
	$timezone{"Iran"} = "3.5";
	$timezone{"Iraq"} = "3.0";
	$timezone{"Ireland"} = "0.0";
	$timezone{"Isle of Man"} = "0.0";
	$timezone{"Israel"} = "2.0";
	$timezone{"Italy"} = "1.0";
	$timezone{"Jamaica"} = "-5.0";
	$timezone{"Japan"} = "9.0";
	$timezone{"Jordan"} = "2.0";
	$timezone{"Kenya"} = "3.0";
	$timezone{"Kuwait"} = "3.0";
	$timezone{"Lebanon"} = "2.0";
	$timezone{"Liberia"} = "0.0";
	$timezone{"Libya"} = "1.0";
	$timezone{"Luxembourg"} = "1.0";
	$timezone{"Madeira Island"} = "0.0";
	$timezone{"Malaysia"} = "8.0";
	$timezone{"Maldive Island"} = "5.0";
	$timezone{"Malta"} = "1.0";
	$timezone{"Marshall Islands"} = "12.0";
	$timezone{"Mauritius"} = "4.0";
	$timezone{"Mexico"} = "x"; # covers several TZs
	$timezone{"Micronesia"} = "x";
	$timezone{"Monaco"} = "1.0";
	$timezone{"Morocco"} = "0.0";
	$timezone{"Myanmar"} = "9.5";
	$timezone{"Nepal"} = "5.75";
	$timezone{"Netherlands"} = "1.0";
	$timezone{"New Caledonia"} = "x";
	$timezone{"New Zealand"} = "12.0"; # apart from Chatham Islands
	$timezone{"Nicaragua"} = "6.0";
	$timezone{"Nigeria"} = "1.0";
	$timezone{"Northern Ireland"} = "0.0";
	$timezone{"Norway"} = "1.0";
	$timezone{"Pakistan"} = "5.0";
	$timezone{"Palau"} = "x";
	$timezone{"Panama"} = "-5.0";
	$timezone{"Papua"} = "10.0";
	$timezone{"Paraguay"} = "-4.0";
	$timezone{"Peru"} = "-5.0";
	$timezone{"Philippines"} = "8.0";
	$timezone{"Poland"} = "1.0";
	$timezone{"Portugal"} = "0.0"; 
	$timezone{"Qatar"} = "x"; # illegible on map
	$timezone{"Romania"} = "2.0";
	$timezone{"Russia"} = "x"; # covers several TZs
	$timezone{"Samoa"} = "x";
	$timezone{"Saudi Arabia"} = "3.0";
	$timezone{"Scotland"} = "0.0";
	$timezone{"Senegal"} = "0.0";
	$timezone{"Serbia"} = "1.0";
	$timezone{"Seychelles"} = "4.0";
	$timezone{"Singapore"} = "8.0";
	$timezone{"Slowenien"} = "1.0";
	$timezone{"South Africa"} = "2.0";
	$timezone{"South Korea"} = "9.0";
	$timezone{"Spain"} = "1.0";
	$timezone{"Sri Lanka"} = "5.5";
	$timezone{"St. Lucia"} = "x";
	$timezone{"Sudan"} = "2.0";
	$timezone{"Sweden"} = "1.0";
	$timezone{"Switzerland"} = "1.0";
	$timezone{"Syria"} = "2.0";
	$timezone{"Tahiti"} = "x";
	$timezone{"Taiwan"} = "8.0";
	$timezone{"Tanzania"} = "3.0";
	$timezone{"Thailand"} = "7.0";
	$timezone{"Tibet"} = "8.0";
	$timezone{"Trinidad"} = "-4.0";
	$timezone{"Tunisia"} = "1.0";
	$timezone{"Turkey"} = "2.0";
	$timezone{"UK Territory"} = "x";
	$timezone{"US Teritory"} = "x";
	$timezone{"US Territory"} = "x";
	$timezone{"USA"} = "x";			# covers several TZs
	$timezone{"Uganda"} = "3.0";
	$timezone{"Ukraine"} = "2.0";
	$timezone{"United Arab Emirates"} = "4.0";
	$timezone{"United Kingdom"} = "0.0";
	$timezone{"Uruguay"} = "-3.0";
	$timezone{"Vanuatu"} = "11.0";
	$timezone{"Venezuela"} = "-4.0";
	$timezone{"Virgin Islands"} = "x";
	$timezone{"Yemen"} = "3.0";
	$timezone{"Zaire"} = "x";	# covers several TZs
	$timezone{"Zambia"} = "2.0";
}
