#!/usr/bin/env python3
# One-time export tool used to create citydb.tsv from the old binary citydb.sqlite.
#
# Provenance of citydb.tsv:
#   1. Run this script against the old citydb.sqlite to produce an initial TSV.
#   2. Manually fix 10 malformed lat/lon entries that existed in the original binary DB:
#      - Altenstadt (Germany): lat was decimal degrees (47.8339), converted to DMS  47 50' 2"
#      - Adak (USA):           lat missing leading space sign character
#      - Amami Island (Japan): lat missing leading space; lon had trailing space
#      - Amchitka (USA):       lon missing leading space sign character
#      - Bastia (France):      lat missing leading space sign character
#      - Bosscha (Indonesia):  lon missing leading space sign character
#      - Cape Canaveral (USA): lat missing leading space sign character
#      - Cape May (USA):       lat missing leading space sign character
#      - Sewerqia (Saudi Arabia): lat and lon missing leading space sign character
#      - Preston (United Kingdom): lat and lon had no spaces in DMS tokens and no sign;
#                                  lon corrected to negative (west longitude)
#   3. Elevation values were rounded to 2 decimal places to remove float32
#      artifacts from the original SQLite database (e.g. 1788.29004 -> 1788.29).
#   4. Three elevations were corrected from feet to meters (audited against GeoNames):
#      - Bryce Canyon (USA): 9100 -> 2774
#      - Hollywood, CA (USA): 276 -> 84
#      - Olympia, WA (USA): 119 -> 36
#
# The valid KStars DMS format is: leading ' ' (positive) or '-' (negative) followed by
# DD deg MM' SS". generate-citydb.py validates this at build time and skips any entry
# that does not conform.
import sqlite3
import argparse

def main():
    parser = argparse.ArgumentParser(description="Export citydb.sqlite to citydb.tsv")
    parser.add_argument("input", metavar="citydb.sqlite")
    parser.add_argument("output", metavar="citydb.tsv")
    args = parser.parse_args()

    conn = sqlite3.connect(args.input)
    cur = conn.cursor()
    cur.execute("SELECT Name, Province, Country, Latitude, Longitude, TZ, TZRule, Elevation FROM city ORDER BY Country, Name")
    rows = cur.fetchall()
    conn.close()

    with open(args.output, 'w', encoding='utf-8') as f:
        for row in rows:
            fields = []
            for i, val in enumerate(row):
                if val is None:
                    fields.append("")
                elif i == 7:  # Elevation: round to 2 decimals to remove float32 artifacts
                    # TODO: consider rounding to integer meters -- sub-meter
                    # precision is not meaningful for city locations, but the
                    # QDoubleSpinBox in locationdialog.ui currently shows 2 decimals.
                    fields.append(f"{val:.2f}")
                else:
                    fields.append(str(val))
            f.write("\t".join(fields) + "\n")

    print(f"Exported {len(rows)} cities to {args.output}")

if __name__ == "__main__":
    main()
