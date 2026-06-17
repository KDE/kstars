#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys

# Determine the TSV file path relative to this script
script_dir = os.path.dirname(os.path.abspath(__file__))
tsv_path = os.path.join(script_dir, "..", "citydb.tsv")

if not os.path.exists(tsv_path):
    # Fallback to the working directory path if run differently
    tsv_path = "data/citydb.tsv"

if not os.path.exists(tsv_path):
    print(f"Error: Could not find citydb.tsv at {tsv_path}", file=sys.stderr)
    sys.exit(1)

seen_cities = set()
cities = []
regions = set()
countries = set()

# Process the TSV file
with open(tsv_path, "r", encoding="utf-8") as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) not in (8, 9):
            continue

        name = fields[0]
        province = fields[1]
        country = fields[2]

        city_key = (name.lower(), province.lower(), country.lower())
        if city_key in seen_cities:
            continue
        seen_cities.add(city_key)

        cities.append((name, province, country))

        if province:
            regions.add((province, country))
        if country:
            countries.add(country)

# Sort and print translation key declarations (xi18nc) for:
# - Cities: xi18nc("City in [Region] Country", "CityName")
# - Regions: xi18nc("Region/state in Country", "RegionName")
# - Countries: xi18nc("Country name", "CountryName")
for name, province, country in sorted(cities):
    if province:
        place = "{0} {1}".format(province, country)
    else:
        place = country
    print('xi18nc("City in {0}", "{1}")'.format(place, name))

for province, country in sorted(regions):
    print('xi18nc("Region/state in {0}", "{1}")'.format(country, province))

for country in sorted(countries):
    print('xi18nc("Country name", "{0}")'.format(country))
