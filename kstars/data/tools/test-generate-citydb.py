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


def run_script(args):
    r = subprocess.run([sys.executable, SCRIPT] + args, capture_output=True, text=True)
    return r


def write_tsv(rows, f):
    for row in rows:
        f.write("\t".join(row) + "\n")
    f.flush()


# One representative input per detectable validation error class.
# error: the invalid row; valid: the corrected row that must be accepted.
VALIDATION_CASES = [
    {
        "label": "malformed latlon (Adak: lat missing leading space)",
        "error": ("Adak", "Alaska", "USA", "51\xb0 52' 46\"",  "-176\xb0 43' 48\"", "-9.0", "US", "1.37"),
        "valid": ("Adak", "Alaska", "USA", " 51\xb0 52' 46\"", "-176\xb0 43' 48\"", "-9.0", "US", "1.37"),
        "name":  "'Adak'",
    },
    {
        "label": "elevation out of range (Bryce Canyon: 9100 m exceeds 9000 m limit)",
        "error": ("Bryce Canyon National Park (Tropic) IDS", "Utah", "USA", " 37\xb0 59' 30\"", "-112\xb0 19' 11\"", "-6.0", "US", "9100.00"),
        "valid": ("Bryce Canyon National Park (Tropic) IDS", "Utah", "USA", " 37\xb0 59' 30\"", "-112\xb0 19' 11\"", "-6.0", "US", "2774.00"),
        "name":  "'Bryce Canyon National Park (Tropic) IDS'",
    },
    {
        "label": "non-numeric TZ (Adak: TZ field is not a number)",
        "error": ("Adak", "Alaska", "USA", " 51\xb0 52' 46\"", "-176\xb0 43' 48\"", "notanumber", "US", "1.37"),
        "valid": ("Adak", "Alaska", "USA", " 51\xb0 52' 46\"", "-176\xb0 43' 48\"", "-9.0",       "US", "1.37"),
        "name":  "'Adak'",
    },
    {
        "label": "non-numeric elevation (Bryce Canyon: elevation field is not a number)",
        "error": ("Bryce Canyon National Park (Tropic) IDS", "Utah", "USA", " 37\xb0 59' 30\"", "-112\xb0 19' 11\"", "-6.0", "US", "notanumber"),
        "valid": ("Bryce Canyon National Park (Tropic) IDS", "Utah", "USA", " 37\xb0 59' 30\"", "-112\xb0 19' 11\"", "-6.0", "US", "2774.00"),
        "name":  "'Bryce Canyon National Park (Tropic) IDS'",
    },
    {
        "label": "TZ out of range (Adak: TZ 99.0 outside [-12, 14])",
        "error": ("Adak", "Alaska", "USA", " 51\xb0 52' 46\"", "-176\xb0 43' 48\"", "99.0", "US", "1.37"),
        "valid": ("Adak", "Alaska", "USA", " 51\xb0 52' 46\"", "-176\xb0 43' 48\"", "-9.0", "US", "1.37"),
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
                    with open(tsv, "w", encoding="utf-8") as f:
                        write_tsv([case["error"], case["valid"]], f)
                    r = run_script([tsv, "--sqlite", db])
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
            with open(tsv, "w", encoding="utf-8") as f:
                write_tsv([
                    ("Altenstadt", "", "Germany", "47.8339", " 10\xb0 52' 04\"", "1.0", "EU", "739.00"),
                ], f)
            r = run_script([tsv, "--sqlite", db])
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("ERROR", r.stderr)
            self.assertIn("cities.tsv:1:", r.stderr)
            self.assertIn("'Altenstadt'", r.stderr)


class TestSQLOutput(unittest.TestCase):

    def test_sql_output_apostrophe_escaping(self):
        """City names with apostrophes must be properly escaped in SQL output."""
        with tempfile.TemporaryDirectory() as tmpdir:
            tsv = os.path.join(tmpdir, "cities.tsv")
            sql = os.path.join(tmpdir, "out.sql")
            db  = os.path.join(tmpdir, "out.sqlite")
            with open(tsv, "w", encoding="utf-8") as f:
                write_tsv([
                    ("Coeur d'Alene", "Idaho", "USA", " 47\xb0 40' 37\"", "-116\xb0 46' 45\"", "-7.0", "US", "665.0"),
                ], f)
            r = run_script([tsv, "-o", sql])
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
            with open(tsv, "w", encoding="utf-8") as f:
                write_tsv([
                    ("Adak", "Alaska", "USA", " 51\xb0 52' 46\"", "-176\xb0 43' 48\"", "-9.0", "US", "1.37"),
                    ("Coeur d'Alene", "Idaho", "USA", " 47\xb0 40' 37\"", "-116\xb0 46' 45\"", "-7.0", "US", "665.0"),
                ], f)
            run_script([tsv, "-o", sql])
            conn = sqlite3.connect(db_sql)
            with open(sql, encoding="utf-8") as f:
                conn.executescript(f.read())
            count_sql = conn.execute("SELECT COUNT(*) FROM city").fetchone()[0]
            conn.close()

            run_script([tsv, "--sqlite", db_direct])
            count_direct = sqlite3.connect(db_direct).execute("SELECT COUNT(*) FROM city").fetchone()[0]

            self.assertEqual(count_sql, count_direct)


class TestProvinceDedup(unittest.TestCase):

    def test_same_name_different_province_both_kept(self):
        """Two cities with same name and country but different province must both be inserted."""
        with tempfile.TemporaryDirectory() as tmpdir:
            tsv = os.path.join(tmpdir, "cities.tsv")
            db  = os.path.join(tmpdir, "out.sqlite")
            with open(tsv, "w", encoding="utf-8") as f:
                write_tsv([
                    ("Springfield", "Illinois", "USA", " 39\xb0 47' 58\"", "-89\xb0 39' 00\"", "-6.0", "US", "168.0"),
                    ("Springfield", "Missouri", "USA", " 37\xb0 13' 00\"", "-93\xb0 18' 00\"", "-6.0", "US", "417.0"),
                ], f)
            run_script([tsv, "--sqlite", db])
            rows = sqlite3.connect(db).execute(
                "SELECT Province FROM city WHERE Name='Springfield' ORDER BY Province"
            ).fetchall()
            self.assertEqual([r[0] for r in rows], ["Illinois", "Missouri"])

    def test_true_duplicate_dropped(self):
        """Exact duplicate (same name+province+country) must be inserted only once."""
        with tempfile.TemporaryDirectory() as tmpdir:
            tsv = os.path.join(tmpdir, "cities.tsv")
            db  = os.path.join(tmpdir, "out.sqlite")
            entry = ("Springfield", "Illinois", "USA", " 39\xb0 47' 58\"", "-89\xb0 39' 00\"", "-6.0", "US", "168.0")
            with open(tsv, "w", encoding="utf-8") as f:
                write_tsv([entry, entry], f)
            run_script([tsv, "--sqlite", db])
            count = sqlite3.connect(db).execute("SELECT COUNT(*) FROM city").fetchone()[0]
            self.assertEqual(count, 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
