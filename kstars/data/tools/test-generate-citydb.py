#!/usr/bin/env python3
"""
Tests for generate-citydb.py.

Run with:  python3 kstars/data/tools/test-generate-citydb.py
"""
import os
import sys
import sqlite3
import subprocess
import tempfile
import unittest

SCRIPT = os.path.join(os.path.dirname(__file__), "generate-citydb.py")

# Minimal countryInfo.txt lines for test data.
# Format: ISO\tISO3\tISOnum\tfips\tName\t...  (only fields 0 and 4 matter)
COUNTRY_LINES = [
    "US\tUSA\t840\tUS\tUnited States\tWashington\t331002651\t9833520\tNA\t.us\tUSD\tDollar\t1\t#####-####\t^\\d{5}(-\\d{4})?$\ten-US,es-US\t6252001\t\t",
    "DE\tDEU\t276\tGM\tGermany\tBerlin\t83783942\t357114\tEU\t.de\tEUR\tEuro\t49\t#####\t^\\d{5}$\tde\t2921044\t\t",
]


def run_script(args):
    r = subprocess.run([sys.executable, SCRIPT] + args, capture_output=True, text=True)
    return r


def write_tsv(rows, f):
    for row in rows:
        f.write("\t".join(row) + "\n")
    f.flush()


def write_countries(tmpdir):
    """Write a minimal countryInfo.txt and return its path."""
    path = os.path.join(tmpdir, "countryInfo.txt")
    with open(path, "w", encoding="utf-8") as f:
        for line in COUNTRY_LINES:
            f.write(line + "\n")
    return path


# One representative input per detectable validation error class.
# Column 3 is now an ISO code, not a display name.
VALIDATION_CASES = [
    {
        "label": "malformed latlon (Altenstadt: lat as decimal degrees)",
        "error": ("Altenstadt", "", "DE", "47.8339", " 10\xb0 52' 04\"", "1.0", "EU", "739.00"),
        "valid": ("Altenstadt", "", "DE", " 47\xb0 50' 02\"", " 10\xb0 52' 04\"", "1.0", "EU", "739.00"),
        "name":  "'Altenstadt'",
    },
    {
        "label": "elevation out of range (Bryce Canyon: 9100 m exceeds 9000 m limit)",
        "error": ("Bryce Canyon National Park (Tropic) IDS", "Utah", "US", " 37\xb0 59' 30\"", "-112\xb0 19' 11\"", "-6.0", "US", "9100.00"),
        "valid": ("Bryce Canyon National Park (Tropic) IDS", "Utah", "US", " 37\xb0 59' 30\"", "-112\xb0 19' 11\"", "-6.0", "US", "2774.00"),
        "name":  "'Bryce Canyon National Park (Tropic) IDS'",
    },
    {
        "label": "non-numeric TZ (Adak: TZ field is not a number)",
        "error": ("Adak", "Alaska", "US", " 51\xb0 52' 46\"", "-176\xb0 43' 48\"", "notanumber", "US", "1.37"),
        "valid": ("Adak", "Alaska", "US", " 51\xb0 52' 46\"", "-176\xb0 43' 48\"", "-9.0",       "US", "1.37"),
        "name":  "'Adak'",
    },
    {
        "label": "non-numeric elevation (Bryce Canyon: elevation field is not a number)",
        "error": ("Bryce Canyon National Park (Tropic) IDS", "Utah", "US", " 37\xb0 59' 30\"", "-112\xb0 19' 11\"", "-6.0", "US", "notanumber"),
        "valid": ("Bryce Canyon National Park (Tropic) IDS", "Utah", "US", " 37\xb0 59' 30\"", "-112\xb0 19' 11\"", "-6.0", "US", "2774.00"),
        "name":  "'Bryce Canyon National Park (Tropic) IDS'",
    },
    {
        "label": "TZ out of range (Adak: TZ 99.0 outside [-12, 14])",
        "error": ("Adak", "Alaska", "US", " 51\xb0 52' 46\"", "-176\xb0 43' 48\"", "99.0", "US", "1.37"),
        "valid": ("Adak", "Alaska", "US", " 51\xb0 52' 46\"", "-176\xb0 43' 48\"", "-9.0", "US", "1.37"),
        "name":  "'Adak'",
    },
]


