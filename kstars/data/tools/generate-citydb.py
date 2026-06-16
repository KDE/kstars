#!/usr/bin/env python3
import sys
import os
import re
import argparse
import sqlite3

# Valid KStars DMS format: leading ' ' (positive) or '-' (negative),
# then digits, degree symbol, space, minutes, quote, seconds, double-quote.
# Example: " 37° 20' 26\""  or "-22° 54' 30\""
_DMS_RE = re.compile(r'^[ -]\d+\xb0 \d+\' \d+"$')

def _warn(msg):
    print("WARNING: " + msg, file=sys.stderr)

def main():
    parser = argparse.ArgumentParser(description="Generate city database from TSV files")
    parser.add_argument("cities_files", nargs="+", metavar="cities.tsv")
    parser.add_argument("-o", metavar="output.sql", dest="output_file", default=None)
    parser.add_argument("--sqlite", metavar="output.sqlite", dest="sqlite_file", default=None)
    args = parser.parse_args()

    if not args.output_file and not args.sqlite_file:
        parser.error("at least one of -o or --sqlite is required")

    DDL = [
        "DROP TABLE IF EXISTS city",
        "CREATE TABLE city ( id INTEGER DEFAULT NULL PRIMARY KEY AUTOINCREMENT, Name TEXT DEFAULT NULL, Province TEXT DEFAULT NULL, Country TEXT DEFAULT NULL, Latitude TEXT DEFAULT NULL, Longitude TEXT DEFAULT NULL, TZ REAL DEFAULT NULL, TZRule TEXT DEFAULT NULL, Elevation REAL NOT NULL DEFAULT -10 )",
        "CREATE INDEX IF NOT EXISTS idx_name_country ON city (Name, Country)",
    ]
    INSERT = "INSERT INTO city (Name, Province, Country, Latitude, Longitude, TZ, TZRule, Elevation) VALUES"

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
                            _warn(f"{cities_file}:{lineno}: skipping row with unexpected field count {len(fields)}")
                        continue
                    name, province, country, lat_s, lon_s, tz_s, rule, elev_s = fields
                    if not _DMS_RE.match(lat_s) or not _DMS_RE.match(lon_s):
                        _warn(f"{cities_file}:{lineno}: skipping {name!r} ({country}): malformed lat/lon lat={lat_s!r} lon={lon_s!r}")
                        continue
                    try:
                        tz_val = float(tz_s)
                        elev_val = float(elev_s) if elev_s else -10.0
                    except ValueError as e:
                        _warn(f"{cities_file}:{lineno}: skipping {name!r} ({country}): {e}")
                        continue
                    if not (-12 <= tz_val <= 14):
                        _warn(f"{cities_file}:{lineno}: skipping {name!r} ({country}): TZ {tz_val} out of range [-12, 14]")
                        continue
                    if not (-500 <= elev_val <= 9000):
                        _warn(f"{cities_file}:{lineno}: skipping {name!r} ({country}): elevation {elev_val} out of range [-500, 9000]")
                        continue
                    city_key = (name.lower(), province.lower(), country.lower())
                    if city_key in seen_cities:
                        continue
                    seen_cities.add(city_key)
                    row = (name, province, country, lat_s, lon_s, tz_val, rule, elev_val)

                    if sql_out:
                        nm, pv, ct, la, lo, tz_v, ru, el = row
                        nm = nm.replace("'", "''")
                        pv = pv.replace("'", "''")
                        ct = ct.replace("'", "''")
                        la = la.replace("'", "''")
                        lo = lo.replace("'", "''")
                        sql_out.write(f"{INSERT} ('{nm}', '{pv}', '{ct}', '{la}', '{lo}', {tz_v}, '{ru}', {el});\n")
                    if cur:
                        cur.execute(f"{INSERT} (?,?,?,?,?,?,?,?)", row)

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
        print("ERROR: no valid rows were produced -- check input files and warnings above", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
