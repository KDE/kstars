#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Christian Kemper <ckemper@gmail.com>
# SPDX-License-Identifier: GPL-2.0-or-later
"""Generate citydb.sqlite from KStars TSV city files.

Column 3 of the input TSV is an ISO 3166-1 alpha-2 country code. The
generator resolves it to a display name using countryInfo.txt (with three
display-name overrides: US->USA, CZ->Czechia, TR->Türkiye) before writing the sqlite
output. This keeps the source format compact and unambiguous while the
C++ code continues to see human-readable country names.

Usage:
    python3 generate-citydb.py --countries countryInfo.txt --strict \
        --sqlite citydb.sqlite citydb.tsv
"""
import sys
import os
import re
import argparse
import sqlite3

_DMS_RE = re.compile(r'^[ -]?\d+\xb0 \d+\' \d+"$')

# GeoNames canonical country names that KStars displays differently.
DISPLAY_NAME_OVERRIDES = {
    "United States": "USA",
    "Czech Republic": "Czechia",
    "Turkey": "Türkiye",
}

_errors = 0
_strict = False


def _warn(msg):
    global _errors
    if _strict:
        _errors += 1
        print("ERROR: " + msg, file=sys.stderr)
    else:
        print("WARNING: " + msg, file=sys.stderr)


def load_countries(countries_file):
    """Load countryInfo.txt -> dict mapping ISO code to display name."""
    countries = {}
    with open(countries_file, 'r', encoding='utf-8') as f:
        for line in f:
            if line.startswith('#'):
                continue
            fields = line.strip().split('\t')
            if len(fields) >= 5:
                iso = fields[0]
                canonical = fields[4]
                countries[iso] = DISPLAY_NAME_OVERRIDES.get(canonical, canonical)
    return countries