class TestValidationErrors(unittest.TestCase):

    def test_validation_errors(self):
        """Each detectable error class must warn, skip the bad row, and accept the corrected row."""
        for case in VALIDATION_CASES:
            with self.subTest(case["label"]):
                with tempfile.TemporaryDirectory() as tmpdir:
                    tsv = os.path.join(tmpdir, "cities.tsv")
                    db  = os.path.join(tmpdir, "out.sqlite")
                    countries = write_countries(tmpdir)
                    with open(tsv, "w", encoding="utf-8") as f:
                        write_tsv([case["error"], case["valid"]], f)
                    r = run_script(["--countries", countries, tsv, "--sqlite", db])
                    self.assertEqual(r.returncode, 0, r.stderr)
                    self.assertIn("cities.tsv:1:", r.stderr)
                    self.assertIn(case["name"], r.stderr)
                    count = sqlite3.connect(db).execute("SELECT COUNT(*) FROM city").fetchone()[0]
                    self.assertEqual(count, 1)

    def test_zero_rows_exits_nonzero(self):
        """An input with no valid rows must exit with a non-zero return code.
        Altenstadt (Germany): lat stored as decimal in the original binary db."""
        with tempfile.TemporaryDirectory() as tmpdir:
            tsv = os.path.join(tmpdir, "cities.tsv")
            db  = os.path.join(tmpdir, "out.sqlite")
            countries = write_countries(tmpdir)
            with open(tsv, "w", encoding="utf-8") as f:
                write_tsv([
                    ("Altenstadt", "", "DE", "47.8339", " 10\xb0 52' 04\"", "1.0", "EU", "739.00"),
                ], f)
            r = run_script(["--countries", countries, tsv, "--sqlite", db])
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("ERROR", r.stderr)
            self.assertIn("cities.tsv:1:", r.stderr)
            self.assertIn("'Altenstadt'", r.stderr)

    def test_unknown_iso_code_strict(self):
        """In strict mode, an unknown ISO code must cause a non-zero exit."""
        with tempfile.TemporaryDirectory() as tmpdir:
            tsv = os.path.join(tmpdir, "cities.tsv")
            db  = os.path.join(tmpdir, "out.sqlite")
            countries = write_countries(tmpdir)
            with open(tsv, "w", encoding="utf-8") as f:
                write_tsv([
                    ("Testville", "", "ZZ", " 10\xb0 00' 00\"", " 20\xb0 00' 00\"", "1.0", "--", "100.0"),
                ], f)
            r = run_script(["--countries", countries, "--strict", tsv, "--sqlite", db])
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("unknown ISO country code", r.stderr)

    def test_unknown_iso_code_nonstrict(self):
        """Without strict mode, an unknown ISO code warns but succeeds."""
        with tempfile.TemporaryDirectory() as tmpdir:
            tsv = os.path.join(tmpdir, "cities.tsv")
            db  = os.path.join(tmpdir, "out.sqlite")
            countries = write_countries(tmpdir)
            with open(tsv, "w", encoding="utf-8") as f:
                write_tsv([
                    ("Testville", "", "ZZ", " 10\xb0 00' 00\"", " 20\xb0 00' 00\"", "1.0", "--", "100.0"),
                ], f)
            r = run_script(["--countries", countries, tsv, "--sqlite", db])
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn("unknown ISO country code", r.stderr)
            row = sqlite3.connect(db).execute("SELECT Country FROM city").fetchone()
            self.assertEqual(row[0], "ZZ")


class TestISOCodeResolution(unittest.TestCase):

    def test_iso_code_resolved_to_display_name(self):
        """ISO codes in TSV must be resolved to display names in sqlite output."""
        with tempfile.TemporaryDirectory() as tmpdir:
            tsv = os.path.join(tmpdir, "cities.tsv")
            db  = os.path.join(tmpdir, "out.sqlite")
            countries = write_countries(tmpdir)
            with open(tsv, "w", encoding="utf-8") as f:
                write_tsv([
                    ("Adak", "Alaska", "US", " 51\xb0 52' 46\"", "-176\xb0 43' 48\"", "-9.0", "US", "1.37"),
                    ("Berlin", "", "DE", " 52\xb0 31' 00\"", " 13\xb0 24' 00\"", "1.0", "EU", "34.0"),
                ], f)
            r = run_script(["--countries", countries, tsv, "--sqlite", db])
            self.assertEqual(r.returncode, 0, r.stderr)
            conn = sqlite3.connect(db)
            rows = conn.execute(
                "SELECT Name, Country FROM city ORDER BY Name"
            ).fetchall()
            conn.close()
            self.assertEqual(rows[0], ("Adak", "USA"))
            self.assertEqual(rows[1], ("Berlin", "Germany"))

    def test_display_name_override_applied(self):
        """DISPLAY_NAME_OVERRIDES must be applied (US->USA, not 'United States')."""
        with tempfile.TemporaryDirectory() as tmpdir:
            tsv = os.path.join(tmpdir, "cities.tsv")
            db  = os.path.join(tmpdir, "out.sqlite")
            countries = write_countries(tmpdir)
            with open(tsv, "w", encoding="utf-8") as f:
                write_tsv([
                    ("Adak", "Alaska", "US", " 51\xb0 52' 46\"", "-176\xb0 43' 48\"", "-9.0", "US", "1.37"),
                ], f)
            r = run_script(["--countries", countries, tsv, "--sqlite", db])
            self.assertEqual(r.returncode, 0, r.stderr)
            row = sqlite3.connect(db).execute("SELECT Country FROM city").fetchone()
            self.assertEqual(row[0], "USA")


