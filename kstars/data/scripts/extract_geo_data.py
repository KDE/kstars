#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys

# Determine paths relative to this script
script_dir = os.path.dirname(os.path.abspath(__file__))
data_dir = os.path.join(script_dir, "..")
tsv_path = os.path.join(data_dir, "citydb.tsv")
countries_path = os.path.join(data_dir, "countryInfo.txt")

if not os.path.exists(tsv_path):
    tsv_path = "data/citydb.tsv"
if not os.path.exists(tsv_path):
    print("Error: Could not find citydb.tsv at {0}".format(tsv_path), file=sys.stderr)
    sys.exit(1)

# GeoNames canonical country names that KStars displays differently.
DISPLAY_NAME_OVERRIDES = {
    "United States": "USA",
    "Czech Republic": "Czechia",
    "Turkey": "Türkiye",
}


def load_countries(path):
    """Load countryInfo.txt -> dict mapping ISO code to display name."""
    countries = {}
    if not os.path.exists(path):
        return countries
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            if line.startswith("#"):
                continue
            fields = line.strip().split("\t")
            if len(fields) >= 5:
                iso = fields[0]
                canonical = fields[4]
                countries[iso] = DISPLAY_NAME_OVERRIDES.get(canonical, canonical)
    return countries


country_map = load_countries(countries_path)

seen_cities = set()
cities = []
regions = set()
countries = set()

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
        country_code = fields[2]
        country = country_map.get(country_code, country_code)

        city_key = (name.lower(), province.lower(), country_code.lower())
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