def main():
    global _strict

    parser = argparse.ArgumentParser(description="Generate city database from TSV files")
    parser.add_argument("cities_files", nargs="+", metavar="cities.tsv")
    parser.add_argument("-o", metavar="output.sql", dest="output_file", default=None)
    parser.add_argument("--sqlite", metavar="output.sqlite", dest="sqlite_file", default=None)
    parser.add_argument("--countries", metavar="countryInfo.txt", dest="countries_file",
                        default=None, help="GeoNames countryInfo.txt for ISO code resolution")
    parser.add_argument("--strict", action="store_true",
                        help="treat validation warnings as errors (non-zero exit)")
    args = parser.parse_args()

    _strict = args.strict

    if not args.output_file and not args.sqlite_file:
        parser.error("at least one of -o or --sqlite is required")

    countries = {}
    if args.countries_file:
        countries = load_countries(args.countries_file)

    DDL = [
        "DROP TABLE IF EXISTS city",
        "CREATE TABLE city ("
        " id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT,"
        " Name TEXT DEFAULT NULL,"
        " Province TEXT DEFAULT NULL,"
        " Country TEXT DEFAULT NULL,"
        " Latitude TEXT DEFAULT NULL,"
        " Longitude TEXT DEFAULT NULL,"
        " TZ REAL DEFAULT NULL,"
        " TZRule TEXT DEFAULT NULL,"
        " Elevation REAL NOT NULL DEFAULT -10"
        " )",
        "CREATE INDEX IF NOT EXISTS idx_name_country ON city (Name, Country)",
    ]
    INSERT = ("INSERT INTO city"
              " (Name, Province, Country, Latitude, Longitude, TZ, TZRule, Elevation)"
              " VALUES")

    sql_out = None
    conn = None
    cur = None
    try:
        if args.output_file:
            sql_out = open(args.output_file, 'w', encoding='utf-8')
            sql_out.write("BEGIN TRANSACTION;\n")
            for stmt in DDL:
                sql_out.write(stmt + ";\n")

        if args.sqlite_file:
            if os.path.exists(args.sqlite_file):
                os.remove(args.sqlite_file)
            conn = sqlite3.connect(args.sqlite_file)
            cur = conn.cursor()
            for stmt in DDL:
                cur.execute(stmt)
            conn.commit()
            cur.execute("BEGIN")

        seen_cities = set()
        count = 0

        for cities_file in args.cities_files:
            with open(cities_file, 'r', encoding='utf-8') as f:
                for lineno, line in enumerate(f, 1):
                    fields = line.strip().split('\t')
                    if len(fields) != 8:
                        if len(fields) > 1:
                            _warn("{0}:{1}: skipping row with unexpected field"
                                  " count {2}".format(cities_file, lineno, len(fields)))
                        continue
                    name, province, country_code, lat_s, lon_s, tz_s, rule, elev_s = fields

                    if countries:
                        if country_code not in countries:
                            _warn("{0}:{1}: {2}: unknown ISO country code"
                                  " {3!r}".format(cities_file, lineno, name, country_code))
                        display_country = countries.get(country_code, country_code)
                    else:
                        display_country = country_code

                    if not _DMS_RE.match(lat_s) or not _DMS_RE.match(lon_s):
                        _warn("{0}:{1}: skipping {2!r} ({3}): malformed lat/lon"
                              " lat={4!r} lon={5!r}".format(
                                  cities_file, lineno, name, country_code, lat_s, lon_s))
                        continue
                    try:
                        tz_val = float(tz_s)
                        elev_val = float(elev_s) if elev_s else -10.0
                    except ValueError as e:
                        _warn("{0}:{1}: skipping {2!r} ({3}): {4}".format(
                            cities_file, lineno, name, country_code, e))
                        continue
                    if not (-12 <= tz_val <= 14):
                        _warn("{0}:{1}: skipping {2!r} ({3}): TZ {4} out of"
                              " range [-12, 14]".format(
                                  cities_file, lineno, name, country_code, tz_val))
                        continue
                    if not (-500 <= elev_val <= 9000):
                        _warn("{0}:{1}: skipping {2!r} ({3}): elevation {4} out"
                              " of range [-500, 9000]".format(
                                  cities_file, lineno, name, country_code, elev_val))
                        continue

                    city_key = (name.lower(), province.lower(), country_code.lower())
                    if city_key in seen_cities:
                        continue
                    seen_cities.add(city_key)
                    row = (name, province, display_country, lat_s, lon_s, tz_val, rule, elev_val)

                    if sql_out:
                        nm, pv, ct, la, lo, tz_v, ru, el = row
                        nm = nm.replace("'", "''")
                        pv = pv.replace("'", "''")
                        ct = ct.replace("'", "''")
                        la = la.replace("'", "''")
                        lo = lo.replace("'", "''")
                        sql_out.write("{0} ('{1}', '{2}', '{3}', '{4}', '{5}',"
                                      " {6}, '{7}', {8});\n".format(
                                          INSERT, nm, pv, ct, la, lo, tz_v, ru, el))
                    if cur:
                        cur.execute("{0} (?,?,?,?,?,?,?,?)".format(INSERT), row)

                    count += 1
                    if count % 1000 == 0:
                        if sql_out:
                            sql_out.write("COMMIT; BEGIN TRANSACTION;\n")
                        if conn:
                            conn.commit()
                            cur.execute("BEGIN")

        if sql_out:
            sql_out.write("COMMIT;\n")
        if conn:
            conn.commit()
    finally:
        if sql_out:
            sql_out.close()
        if conn:
            conn.close()

    if count == 0:
        print("ERROR: no valid rows were produced -- check input files"
              " and warnings above", file=sys.stderr)
        sys.exit(1)

    if _errors > 0:
        print("ERROR: {0} validation error(s) in strict mode".format(_errors),
              file=sys.stderr)
        sys.exit(1)

    print("Generated {0} cities".format(count), file=sys.stderr)


if __name__ == "__main__":
    main()