class TestSQLOutput(unittest.TestCase):

    def test_sql_output_apostrophe_escaping(self):
        """City names with apostrophes must be properly escaped in SQL output."""
        with tempfile.TemporaryDirectory() as tmpdir:
            tsv = os.path.join(tmpdir, "cities.tsv")
            sql = os.path.join(tmpdir, "out.sql")
            db  = os.path.join(tmpdir, "out.sqlite")
            countries = write_countries(tmpdir)
            with open(tsv, "w", encoding="utf-8") as f:
                write_tsv([
                    ("Coeur d'Alene", "Idaho", "US", " 47\xb0 40' 37\"", "-116\xb0 46' 45\"", "-7.0", "US", "665.0"),
                ], f)
            r = run_script(["--countries", countries, tsv, "-o", sql])
            self.assertEqual(r.returncode, 0, r.stderr)
            conn = sqlite3.connect(db)
            with open(sql, encoding="utf-8") as f:
                conn.executescript(f.read())
            row = conn.execute("SELECT Name FROM city").fetchone()
            conn.close()
            self.assertIsNotNone(row)
            self.assertEqual(row[0], "Coeur d'Alene")

    def test_sql_output_row_count_matches_sqlite(self):
        """SQL output and --sqlite output must produce the same row count."""
        with tempfile.TemporaryDirectory() as tmpdir:
            tsv = os.path.join(tmpdir, "cities.tsv")
            sql = os.path.join(tmpdir, "out.sql")
            db_sql    = os.path.join(tmpdir, "from_sql.sqlite")
            db_direct = os.path.join(tmpdir, "direct.sqlite")
            countries = write_countries(tmpdir)
            with open(tsv, "w", encoding="utf-8") as f:
                write_tsv([
                    ("Adak", "Alaska", "US", " 51\xb0 52' 46\"", "-176\xb0 43' 48\"", "-9.0", "US", "1.37"),
                    ("Coeur d'Alene", "Idaho", "US", " 47\xb0 40' 37\"", "-116\xb0 46' 45\"", "-7.0", "US", "665.0"),
                ], f)
            run_script(["--countries", countries, tsv, "-o", sql])
            conn = sqlite3.connect(db_sql)
            with open(sql, encoding="utf-8") as f:
                conn.executescript(f.read())
            count_sql = conn.execute("SELECT COUNT(*) FROM city").fetchone()[0]
            conn.close()

            run_script(["--countries", countries, tsv, "--sqlite", db_direct])
            count_direct = sqlite3.connect(db_direct).execute("SELECT COUNT(*) FROM city").fetchone()[0]

            self.assertEqual(count_sql, count_direct)


class TestProvinceDedup(unittest.TestCase):

    def test_same_name_different_province_both_kept(self):
        """Two cities with same name and country but different province must both be inserted."""
        with tempfile.TemporaryDirectory() as tmpdir:
            tsv = os.path.join(tmpdir, "cities.tsv")
            db  = os.path.join(tmpdir, "out.sqlite")
            countries = write_countries(tmpdir)
            with open(tsv, "w", encoding="utf-8") as f:
                write_tsv([
                    ("Springfield", "Illinois", "US", " 39\xb0 47' 58\"", "-89\xb0 39' 00\"", "-6.0", "US", "168.0"),
                    ("Springfield", "Missouri", "US", " 37\xb0 13' 00\"", "-93\xb0 18' 00\"", "-6.0", "US", "417.0"),
                ], f)
            run_script(["--countries", countries, tsv, "--sqlite", db])
            rows = sqlite3.connect(db).execute(
                "SELECT Province FROM city WHERE Name='Springfield' ORDER BY Province"
            ).fetchall()
            self.assertEqual([r[0] for r in rows], ["Illinois", "Missouri"])

    def test_true_duplicate_dropped(self):
        """Exact duplicate (same name+province+country) must be inserted only once."""
        with tempfile.TemporaryDirectory() as tmpdir:
            tsv = os.path.join(tmpdir, "cities.tsv")
            db  = os.path.join(tmpdir, "out.sqlite")
            countries = write_countries(tmpdir)
            entry = ("Springfield", "Illinois", "US", " 39\xb0 47' 58\"", "-89\xb0 39' 00\"", "-6.0", "US", "168.0")
            with open(tsv, "w", encoding="utf-8") as f:
                write_tsv([entry, entry], f)
            run_script(["--countries", countries, tsv, "--sqlite", db])
            count = sqlite3.connect(db).execute("SELECT COUNT(*) FROM city").fetchone()[0]
            self.assertEqual(count, 1)


class TestWithoutCountries(unittest.TestCase):

    def test_no_countries_file_passes_code_through(self):
        """Without --countries, column 3 is used as-is (backwards compat for manual use)."""
        with tempfile.TemporaryDirectory() as tmpdir:
            tsv = os.path.join(tmpdir, "cities.tsv")
            db  = os.path.join(tmpdir, "out.sqlite")
            with open(tsv, "w", encoding="utf-8") as f:
                write_tsv([
                    ("Adak", "Alaska", "US", " 51\xb0 52' 46\"", "-176\xb0 43' 48\"", "-9.0", "US", "1.37"),
                ], f)
            r = run_script([tsv, "--sqlite", db])
            self.assertEqual(r.returncode, 0, r.stderr)
            row = sqlite3.connect(db).execute("SELECT Country FROM city").fetchone()
            self.assertEqual(row[0], "US")


if __name__ == "__main__":
    unittest.main(verbosity=2)
