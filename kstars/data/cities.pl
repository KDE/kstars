$filename = $ARGV[0]; # "somecities.dat"

&initTZList();

open ( CITIES, $filename ) || die "file $filename not found";
while ( $city = <CITIES> ) {
	chop $city;
	$_ = $city;
	$fieldcount = s/://g;
	@fields = split(/:/, $city );

  for ( $i=0; $i<$fieldcount; $i++) {
  	$fields[$i] =~ s/^\s+//;
	  $fields[$i] =~ s/\s+$//;
	}
	#printf("calling 0: %s, 1: %s, 2: %s \n", $fields[0], $fields[1], $fields[2]);

  #Only calculate TZ if it is currently set to "x"
  if ($fields[11]=="x") {
		$TZ = &calcTZ($fields[2], $fields[1]);
  } else {
		$TZ = $fields[11];
	}

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
	local($province);
	local($TZ);
	$country=$_[0];
	$province=$_[1];
	# printf ("calculating TZ for country=x%sx, province=x%sx\n", $country, $province);

	$TZ = $timezone{$country};
	# printf ("Current result: %s\n", $TZ);
	if ( ($TZ eq "" ) or ($TZ eq "x")) {
		if ( $country eq "USA" ) { 
			$TZ = $us_timezone{$province}; 
		}
		if ( $country eq "Canada" ) { 
			$TZ = $ca_timezone{$province}; 
		}
		if ( $country eq "Australia" ) { 
			$TZ = $au_timezone{$province}; 
		}
		if ( $TZ eq "" ) {
			$TZ = "x";
	  }
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

#Province-specific TZ's
# US States
	$us_timezone{"Alabama"} = "-6.0";
  $us_timezone{"Alaska"} = "-9.0";
  $us_timezone{"Arizona"} = "-7.0"; #AZ does not use daylight savings time
  $us_timezone{"Arkansas"} = "-6.0";
  $us_timezone{"California"} = "-8.0";
  $us_timezone{"Colorado"} = "-7.0";
  $us_timezone{"Connecticut"} = "-5.0";
  $us_timezone{"Delaware"} = "-5.0";
  $us_timezone{"Florida"} = "-5.0"; #a small part of western FL is -6.0 (CST)
  $us_timezone{"Georgia"} = "-5.0";
  $us_timezone{"Hawaii"} = "-10.0";
  $us_timezone{"Idaho"} = "x"; #northern ID is -8.0; southern ID is -7.0
  $us_timezone{"Illinois"} = "-6.0";
  $us_timezone{"Indiana"} = "-5.0"; #two corners of IN are in -6.0; the rest of IN does not use savings time
  $us_timezone{"Iowa"} = "-6.0";
  $us_timezone{"Kansas"} = "-6.0"; #4 counties in Western KS are -7.0
  $us_timezone{"Kentucky"} = "x"; #half is -5.0, half is -6.0
  $us_timezone{"Louisiana"} = "-6.0";
  $us_timezone{"Maine"} = "-5.0";
  $us_timezone{"Maryland"} = "-5.0";
  $us_timezone{"Massachusetts"} = "-5.0";
  $us_timezone{"Michigan"} = "-5.0";
  $us_timezone{"Minnesota"} = "-6.0";
  $us_timezone{"Mississippi"} = "-6.0";
  $us_timezone{"Missouri"} = "-6.0";
  $us_timezone{"Montana"} = "-7.0";
  $us_timezone{"Nebraska"} = "x"; #eastern 2/3 is -6.0, the rest is -7.0
  $us_timezone{"Nevada"} = "-8.0";
  $us_timezone{"New Hampshire"} = "-5.0";
  $us_timezone{"New Jersey"} = "-5.0";
  $us_timezone{"New Mexico"} = "-7.0";
  $us_timezone{"New York"} = "-5.0";
  $us_timezone{"North Carolina"} = "-5.0";
  $us_timezone{"North Dakota"} = "x"; #3/4 is -6.0, the rest is -7.0
  $us_timezone{"Ohio"} = "-5.0";
  $us_timezone{"Oklahoma"} = "-6.0";
  $us_timezone{"Oregon"} = "-8.0"; #one county in Easter OR is -7.0
  $us_timezone{"Pennsylvania"} = "-5.0";
  $us_timezone{"Rhode Island"} = "-5.0";
  $us_timezone{"South Carolina"} = "-5.0";
  $us_timezone{"South Dakota"} = "x"; #1/2 is -6.0, the rest is -7.0
  $us_timezone{"Tennessee"} = "x"; #2/3 is -6.0, the rest is -5.0
  $us_timezone{"Texas"} = "-6.0"; #two westernmost counties are -7.0
  $us_timezone{"Utah"} = "-7.0";
  $us_timezone{"Vermont"} = "-5.0";
  $us_timezone{"Virginia"} = "-5.0";
  $us_timezone{"Washington"} = "-8.0";
  $us_timezone{"West Virginia"} = "-5.0";
  $us_timezone{"Wisconsin"} = "-6.0";
  $us_timezone{"Wyoming"} = "-7.0";
  $us_timezone{"DC"} = "-5.0";
  $us_timezone{"Puerto Rico"} = "-5.0";

#Canadian Provinces
	$ca_timezone{"Alberta"} = "-7.0";
  $ca_timezone{"British Columbia"} = "-8.0"; #Small parts of Eastern BC are -7.0
	$ca_timezone{"Labrador"} = "-4.0";
  $ca_timezone{"Manitoba"} = "-6.0"; 
	$ca_timezone{"New Brunswick"} = "-4.0";
	$ca_timezone{"Newfoundland"} = "-3.5"; 
	$ca_timezone{"Northwest Territories"} = "-7.0";
  $ca_timezone{"Nova Scotia"} = "-4.0";
	$ca_timezone{"Nunavut"} = "x"; #spans 3 timezones!  -5, -6 and -7
  $ca_timezone{"Ontario"} = "x"; #Eastern 2/3 is -5, the rest is -6
	$ca_timezone{"Prince Edward Island"} = "-4.0";
	$ca_timezone{"Quebec"} = "-5.0"; #small eastern section is -4
	$ca_timezone{"Saskatchewan"} = "-6.0"; #does not use savings time.
	$ca_timezone{"Yukon"} = "-8.0";

#Australian Provinces
	$au_timezone{"ACT"} = "+10.0";
	$au_timezone{"New South Wales"} = "+10.0";
  $au_timezone{"Northern Territory"} = "+9.0";
	$au_timezone{"Queensland"} = "+10.0";
	$au_timezone{"South Australia"} = "+9.0";
	$au_timezone{"Tasmania"} = "+10.0";
	$au_timezone{"Victoria"} = "+10.0";
	$au_timezone{"Western Australia"} = "+8.0";
}
